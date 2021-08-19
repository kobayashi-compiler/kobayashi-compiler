#include "pass.hpp"

std::vector<BB *> get_BB_out(BB *w) {
  Instr *i = w->back();
  Case(JumpInstr, i0, i) return {i0->target};
  Case(BranchInstr, i0, i) return {i0->target1, i0->target0};
  return {};
}

void phi_src_rewrite(BB *bb_cur, BB *bb_old) {
  for (BB *u : get_BB_out(bb_cur)) {
    u->for_each([&](Instr *x) {
      Case(PhiInstr, x0, x) {
        for (auto &kv : x0->uses) {
          if (kv.second == bb_old) kv.second = bb_cur;
        }
      }
    });
  }
}

void func_inline(CompileUnit &c) {
  dbg << "## func inline\n";
  dbg << "```cpp\n" << c << "```\n\n";

  struct State {
    std::vector<NormalFunc *> pre;
    int deg = 0;
    bool recursive = 0;
  };
  std::unordered_map<NormalFunc *, State> S;
  c.for_each([&](NormalFunc *f) {
    f->for_each([&](Instr *x) {
      Case(CallInstr, y, x) {
        Case(NormalFunc, g, y->f) {
          // f call g
          if (g != f) {
            ++S[f].deg;
            S[g].pre.push_back(f);
          } else {
            S[f].recursive = true;
          }
        }
      }
    });
  });
  auto Inline = [&](NormalFunc *f) {
    dbg << "#### func inline: " << f->name << "\n";
    dbg << "```cpp\n" << *f << "```\n\n";

    struct InlineState {
      std::unordered_map<MemObject *, MemObject *> mp;
      int cnt = 0;
    };
    std::unordered_map<NormalFunc *, InlineState> IS;
    std::vector<BB *> _bbs;
    f->for_each([&](BB *_bb) { _bbs.push_back(_bb); });
    std::for_each(_bbs.cbegin(), _bbs.cend(), [&](BB *_bb) {
      for (BB *bb = _bb;;) {
        for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
          Case(CallInstr, call, it->get()) {
            Case(NormalFunc, g, call->f) {
              if (g != f && !S[g].recursive) {
                if (!IS.count(g)) {
                  g->scope.for_each([&](MemObject *x0, MemObject *x1) {
                    assert(!x1->global);
                    if (x1->arg) {
                      delete x1;
                    } else {
                      // alloc local vars
                      f->scope.add(x1);
                      IS[g].mp[x0] = x1;
                    }
                  });
                  dbg << "move " << g->name << " local var to " << f->name
                      << "\n";
                  dbg << "```cpp\n" << g->scope << "```\n\n";
                  dbg << "```cpp\n" << f->scope << "```\n\n";
                }
                auto &mp_mem = IS[g].mp;
                int _il_id = ++IS[g].cnt;
                std::string _il_name = g->name + std::to_string(_il_id);
                std::unordered_map<BB *, BB *> mp_bb;
                std::unordered_map<Reg, Reg> mp_reg;
                g->for_each([&](BB *bb) {
                  // alloc BBs
                  mp_bb[bb] = f->new_BB();
                  dbg << bb->name << " -> " << mp_bb[bb]->name << "\n";
                  bb->for_each([&](Instr *i) {
                    Case(RegWriteInstr, i0, i) {
                      Reg r = i0->d1;
                      assert(r.id);
                      if (!mp_reg.count(r))
                        mp_reg[r] = f->new_Reg(g->get_name(r) + "_" + _il_name);
                    }
                  });
                });
                BB *nxt = f->new_BB();
                nxt->instrs.splice(nxt->instrs.begin(), bb->instrs, ++it,
                                   bb->instrs.end());
                auto f1 = [&](Reg &x) {
                  if (!mp_reg.count(x)) {
                    std::cerr << "unknown reg: " << x << " " << g->get_name(x)
                              << "\n";
                    assert(0);
                  }
                  x = mp_reg.at(x);
                };
                auto f2 = [&](BB *&x) { x = mp_bb.at(x); };
                auto f3 = [&](MemObject *&x) {
                  if (!x->global) {
                    assert(!x->arg);
                    x = mp_mem.at(x);
                  }
                };
                g->for_each([&](BB *bb0) {
                  // copy all BBs
                  BB *bb1 = mp_bb.at(bb0);
                  bb0->for_each([&](Instr *x) {
                    Instr *y = NULL;
                    Case(LocalVarDef, x1, x) {
                      if (x1->data->arg) return;
                    }
                    Case(LoadArg, x1, x) {
                      auto y1 = new UnaryOpInstr(x1->d1, call->args.at(x1->id),
                                                 UnaryOp::ID);
                      f1(y1->d1);
                      y = y1;
                    }
                    else Case(ReturnInstr, x1, x) {
                      // copy return value
                      auto y1 = new UnaryOpInstr(call->d1, x1->s1, UnaryOp::ID);
                      f1(y1->s1);
                      bb1->push(y1);
                      y = new JumpInstr(nxt);
                    }
                    else {
                      y = x->map(f1, f2, f3);
                    }
                    bb1->push(y);
                  });
                });
                bb->pop();
                bb->push(new JumpInstr(mp_bb.at(g->entry)));  // call
                phi_src_rewrite(nxt, bb);
                bb = nxt;
                goto o;
              }
            }
          }
        }
        break;
      o:;
      }
    });
    dbg << "```cpp\n" << *f << "```\n\n";
  };
  std::queue<NormalFunc *> q;
  c.for_each([&](NormalFunc *f) {
    if (S[f].deg == 0) q.push(f);
  });
  size_t f_cnt = 0;
  while (!q.empty()) {
    auto f = q.front();
    q.pop();
    for (auto g : S[f].pre) {
      if (--S[g].deg == 0) q.push(g);
    }
    ++f_cnt;
    Inline(f);
  }
  assert(f_cnt == c.funcs.size());
  // co-recursive call not supportted
  remove_unused_func(c);
  c.for_each(dce_BB);
}
