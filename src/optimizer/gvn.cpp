#include "optimizer/pass.hpp"

namespace GVN {

bool pinned(RegWriteInstr *i) {
  Case(LocalVarDef, i0, i) return 1;

  Case(PhiInstr, i0, i) return 1;
  Case(CallInstr, i0, i) return 1;
  Case(LoadInstr, i0, i) return 1;

  Case(MemDef, i0, i) return 1;
  Case(MemUse, i0, i) return 1;
  Case(MemEffect, i0, i) return 1;
  Case(MemRead, i0, i) {
    // return 0; //warning: may cause error!
    return !i0->data->is_single_var();
  }
  Case(MemWrite, i0, i) return 1;
  return 0;
};

void gvn(NormalFunc *f) {
  dbg << "## gvn\n";

  auto def = build_defs(f);
  std::unordered_map<MemObject *, int> vn_addr;
  std::unordered_map<int, int> vn_arg, vn_c, rvn_c;
  std::unordered_map<std::pair<int, int>, int> vn_uop, vn_mem_read;
  std::unordered_map<std::tuple<int, int, int>, int> vn_op, vn_ai;
  std::unordered_map<Reg, int> vn;
  std::unordered_map<int, Reg> rvn;
  std::unordered_map<int, std::pair<Reg, int>> radd_c;
  std::map<std::pair<Func *, std::vector<int>>, int> vn_call;
  int id = 0;
  auto get_vn_c = [&](int c) {
    int &v_c = vn_c[c];
    if (!v_c) {
      rvn_c[v_c = ++id] = c;
      Reg r = rvn[v_c] = f->new_Reg();
      f->entry->push_front(new LoadConst(r, c));
    }
    return v_c;
  };
  auto get_id = [&](int &x) {
    if (!x) x = ++id;
    return x;
  };
  auto get_vn_uop = [&](int s1, UnaryOp::Type type) {
    return get_id(vn_uop[std::make_pair(s1, (int)type)]);
  };
  auto get_vn_op = [&](int s1, int s2, BinaryOp::Type type) {
    return get_id(vn_op[std::make_tuple(s1, s2, (int)type)]);
  };
  auto get_vn = [&](RegWriteInstr *x0) {
    Case(LoadAddr, x1, x0) { return get_id(vn_addr[x1->offset]); }
    Case(LoadConst, x1, x0) { return get_vn_c(x1->value); }
    Case(UnaryOpInstr, x1, x0) {
      int v1 = vn.at(x1->s1);
      auto it1 = rvn_c.find(v1);
      if (it1 != rvn_c.end()) {
        // op c1
        return get_vn_c(x1->compute(it1->second));
      }
      if (x1->op.type == UnaryOp::ID) return v1;  // copy
      return get_vn_uop(v1, x1->op.type);
    }
    Case(BinaryOpInstr, x1, x0) {
      int v1 = vn.at(x1->s1);
      if (!vn.count(x1->s2)) std::cerr << f->get_name(x1->s2) << " not found\n";
      int v2 = vn.at(x1->s2);
      if (rvn_c.count(v1) && (x1->op.comm())) {
        std::swap(v1, v2);
        std::swap(x1->s1, x1->s2);
      }
      auto it1 = rvn_c.find(v1);
      auto it2 = rvn_c.find(v2);
      bool isc1 = 0, isc2 = 0;
      int c1 = 0, c2 = 0;
      if (it1 != rvn_c.end()) c1 = it1->second, isc1 = 1;
      if (it2 != rvn_c.end()) c2 = it2->second, isc2 = 1;

      if (isc1 && isc2) {
        int res = x1->compute(c1, c2);
        // c1 op c2
        return get_vn_c(res);
      }

      if (isc2) switch (x1->op.type) {
          case BinaryOp::SUB:
            c2 = -c2;
            v2 = get_vn_c(c2);
            x1->op = BinaryOp::ADD;
            x1->s2 = rvn.at(v2);
            // (s1 - c2) = (s1 + (-c2))
          case BinaryOp::ADD: {
            auto it = radd_c.find(v1);
            if (it != radd_c.end()) {
              x1->s1 = it->second.first;
              v1 = vn.at(x1->s1);
              c2 += it->second.second;
              v2 = get_vn_c(c2);
              x1->s2 = rvn.at(v2);
              // ((s1' + c2') + c2) = (s1' + (c2' + c2))
            }
            if (c2 == 0) return v1;  // s1 + 0
            int ret = get_vn_op(v1, v2, x1->op.type);
            radd_c[ret] = std::make_pair(x1->s1, c2);
            return ret;
          }
          case BinaryOp::MUL:
            if (c2 == 0) return v2;  // s1 * 0
            if (c2 == 1) return v1;  // s1 * 1
            break;
          case BinaryOp::DIV:
            if (c2 == 1) return v1;  // s1 / 1
            break;
          default:
            break;
        }
      return get_vn_op(v1, v2, x1->op.type);
    }
    Case(LoadArg, x1, x0) { return get_id(vn_arg[x1->id]); }
    Case(MemRead, x1, x0) {
      int v1 = vn.at(x1->mem);
      int v2 = vn.at(x1->addr);
      return get_id(vn_mem_read[std::make_pair(v1, v2)]);
    }
    Case(ArrayIndex, x1, x0) {
      int v1 = vn.at(x1->s1);
      int v2 = vn.at(x1->s2);
      return get_id(vn_ai[std::make_tuple(v1, v2, x1->size)]);
    }
    Case(LoadInstr, x1, x0) { return ++id; }
    Case(CallInstr, x1, x0) {
      if (x1->pure) {
        std::vector<int> uses;
        for (MemUse *use : x1->in) {
          uses.push_back(vn.at(use->s1));
        }
        for (Reg r : x1->args) uses.push_back(vn.at(r));
        return get_id(vn_call[std::make_pair(x1->f, uses)]);
      }
      return ++id;
    }
    Case(PhiInstr, x1, x0) { return ++id; }
    return ++id;
  };

  std::unordered_map<Reg, Reg> mp_reg;
  auto S = build_dom_tree(f);
  std::function<void(BB *)> dfs;

  dfs = [&](BB *w) {
    std::vector<int> new_hash;
    for (auto it = w->instrs.begin(); it != w->instrs.end(); ++it) {
      Instr *i = it->get();
      Case(RegWriteInstr, i0, i) {
        int v = vn[i0->d1] = get_vn(i0);
        if (rvn.count(v)) {
          mp_reg[i0->d1] = rvn[v];
        } else {
          rvn[v] = i0->d1;
          new_hash.push_back(v);
          if (rvn_c.count(v)) {
            *it = std::unique_ptr<Instr>(new LoadConst(i0->d1, rvn_c[v]));
          }
        }
      }
    }
    for (BB *u : S[w].dom_ch) {
      dfs(u);
    }
    for (int v : new_hash) {
      rvn.erase(rvn.find(v));
    }
  };

  dfs(f->entry);

  f->for_each([&](BB *bb) {
    std::map<std::vector<std::pair<Reg, BB *>>, Reg> mp_phi;
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *x = it->get();
      Case(PhiInstr, x0, x) {
        Reg r0;
        int cnt = 0, flag = 1;
        x0->map_use([&](Reg &r) {
          if (!cnt++)
            r0 = r;
          else
            flag &= (r0 == r);
        });
        if (flag && cnt >= 1) {
          mp_reg[x0->d1] = r0;
        } else {
          Reg &r = mp_phi[x0->uses];
          if (r.id)
            mp_reg[x0->d1] = r;
          else
            r = x0->d1;
        }
      }
    }
  });

  map_use(f, mp_reg);

  remove_unused_def(f);
}

void gcm_schedule_early(NormalFunc *f) {
  // if(f->name!="main")return;

  dbg << "## gcm schedule early\n";

  // schedule early

  auto S = build_dom_tree(f);
  std::function<void(BB *)> dfs;

  std::unordered_map<Reg, int> def_depth;
  int depth = 0;

  dfs = [&](BB *w) {
    // if(!w->disable_schedule_early)
    for (auto it = w->instrs.begin(); it != w->instrs.end();) {
      Instr *i = it->get();
      Case(RegWriteInstr, i0, i) {
        if (!pinned(i0) && !w->disable_schedule_early) {
          int d = 0;
          i0->map_use([&](Reg &r) { d = std::max(d, def_depth.at(r)); });
          BB *u = w, *u1, *u2 = u;
          int d1 = depth, d2 = depth;
          for (; d < depth; ++d, u2 = u1) {
            u1 = S[u2].dom_fa;
            --d1;
            if (S[u1].loop_depth <= S[w].loop_depth) u = u1, d2 = d1;
          }
          def_depth[i0->d1] = d2;
          if (u != w) {
            u->instrs.insert(--u->instrs.end(), std::move(*it));
            auto del = it++;
            w->instrs.erase(del);
            continue;
          }
        } else {
          def_depth[i0->d1] = depth;
        }
      }
      ++it;
    }
    ++depth;
    for (BB *u : S[w].dom_ch) {
      dfs(u);
    }
    --depth;
    w->for_each([&](Instr *i) {
      Case(RegWriteInstr, i0, i) { def_depth.erase(i0->d1); }
    });
  };

  dfs(f->entry);

  print_ssa(f);
}

void gcm_schedule_late(NormalFunc *f) {
  dbg << "## gcm schedule late\n";

  // schedule early

  auto S = build_dom_tree(f);

  std::unordered_map<Reg, BB *> uses_lca;

  auto cal_use_lca = [&](Reg r, BB *u) {
    BB *&z = uses_lca[r];
    if (!z)
      z = u;
    else
      while (!S[z].dom(S[u])) z = S[z].dom_fa;
    // dbg<<"uses lca: "<<f->get_name(r)<<": "<<z->name<<" updated by
    // "<<u->name<<"\n";
  };

  print_cfg(S, f);

  dom_tree_rdfs(S, [&](BB *w) {
    for (BB *u : S[w].out) {
      u->for_each([&](Instr *i) {
        Case(PhiInstr, i0, i) {
          for (auto &kv : i0->uses) {
            if (kv.second == w) cal_use_lca(kv.first, w);
          }
        }
      });
    }
    for (auto it = w->instrs.end(); it != w->instrs.begin();) {
      --it;
      Instr *i = it->get();
      Case(RegWriteInstr, i0, i) {
        if (!pinned(i0)) {
          CaseNot(MemRead, i0) {
            BB *u = uses_lca[i0->d1];
            if (u) {
              if (!S[w].dom(S[u])) {
                dbg << f->get_name(i0->d1) << "\n";
                dbg << w->name << " should dom " << u->name << "\n";
                assert(0);
              }
              BB *v = S[w].get_loop_rt();
              if (!v || in_loop(S, u, v))
                while (S[u].get_loop_rt() != v) {
                  u = S[u].dom_fa;
                }
              if (u != w) {
                auto del = it++;
                u->instrs.emplace_front(std::move(*del));
                w->instrs.erase(del);
                i->map_use([&](Reg &r) { cal_use_lca(r, u); });
                continue;
              }
            }
          }
        }
      }
      CaseNot(PhiInstr, i) {
        i->map_use([&](Reg &r) { cal_use_lca(r, w); });
      }
    }
  });

  print_ssa(f);
}
}  // namespace GVN

bool gcm_move_branch_out_of_loop(NormalFunc *f) {
  dbg << "## gcm_move_branch_out_of_loop: " << f->name << "\n";
  auto S = build_dom_tree(f);
  auto defs = build_defs(f);
  auto i2bb = build_in2bb(f);
  // print_dom_tree(S,f);
  // print_loop_tree(S,f);
  // print_cfg(S,f);
  return f->for_each_until([&](BB *w) {
    Case(BranchInstr, i0, w->back()) {
      dbg << "??w:" << w->name << "\n";
      BB *u = S[w].get_loop_rt();
      if (!u) return 0;
      BB *z = i2bb.at(defs.at(i0->cond));
      dbg << "?w:" << w->name << "\n";
      dbg << "?z:" << z->name << "\n";
      dbg << "?u:" << u->name << "\n";
      if (!S[z].sdom(S[u])) return 0;
      for (;;) {
        BB *u1 = S[u].loop_fa;
        if (!u1) break;
        if (!S[z].dom(S[u1])) return 0;
        u = u1;
      }
      dbg << "w:" << w->name << "\n";
      dbg << "z:" << z->name << "\n";
      dbg << "u:" << u->name << "\n";
      for (auto v : S[u].loop_exit) dbg << "exit " << v->name << "\n";
      // cond def at z, z dom u
      // move branch to u
      if (!S[u].loop_simple) return 0;
      BB *exit = *S[u].loop_exit.begin();
      std::cerr << "\x1b[34m"
                << "move branch out of loop"
                << "\x1b[0m"
                << "\n";
      dbg << "w:" << w->name << "\n";
      dbg << "z:" << z->name << "\n";
      dbg << "u:" << u->name << "\n";
      BB *pre_exit = f->new_BB("pre_exit");
      pre_exit->push(new JumpInstr(exit));
      Reg cond = i0->cond;
      BB *pre_entry = f->new_BB("pre_entry");
      S[u].loop_pre->back()->map_BB([&](BB *&bb) {
        if (bb == u) bb = pre_entry;
      });

      std::unordered_map<BB *, BB *> mp_bb;
      std::unordered_set<Reg> new_reg;
      loop_tree_for_each(S, u, [&](BB *w) {
        mp_bb[w] = f->new_BB(w->name + "_m");
        w->for_each([&](Instr *i) {
          Case(RegWriteInstr, i0, i) { new_reg.insert(i0->d1); }
        });
      });
      for (auto &kv : mp_bb) {
        BB *w = kv.first, *u = kv.second;
        if (w == exit) continue;
        w->for_each([&](Instr *i) {
          auto j = i->copy();
          u->push(j);
        });
        w->map_BB(partial_map(exit, pre_exit));
        u->map_BB(partial_map(exit, pre_exit));
        u->map_BB(partial_map(mp_bb));
        w->back()->map_use([&](Reg &r) {
          if (r == cond) {
            r = f->new_Reg();
            w->push1(new LoadConst(r, 1));
          }
        });
        u->back()->map_use([&](Reg &r) {
          if (r == cond) {
            r = f->new_Reg();
            w->push1(new LoadConst(r, 0));
          }
        });
      }
      pre_entry->push(new BranchInstr(cond, u, mp_bb.at(u)));
      ssa_construction(f, [&](Reg r) { return new_reg.count(r); });
      repeat(dce_BB)(f);
      return 1;
    }
    return 0;
  });
}

void gcm(CompileUnit &c) {
  c.for_each(GVN::gcm_schedule_late);
  dbg << "```cpp\n" << c.scope << "```\n";
}

void gvn(CompileUnit &c) {
  c.for_each(GVN::gvn);
  c.for_each(GVN::gcm_schedule_early);
  c.for_each(GVN::gvn);
}
