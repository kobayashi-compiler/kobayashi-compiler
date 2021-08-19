#include "pass.hpp"

void del_edge(BB *src, BB *dst) {
  dst->for_each([&](Instr *i) {
    Case(PhiInstr, i0, i) {
      auto it = std::find_if(i0->uses.begin(), i0->uses.end(),
                             [&](auto &x) { return x.second == src; });
      if (it != i0->uses.end()) i0->uses.erase(it);
    }
  });
}

bool dce_BB(NormalFunc *f) {
  dbg << "#### dce BB: " << f->name << "\n";
  std::queue<BB *> q;
  struct State {
    bool visited = 0, one_goto_entry = 0;
    BB *pre = NULL, *nxt = NULL;
  };
  auto defs = build_defs(f);
  bool changed = 0;
  std::unordered_map<BB *, State> S;
  auto visit = [&](BB *&x, BB *pre, bool jump) {
    /*int t=0;
    while(x->instrs.size()==1){
            if(++t==5)break;
            // optional:
            // goto x
            // x: goto y => goto y
            Case(JumpInstr,y,x->back()){
                    x=y->target;
            }else break;
    }*/
    State &s = S[x];
    if (!s.visited) {
      s.visited = 1;
      q.push(x);
      s.pre = pre;
      S[pre].nxt = x;
      s.one_goto_entry = jump;
    } else {
      s.one_goto_entry = 0;
    }
  };
  visit(f->entry, NULL, 0);
  while (!q.empty()) {
    BB *w = q.front();
    q.pop();
    Instr *x = w->back();
    Case(JumpInstr, y, x) { visit(y->target, w, 1); }
    else Case(BranchInstr, y, x) {
      auto elim_branch = [&](BB *z) {
        BB *d = (z == y->target1 ? y->target0 : y->target1);
        del_edge(w, d);
        w->pop();
        w->push(new JumpInstr(z));
        changed = 1;
        visit(z, w, 1);
      };
      if (y->target1 == y->target0) {
        assert(0);
        // double edge is forbidden
        // elim_branch(y->target1);
      } else {
        auto cond = defs.at(y->cond);
        Case(LoadConst, lc, cond) {
          BB *z = (lc->value ? y->target1 : y->target0);
          elim_branch(z);
        }
        else {
          visit(y->target1, w, 0);
          visit(y->target0, w, 0);
        }
      }
    }
    else Case(ReturnInstr, y, x) {
      ;
    }
    else {
      Reg r = f->new_Reg();
      w->push(new LoadConst(r, 0));
      w->push(new ReturnInstr(r, true));
      dbg << "Warning: no jump at the end of BB: " << w->name << "\n";
    }
  }

  auto nex = [&](BB *bb) -> bool { return !S[bb].visited; };
  f->for_each([&](BB *w) {
    if (nex(w)) {
      for (BB *u : get_BB_out(w)) del_edge(w, u);
    }
  });

  f->for_each([&](BB *w) { dbg << w->name << ": " << !nex(w) << "\n"; });
  dbg << "```cpp\n" << *f << "```\n";

  f->for_each([&](BB *bb) {
    // optional: compress [xxx]->[yy] into [xxxyy]
    State &s = S[bb];
    while (s.visited && !s.one_goto_entry && s.nxt && S[s.nxt].one_goto_entry) {
      dbg << "merge: " << bb->name << " " << s.nxt->name << "\n";
      for (auto it = s.nxt->instrs.begin(); it != s.nxt->instrs.end(); ++it) {
        Case(PhiInstr, i0, it->get()) {
          assert(i0->uses.size() == 1);
          assert(i0->uses.at(0).second == bb);
          *it = std::unique_ptr<Instr>(
              new UnaryOpInstr(i0->d1, i0->uses.at(0).first, UnaryOp::ID));
        }
      }
      bb->pop();
      bb->instrs.splice(bb->instrs.end(), s.nxt->instrs);
      phi_src_rewrite(bb, s.nxt);
      S[s.nxt].visited = 0;
      s.nxt = S[s.nxt].nxt;
      changed = 1;
    }
  });
  // remove unused BBs
  f->bbs.erase(remove_if(f->bbs.begin(), f->bbs.end(),
                         [&](auto &bb) { return nex(bb.get()); }),
               f->bbs.end());
  return changed;
}

void remove_unused_memobj(CompileUnit &c) {
  std::unordered_set<MemObject *> used;
  c.for_each([&](NormalFunc *f) {
    f->for_each([&](Instr *i) {
      Case(LoadAddr, la, i) { used.insert(la->offset); }
    });
    for (auto &kv : f->scope.array_args) {
      used.insert(kv.second);
      // array args are never removed
    }
  });

  // remove local defs
  c.for_each([&](NormalFunc *f) {
    f->for_each([&](BB *bb) {
      bb->instrs.remove_if([&](auto &nex) {
        Case(LocalVarDef, x, nex.get()) {
          if (!used.count(x->data)) return 1;
        }
        return 0;
      });
    });
  });

  // remove from scopes
  auto nex = [&](std::unique_ptr<MemObject> &m) {
    return !used.count(m.get()) && !m->arg;
  };
  c.for_each([&](NormalFunc *f) {
    auto &ms = f->scope.objects;
    ms.erase(std::remove_if(ms.begin(), ms.end(), nex), ms.end());
  });
  auto &ms = c.scope.objects;
  ms.erase(std::remove_if(ms.begin(), ms.end(), nex), ms.end());
}

bool remove_unused_loop(NormalFunc *f) {
  // effect: CFG structure
  dbg << "#### remove unused loop: " << f->name << "\n";
  bool is_main = (f->name == "main");

  auto uses = build_use_count(f);
  auto S = build_dom_tree(f);
  for (auto &w0 : f->bbs) {
    BB *w = w0.get();
    auto &sw = S.at(w);
    if (!sw.loop_rt) continue;
    if (sw.loop_exit.size() != 1) continue;
    dbg << "check: " << w->name << "\n";
    BB *exit = *sw.loop_exit.begin();
    std::unordered_map<Reg, RegWriteInstr *> loop_defs;
    std::unordered_map<Reg, int> loop_uses;
    bool unused = 1;
    loop_tree_for_each(S, w, [&](BB *u) {
      get_defs(loop_defs, u);
      get_use_count(loop_uses, u);
      u->for_each([&](Instr *i) {
        Case(MemEffect, i0, i) {
          if (i0->data->global && i0->data->arg || !is_main) unused = 0;
        }
        else Case(MemWrite, i0, i) {
          if (!is_main) unused = 0;
        }
      });
    });
    if (unused)
      exit->for_each([&](Instr *i) {
        Case(PhiInstr, i0, i) {
          i0->map_use([&](Reg &r) {
            if (loop_defs.count(r)) unused = 0;
          });
        }
      });
    if (unused)
      for (auto &kv : loop_defs) {
        Reg r = kv.first;
        if (loop_uses[r] != uses[r]) {
          unused = 0;
          break;
        }
      }
    if (unused) {
      auto os = dbg.sync(std::cout);
      os << "\x1b[34m";
      os << "unused: " << w->name << "\n";
      os << "remove unused loop"
         << "\x1b[0m"
         << "\n";
      print_cfg(S, f);
      print_ssa(f);
      BB *exit = *sw.loop_exit.begin();
      w->instrs.clear();
      w->push(new JumpInstr(exit));
      std::unordered_set<BB *> del;
      loop_tree_for_each(S, w, [&](BB *u) {
        if (u != w) del.insert(u);
      });
      auto nex = [&](auto &bb) { return del.count(bb.get()); };
      f->bbs.erase(remove_if(f->bbs.begin(), f->bbs.end(), nex), f->bbs.end());
      repeat(dce_BB)(f);
      remove_unused_def(f);
      return 1;
    }
  }
  return 0;
}

void remove_unused_def(NormalFunc *f) {
  std::unordered_set<Instr *> used;
  auto defs = build_defs(f);
  std::function<void(Instr * i)> dfs;
  dfs = [&](Instr *i) {
    if (used.count(i)) return;
    used.insert(i);
    i->map_use([&](Reg &v) {
      if (v.id && !defs.count(v)) {
        SetPrintContext _(f);
        dbg << "no def:" << *i << "\n";
        dbg << v << "\n";
      }
      if (v.id) dfs(defs.at(v));
    });
  };
  f->for_each([&](Instr *i) {
    bool t = 0;

    Case(LocalVarDef, i0, i) t = 1;

    Case(CallInstr, i0, i) t = !(i0->pure);
    Case(ControlInstr, i0, i) t = 1;
    Case(StoreInstr, i0, i) t = 1;

    Case(MemDef, i0, i) t = 1;
    Case(MemUse, i0, i) t = 1;
    Case(MemEffect, i0, i) t = 1;
    Case(MemWrite, i0, i) {
      if (i0->data->arg)
        t = 1;
      else if (i0->data->global) {
        if (f->name != "main") t = 1;
      }
    }

    if (t) dfs(i);
  });
  bool flag = 0;
  f->for_each([&](BB *bb) {
    SetPrintContext _(f);
    bb->instrs.remove_if([&](std::unique_ptr<Instr> &i) {
      if (!used.count(i.get())) {
        dbg << "remove: " << *i << "\n";
        return 1;
      }
      return 0;
    });
  });

  dbg << "### remove unused def\n";
  print_ssa(f);
}

std::vector<NormalFunc *> get_call_order(CompileUnit &c) {
  std::unordered_set<NormalFunc *> visited;
  std::vector<NormalFunc *> vec;
  std::function<void(NormalFunc *)> dfs;
  dfs = [&](NormalFunc *f) {
    if (visited.count(f)) return;
    visited.insert(f);
    f->for_each([&](Instr *x) {
      Case(CallInstr, y, x) {
        Case(NormalFunc, g, y->f) {
          // f call g
          if (g != f) dfs(g);
        }
      }
    });
    vec.push_back(f);
    dbg << vec.size() << ": " << f->name << "\n";
  };
  dfs(c.funcs.at("main").get());
  return vec;
}

void remove_unused_func(CompileUnit &c) {
  auto order = get_call_order(c);
  std::unordered_set<NormalFunc *> visited(order.begin(), order.end());

  std::vector<std::string> del;
  c.for_each([&](NormalFunc *f) {
    if (!visited.count(f)) del.push_back(f->name);
  });
  dbg << "## remove unused func\n";
  dbg << "```cpp\n" << *c.funcs.at("main").get() << "```\n\n";
  for (auto &x : del) {
    dbg << "remove: " << x << "\n";
    c.funcs.erase(x);
  }
  dbg << "```cpp\n" << *c.funcs.at("main").get() << "```\n\n";
}
