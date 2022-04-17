#include "common/common.hpp"
#include "optimizer/pass.hpp"

void into_array_ssa(CompileUnit &c) {
  dbg << "#### ssa => array ssa\n";

  auto rw = get_array_rw(c);

  std::unordered_set<MemObject *> mutable_global_var;
  for (auto &kv : rw) {
    for (auto &kv2 : kv.second.global_rw) {
      if (kv2.second) {
        mutable_global_var.insert(kv2.first);
        dbg << *kv2.first << ": mutable\n";
      }
    }
  }

  c.scope.for_each([&](MemObject *m) {
    m->is_const = (!m->arg && !mutable_global_var.count(m));
  });

  for (auto &kv : rw) {
    CaseNot(NormalFunc, kv.first) continue;
    dbg << "```cpp\n";
    dbg << "rw " << kv.first->name << "\n";
    dbg << "global rw:\n";
    for (auto &kv2 : kv.second.global_rw) {
      dbg << *kv2.first << ": " << (kv2.second ? "rw" : "ro") << "\n";
    }
    dbg << "arg rw:\n";
    for (auto &kv2 : kv.second.arg_rw) {
      dbg << kv2.first << ": " << (kv2.second.may_write ? "rw" : "ro") << "\n";
      for (auto alias : kv2.second.maybe) dbg << "maybe: " << *alias << "\n";
    }
    dbg << "```\n";
  }

  c.for_each([&](NormalFunc *f) {
    std::unordered_map<MemObject *, Reg> m2r;
    std::unordered_set<Reg> new_reg;

    c.scope.for_each([&](MemObject *m) {
      Reg r = f->new_Reg(m->name + (m->arg ? ":IO" : ":G"));
      f->entry->push_front(new MemDef(r, m));
      m2r[m] = r;
      new_reg.insert(r);
    });
    f->scope.for_each([&](MemObject *m) {
      Reg r = f->new_Reg(m->name + (m->arg ? ":A" : ":L"));
      m2r[m] = r;
      new_reg.insert(r);
    });
    if (!rw.count(f)) std::cerr << "func not in rw: " << f->name << "\n";
    assert(rw.count(f));

    auto &types = rw.at(f).types;

    f->for_each([&](BB *bb) {
      // dbg<<"```cpp\n"<<*bb<<"```\n\n";
      for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
        Instr *x = it->get();
        Case(LoadInstr, x0, x) {
          MemObject *m = types.at(x0->addr).base;
          Reg mem = m2r.at(m);
          *it = std::unique_ptr<Instr>(new MemRead(x0->d1, mem, x0->addr, m));
        }
        else Case(StoreInstr, x0, x) {
          MemObject *m = types.at(x0->addr).base;
          if (m->is_const) {
            std::cerr << *m << " is const but is written in" << f->name << ": "
                      << bb->name << "\n";
          }
          assert(!m->is_const);
          Reg mem = m2r.at(m);
          *it = std::unique_ptr<Instr>(
              new MemWrite(mem, mem, x0->addr, x0->s1, m));
          auto it2 = it;
          ++it2;
          // alias
          auto &sf = rw.at(f);
          for (auto am : sf.alias[m]) {
            if (!m2r.count(am)) continue;
            Reg mem0 = m2r.at(am);
            bb->instrs.insert(
                it2, std::unique_ptr<Instr>(new MemEffect(mem0, mem0, am)));
          }
        }
        else Case(LocalVarDef, x0, x) {
          MemObject *m = x0->data;
          Reg r = m2r.at(m);
          bb->instrs.insert(it, std::unique_ptr<Instr>(new MemDef(r, m)));
        }
        else Case(CallInstr, x0, x) {
          auto it2 = it;
          ++it2;
          auto &sg = rw.at(x0->f);
          std::unordered_map<MemObject *, bool> used, used_a;
          for (auto &kv : sg.global_rw) {
            used[kv.first] |= kv.second;
          }
          for (auto &kv : sg.arg_rw) {
            auto m = types.at(x0->args.at(kv.first)).base;
            used[m] |= kv.second.may_write;
          }
          used_a = used;
          auto &sf = rw.at(f);
          for (auto &kv : used) {
            for (auto m : sf.alias[kv.first]) {
              used_a[m] |= kv.second;
            }
          }
          x0->pure = 1;
          for (auto &kv : used_a) {
            auto m = kv.first;
            if (!m2r.count(m)) continue;
            Reg mem0 = m2r.at(m);
            auto use = new MemUse(mem0, m);
            bb->instrs.insert(it, std::unique_ptr<Instr>(use));
            x0->in.push_back(use);
            if (kv.second) {
              auto effect = new MemEffect(mem0, mem0, m);
              bb->instrs.insert(it2, std::unique_ptr<Instr>(effect));
              x0->pure = 0;
            }
          }
        }
      }
      // dbg<<"```cpp\n"<<*bb<<"```\n\n";
    });
    ssa_construction(f, [&](Reg r) -> bool { return new_reg.count(r); });
    auto S = build_dom_tree(f);
    print_cfg(S, f);
    print_ssa(f);
  });

  // TODO
}

std::unordered_map<Reg, MemObject *> infer_array_regs(NormalFunc *f) {
  std::unordered_map<Reg, MemObject *> mp;
  auto defs = build_defs(f);
  std::function<void(Instr *)> dfs;
  dfs = [&](Instr *_i) {
    Case(RegWriteInstr, i, _i) {
      if (mp.count(i->d1)) return;
      mp[i->d1] = NULL;
      Case(MemWrite, i0, i) { mp[i0->d1] = i0->data; }
      else Case(MemDef, i0, i) {
        mp[i0->d1] = i0->data;
      }
      else Case(MemEffect, i0, i) {
        mp[i0->d1] = i0->data;
      }
      else Case(PhiInstr, i0, i) {
        MemObject *ans = NULL;
        for (auto &kv : i0->uses) {
          dfs(defs.at(kv.first));
          auto v = mp.at(kv.first);
          if (v != NULL) {
            assert(ans == NULL || ans == v);
            ans = v;
          }
        }
        mp[i0->d1] = ans;
      }
      else Case(UnaryOpInstr, i0, i) {
        dfs(defs.at(i0->s1));
        mp[i0->d1] = mp.at(i0->s1);
      }
    }
  };
  f->for_each(dfs);
  return mp;
}

std::unordered_map<MemObject *, MemObject *> stack_array_to_global(
    CompileUnit &c);

void from_array_ssa(CompileUnit &c) {
  auto sa2g = stack_array_to_global(c);

  dbg << "#### array ssa => ssa\n";
  c.for_each([&](NormalFunc *f) {
    std::unordered_set<Reg> mem_regs;
    auto A = infer_array_regs(f);
    auto dels = [&](BB *bb) {
      for (auto it = bb->instrs.begin(); it != bb->instrs.end();) {
        Instr *x = it->get();
        bool del_flag = 0;
        Case(MemDef, x0, x) {
          mem_regs.insert(x0->d1);
          del_flag = 1;
        }
        else Case(MemUse, x0, x) {
          del_flag = 1;
        }
        else Case(MemEffect, x0, x) {
          mem_regs.insert(x0->d1);
          del_flag = 1;
        }
        else Case(MemRead, x0, x) {
          *it = std::unique_ptr<Instr>(new LoadInstr(x0->d1, x0->addr));
        }
        else Case(MemWrite, x0, x) {
          if (sa2g.count(x0->data))
            del_flag = 1;
          else {
            mem_regs.insert(x0->d1);
            *it = std::unique_ptr<Instr>(new StoreInstr(x0->addr, x0->s1));
          }
        }
        else Case(CallInstr, x0, x) {
          x0->pure = 0;
          x0->in.clear();
        }
        else Case(PhiInstr, x0, x) {
          del_flag = (A.at(x0->d1) != NULL);
        }
        else Case(UnaryOpInstr, x0, x) {
          del_flag = (A.at(x0->d1) != NULL);
        }
        else Case(LoadAddr, la, x) {
          partial_map(sa2g)(la->offset);
        }
        if (del_flag) {
          auto del = it++;
          bb->instrs.erase(del);
        } else {
          ++it;
        }
      }
    };
    f->for_each(dels);
    dbg << f->name << "\n";
    print_ssa(f);
  });
}

std::unordered_map<MemObject *, MemObject *> stack_array_to_global(
    CompileUnit &c) {
  std::unordered_map<MemObject *, MemObject *> ret;

  PassIsEnabled("stack-array-to-global");
  else return ret;

  struct State {
    bool effect = 0;
    std::unordered_set<Reg> uses;
  };
  c.for_each([&](NormalFunc *f) {
    auto defs = build_defs(f);
    std::unordered_map<MemObject *, State> S;
    f->for_each([&](Instr *x) {
      Case(MemRead, x0, x) { S[x0->data].uses.insert(x0->mem); }
      else Case(MemEffect, x0, x) {
        S[x0->data].effect = 1;
      }
      else Case(MemUse, x0, x) {
        S[x0->data].uses.insert(x0->s1);
      }
    });
    for (auto &kv : S) {
      MemObject *data = kv.first;
      State &s = kv.second;
      if (data->global) continue;
      if (data->arg) continue;
      if (data->size > 256) continue;
      if (data->size <= 4) continue;
      if (s.effect) continue;
      if (s.uses.size() != 1) continue;

      Instr *last = defs.at(*s.uses.begin());
      std::unordered_map<int, int> mp;
      for (int t = 0; t < 64; ++t) {
        // auto os=dbg.sync(std::cout);
        // SetPrintContext _(f);
        // os<<t<<": "<<*last<<"\n";
        Case(MemWrite, mw, last) {
          Instr *s1 = defs.at(mw->s1);
          int s1_value = 0, pos = -1;
          Case(LoadConst, lc, s1) { s1_value = lc->value; }
          else break;
          Instr *addr = defs.at(mw->addr);
          Case(BinaryOpInstr, bop, addr) {
            if (bop->op.type != BinaryOp::ADD) break;
            auto s1 = defs.at(bop->s1);
            auto s2 = defs.at(bop->s2);
            CaseNot(LoadAddr, s1) std::swap(s1, s2);
            CaseNot(LoadAddr, s1) break;
            Case(LoadConst, lc, s2) { pos = lc->value; }
            else break;
          }
          else Case(LoadAddr, la, addr) {
            pos = 0;
          }
          else break;
          assert(pos != -1);
          if (!mp.count(pos)) {
            mp[pos] = s1_value;
            // os<<pos<<": "<<s1_value<<"\n";
          }
          last = defs.at(mw->mem);
        }
        else Case(MemDef, md, last) {
          MemObject *new_data = c.scope.new_MemObject(data->name);
          new_data->global = 1;
          new_data->arg = 0;
          auto initial_value = new int32_t[data->size / 4]();
          for (auto &kv : mp)
            if (kv.first >= 0 && kv.first + 4 <= data->size &&
                kv.first % 4 == 0)
              initial_value[kv.first / 4] = kv.second;
          new_data->init(initial_value, data->size);
          new_data->is_const = 1;
          new_data->dims = data->dims;
          ret[data] = new_data;
          {
            auto os = dbg.sync(std::cout);
            os << "\x1b[34m\n";
            os << "const stack array: " << data->name << "\n";
            for (int i = 0; i < data->size; i += 4)
              os << new_data->at(i) << " ";
            os << "\x1b[0m\n";
          }
          break;
        }
        else break;
      }
    }
  });
  return ret;
}

bool simplify_load_store(NormalFunc *f) {
  dbg << "#### simplify load store: " << f->name << "\n";
  bool changed = 0;
  auto S = build_dom_tree(f);
  auto defs = build_defs(f);
  auto i2bb = build_in2bb(f);
  std::unordered_map<Reg, Reg> mp_reg;
  dom_tree_dfs(S, [&](BB *bb) {
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Instr *i = it->get();
      Case(MemRead, mr, i) {
        Instr *i0 = defs.at(mr->mem);
        for (;;) {
          Case(MemWrite, mw, i0) {
            if (mw->addr == mr->addr) {
              mp_reg[mr->d1] = mw->s1;
            }
            /*if(proved_neq(mw->addr,mr->addr)){ //TODO
                    i0=defs.at(mw->mem);
                    continue;
            }*/
          }
          else Case(PhiInstr, phi, i0) {
            std::vector<std::pair<Reg, BB *>> v0;
            for (auto &kv : phi->uses) {
              auto def = defs.at(kv.first);
              Case(MemWrite, mw2, def) {
                if (mw2->addr == mr->addr) {
                  v0.emplace_back(mw2->s1, kv.second);
                }
              }
              else break;
            }
            if (v0.size() == phi->uses.size()) {
              Reg phi_var = f->new_Reg(f->get_name(phi->d1) + "_m2r");
              auto phi_instr = new PhiInstr(phi_var);
              for (auto &kv : v0) {
                phi_instr->add_use(kv.first, kv.second);
              }
              i2bb.at(phi)->push_front(phi_instr);
              mp_reg[mr->d1] = phi_var;
              dbg << "```\n"
                  << f->get_name(mr->d1) << " >> " << f->get_name(phi_var)
                  << "\n"
                  << "```\n";
            }
          }
          else Case(MemDef, md, i0) {
            if (md->data->global && (md->data->is_const || f->name == "main")) {
              auto addr = defs.at(mr->addr);
              Case(BinaryOpInstr, bop, addr) {
                if (bop->op.type != BinaryOp::ADD) break;
                auto s1 = defs.at(bop->s1);
                auto s2 = defs.at(bop->s2);
                CaseNot(LoadAddr, s1) std::swap(s1, s2);
                CaseNot(LoadAddr, s1) break;
                Case(LoadConst, lc, s2) {
                  int value = md->data->at(lc->value);
                  Reg r = f->new_Reg();
                  f->entry->push_front(new LoadConst(r, value));
                  mp_reg[mr->d1] = r;
                  dbg << "```\n"
                      << f->get_name(mr->d1) << " >>[lc]>> " << f->get_name(r)
                      << "\n"
                      << "```\n";
                }
              }
              else Case(LoadAddr, la, addr) {
                int value = md->data->at(0);
                Reg r = f->new_Reg();
                f->entry->push_front(new LoadConst(r, value));
                mp_reg[mr->d1] = r;
                dbg << "```\n"
                    << f->get_name(mr->d1) << " >>[lc]>> " << f->get_name(r)
                    << "\n"
                    << "```\n";
              }
            }
          }
          break;
        }
      }
    }
  });
  changed |= !mp_reg.empty();
  map_use(f, mp_reg);
  remove_unused_def(f);
  return changed;
}

void array_to_struct(CompileUnit &c) {
  dbg << "## array to struct\n";
  into_array_ssa(c);
  typedef std::pair<NormalFunc *, Reg> FReg;
  struct State {
    bool bad = 0;
    std::unordered_map<FReg, double> weight;
  };
  std::unordered_map<MemObject *, State> S;
  std::unordered_map<FReg, std::vector<MemObject *>> groups;
  c.for_each([&](NormalFunc *f) {
    auto prob = estimate_BB_prob(f);
    auto defs = build_defs(f);
    f->for_each([&](BB *bb) {
      double p = prob.at(bb);
      bb->for_each([&](Instr *x) {
        Case(MemRead, x0, x) {
          Case(ArrayIndex, ai, defs.at(x0->addr)) {
            S[x0->data].weight[FReg(f, ai->s2)] += p;
          }
          else S[x0->data].bad = 1;
        }
        else Case(MemWrite, x0, x) {
          Case(ArrayIndex, ai, defs.at(x0->addr)) {
            S[x0->data].weight[FReg(f, ai->s2)] += p;
          }
          else S[x0->data].bad = 1;
        }
        else Case(MemEffect, x0, x) {
          S[x0->data].bad = 1;
        }
        else Case(MemUse, x0, x) {
          S[x0->data].bad = 1;
        }
      });
    });
  });
  from_array_ssa(c);
  for (auto &kv : S) {
    MemObject *mem = kv.first;
    State &s = kv.second;
    if (s.bad) continue;
    if (!mem->global) continue;
    if (mem->arg) continue;
    if (mem->initial_value != NULL) continue;
    if (mem->dims.size() != 1) continue;
    for (auto &kv : s.weight) {
      groups[kv.first].push_back(mem);
    }
  }
  std::unordered_set<std::pair<MemObject *, MemObject *>> pairs;
  std::unordered_map<MemObject *, std::vector<MemObject *>> merged;
  for (auto &kv : groups) {
    for (MemObject *m1 : kv.second) {
      for (MemObject *m2 : kv.second) {
        if (m1 < m2 && m1->dims == m2->dims)
          pairs.insert(std::make_pair(m1, m2));
      }
    }
  }
  for (auto &pair : pairs) {
    MemObject *m1 = pair.first, *m2 = pair.second;
    double w1 = 0, w2 = 0;
    for (auto &kv : S.at(m1).weight) {
      (S.at(m2).weight.count(kv.first) ? w1 : w2) += kv.second;
    }
    for (auto &kv : S.at(m2).weight) {
      (S.at(m1).weight.count(kv.first) ? w1 : w2) += kv.second;
    }
    if (w1 >= w2 * 4) {
      merged[m1].push_back(m2);
      merged[m2].push_back(m1);
      // std::cout<<"merged: "<<m1<<"  "<<m2<<"\n";
    }
  }
  std::unordered_map<MemObject *, std::pair<MemObject *, int>> mp_mem;
  auto os = dbg.sync(std::cout);
  for (auto &kv : merged) {
    auto cur = kv.first;
    if (kv.second.empty()) continue;
    std::queue<MemObject *> q;
    auto m0 = c.scope.new_MemObject(cur->name + ":merged");
    m0->size = 0;
    m0->global = 1;
    m0->arg = 0;
    m0->dims = cur->dims;
    m0->dims.push_back(0);
    int id = 0;
    auto visit = [&](MemObject *m) {
      if (mp_mem.count(m)) return;
      mp_mem[m] = std::make_pair(m0, m0->dims.back());
      m0->size += cur->size;
      os << "trans:\n"
         << *m << "\n->\n"
         << *m0 << "\n"
         << m0->dims.back() << "\n\n";
      m0->dims.back() += 1;
      q.push(m);
    };
    visit(kv.first);
    while (!q.empty()) {
      auto m1 = q.front();
      q.pop();
      for (auto m2 : merged.at(m1)) visit(m2);
    }
  }
  /*for(auto &kv:groups){
          auto os=dbg.sync(std::cout);
          os<<kv.first.first->name<<","<<kv.first.second<<"\n";
          for(MemObject *mem:kv.second){
                  os<<*mem<<"\n";
          }
          os<<"\n";
  }*/
  c.for_each([&](NormalFunc *f) {
    auto type = simple_type_check(f);
    auto defs = build_defs(f);
    f->for_each([&](BB *bb) {
      for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
        Instr *x = it->get();
        auto rewrite_addr = [&](Reg &addr) {
          auto m0 = type.at(addr).base;
          if (mp_mem.count(m0)) {
            auto &kv = mp_mem.at(m0);
            auto m1 = kv.first;
            int id = kv.second;
            assert(m1->dims.size() == 2);
            assert(0 <= id && id < m1->dims.back());
            Case(ArrayIndex, ai, defs.at(addr)) {
              addr = f->new_Reg();
              Reg r_id = f->new_Reg();
              f->entry->push_front(new LoadConst(r_id, id));
              bb->instrs.insert(it, std::unique_ptr<Instr>(new ArrayIndex(
                                        addr, ai->d1, r_id, 4, -1)));
            }
            else assert(0);
          }
        };
        Case(LoadAddr, x0, x) {
          if (mp_mem.count(x0->offset))
            x0->offset = mp_mem.at(x0->offset).first;
        }
        else Case(LoadInstr, x0, x) {
          rewrite_addr(x0->addr);
        }
        else Case(StoreInstr, x0, x) {
          rewrite_addr(x0->addr);
        }
        else Case(ArrayIndex, x0, x) {
          auto m0 = type.at(x0->d1).base;
          if (mp_mem.count(m0)) {
            assert(x0->size == 4);
            auto &kv = mp_mem.at(m0);
            auto m1 = kv.first;
            x0->size *= m1->dims.back();
          }
        }
      }
    });
  });
  dbg << "## array to struct end\n";
  c.for_each(print_ssa);
}

void array_ssa_passes(CompileUnit &c, int _check_loop) {
  dbg << "#### array ssa passes\n";
  remove_unused_func(c);
  remove_unused_memobj(c);
  into_array_ssa(c);
  gvn(c);
  PassIsEnabled("simplify-load-store") c.for_each(repeat(simplify_load_store));
  PassIsEnabled("remove-unused-loop") c.for_each(repeat(remove_unused_loop));
  std::unordered_map<NormalFunc *, std::unordered_map<BB *, LoopInfo>> LI;
  if (_check_loop) {
    gcm(c);
    c.for_each([&](NormalFunc *f) { LI[f] = check_loop(f, _check_loop); });
  }
  from_array_ssa(c);
  if (_check_loop == 2) {
    c.for_each([&](NormalFunc *f) {
      PassIsEnabled("loop-parallel") loop_parallel(LI[f], c, f);
    });
  }
  if (_check_loop == 1) {
    c.for_each([&](NormalFunc *f) {
      PassIsEnabled("loop-unroll") loop_unroll(LI[f], f);
    });
  }
  // TODO
}
