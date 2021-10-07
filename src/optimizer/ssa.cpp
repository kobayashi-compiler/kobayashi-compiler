#include "optimizer/pass.hpp"

void ssa_construction(NormalFunc *f, std::function<bool(Reg)> check) {
  dbg << "## ssa construction: " << f->name << "\n";
  // dbg<<"```cpp\n"<<f->scope<<"```\n\n";
  auto &rns = print_ctx.instr_comment;
  rns.clear();
  // dbg<<"A\n";
  // dbg<<"```cpp\n"<<*f<<"```\n\n";
  f->for_each([&](Instr *x) {
    Case(RegWriteInstr, y, x) {
      if (check(y->d1)) rns[x] = f->get_name(y->d1);
    }
  });
  dbg << "B\n";
  dbg << "```cpp\n" << *f << "```\n\n";
  rns.clear();

  auto S = build_dom_tree(f);
  struct DefState {
    std::unordered_set<BB *> bbs;
    Instr *def_pos = NULL;
    Reg reachingDef = 0;  // reg 0 is uninitialized value
    int id = 0;
  };
  struct InstrState {
    BB *fa;
    int id;
  };
  std::unordered_map<Reg, DefState> defs;
  std::unordered_map<Instr *, InstrState> Si;
  std::unordered_map<PhiInstr *, Reg> phi_var;
  f->for_each([&](BB *bb) {
    bb->for_each([&](Instr *x) {
      Case(RegWriteInstr, y, x) {
        if (check(y->d1)) defs[y->d1].bbs.insert(bb);
      }
    });
  });
  for (auto &kv : defs) {
    SetDebugState _(dbg, 0);
    Reg v(kv.first);
    dbg << v << "\n";
    dbg << "```cpp\n";
    auto &D = kv.second.bbs;
    std::unordered_set<BB *> F, W = D;
    while (!W.empty()) {
      BB *x = *W.begin();
      W.erase(W.begin());
      dbg << "X: " << x->name << "\n";
      for (BB *y : S[x].DF) {
        dbg << "Y in DF(X): " << y->name << "\n";
        if (!F.count(y)) {
          auto phi = new PhiInstr(v);
          phi_var[phi] = v;
          y->push_front(phi);
          F.insert(y);
          if (!D.count(y)) W.insert(y);
        }
      }
    }
    dbg << "```\n";
  }

  f->for_each([&](BB *bb) {
    int id = 0;
    bb->for_each([&](Instr *x) { Si[x] = InstrState{bb, id++}; });
  });

  // dbg<<"```cpp\n"<<*f<<"```\n\n";

  auto updateReachingDef = [&](Reg v, Instr *i) {
    Reg &r = defs[v].reachingDef;
    dbg << v << ".reachingDef: " << r;
    while (r.id) {
      auto &dr = defs[r];
      Instr *def = dr.def_pos;
      BB *b1 = Si[def].fa, *b2 = Si[i].fa;
      if (S[b1].sdom(S[b2])) break;
      if (b1 == b2 && Si[def].id < Si[i].id) break;
      r = dr.reachingDef;
      dbg << " -> " << r;
    }
    dbg << "\n";
  };
  dom_tree_dfs(S, [&](BB *bb) {
    SetDebugState _(dbg, 0);
    dbg << "BB " << bb->name << "\n";
    dbg << "```cpp\n";
    bb->for_each([&](Instr *i) {
      dbg << "visit " << *i << "\n";
      Case(PhiInstr, i1, i) {}
      else i->map_use([&](Reg &v) {
        if (check(v)) {
          dbg << "use " << v << "\n";
          updateReachingDef(v, i);
          v = defs[v].reachingDef;
        }
      });
      Case(RegWriteInstr, i0, i) {
        Reg &v = i0->d1;
        if (check(v)) {
          dbg << "def " << v << "\n";
          updateReachingDef(v, i0);
          Reg v1 =
              f->new_Reg(f->get_name(v) + "_" + std::to_string(++defs[v].id));
          dbg << "ren " << v << " -> " << v1 << "\n";
          defs[v1].def_pos = i0;
          defs[v1].reachingDef = defs[v].reachingDef;
          defs[v].reachingDef = v1;
          dbg << v << ".reachingDef: " << defs[v].reachingDef << "\n";
          dbg << v1 << ".reachingDef: " << defs[v1].reachingDef << "\n";
          v = v1;
        }
      }
    });
    dbg << "```\n";
    for (BB *bb1 : S[bb].out) {
      dbg << "edge " << bb->name << " -> " << bb1->name << "\n";
      dbg << "```cpp\n";
      bb1->for_each([&](Instr *i) {
        Case(PhiInstr, phi, i) {
          if (phi_var.count(phi)) {
            dbg << "visit " << *phi << "\n";
            Reg v = phi_var[phi];
            updateReachingDef(v, bb->back());
            v = defs[v.id].reachingDef;
            phi->add_use(v, bb);
          } else {
            dbg << "visit old " << *phi << "\n";
            for (auto &u : phi->uses) {
              auto &v = u.first;
              if (u.second == bb && check(v)) {
                updateReachingDef(v, bb->back());
                v = defs[v.id].reachingDef;
              }
            }
          }
        }
      });
      dbg << "```\n";
    }
  });

  // dbg<<"```cpp\n"<<*f<<"```\n\n";

  remove_unused_def(f);
}

void print_ssa(NormalFunc *f) {
  auto &rns = print_ctx.instr_comment;
  rns.clear();
  auto defs = build_defs(f);
  std::function<std::string(RegWriteInstr *)> dfs;
  dfs = [&](RegWriteInstr *x) -> std::string {
    if (rns.count(x))
      ;
    else
      Case(LoadAddr, x0, x) { rns[x] = std::string("&") + x0->offset->name; }
    else Case(LoadConst, x0, x) {
      rns[x] = std::to_string(x0->value);
    }
    else Case(LoadArg, x0, x) {
      rns[x] = std::string("arg") + std::to_string(x0->id);
    }
    else Case(ArrayIndex, x0, x) {
      rns[x] = dfs(defs.at(x0->s1)) + "[" + dfs(defs.at(x0->s2)) + "]";
    }
    else Case(UnaryOpInstr, x0, x) {
      if (x0->op.type == UnaryOp::ID) {
        if (!defs.count(x0->s1)) {
          SetPrintContext _(f);
          dbg << "no def:" << *x0 << "\n";
          dbg << x0->s1 << "\n";
          dbg << "```cpp\n" << *f << "```\n";
          assert(0);
        }
        rns[x] = dfs(defs.at(x0->s1));
      } else {
        rns[x] =
            std::string("(") + x0->op.get_name() + dfs(defs.at(x0->s1)) + ")";
      }
    }
    else Case(BinaryOpInstr, x0, x) {
      if (!defs.count(x0->s1)) {
        dbg << "```cpp\n" << *f << "```\n";
        std::cerr << f->get_name(x->d1) << " = " << f->get_name(x0->s1) << " "
                  << f->get_name(x0->s2) << "\n";
      }
      if (!defs.count(x0->s1)) std::cerr << *x0 << " bad s1\n";
      if (!defs.count(x0->s2)) std::cerr << *x0 << " bad s2\n";
      rns[x] = std::string("(") + dfs(defs.at(x0->s1)) + x0->op.get_name() +
               dfs(defs.at(x0->s2)) + ")";
    }
    else Case(LoadInstr, x0, x) {
      auto name = f->get_name(x->d1);
      rns[x] = name + " = *" + dfs(defs.at(x0->addr));
    }
    else {
      rns[x] = f->get_name(x->d1);
    }

    Case(LoadInstr, x0, x) { return f->get_name(x->d1); }
    return rns[x];
  };

  f->for_each([&](Instr *x) {
    Case(RegWriteInstr, x0, x) { dfs(x0); }
    else Case(StoreInstr, x0, x) {
      rns[x] = std::string("(*") + dfs(defs.at(x0->addr)) +
               ") = " + f->get_name(x0->s1);
    }
  });

  dbg << "```cpp\n" << *f << "```\n\n";

  rns.clear();
}
