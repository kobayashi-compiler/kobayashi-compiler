#include "common/common.hpp"
#include "optimizer/pass.hpp"

void for_each_phi(BB *w, std::function<void(PhiInstr *)> F) {
  w->for_each([&](Instr *i) { Case(PhiInstr, phi, i) F(phi); });
}

namespace ExprUtil {

struct MulExpr : Printable {
  std::multiset<Reg> terms;
  MulExpr(Reg r) { terms.insert(r); }
  // t1*t2*...*tn*c
  bool operator<(const MulExpr &w) const { return terms < w.terms; }
  MulExpr operator*(const MulExpr &w) const {
    MulExpr res = *this;
    res.terms.insert(w.terms.begin(), w.terms.end());
    return res;
  }
  size_t size() const { return terms.size(); }
  void print(std::ostream &os) const override {
    bool flag = 0;
    for (Reg r : terms) {
      if (flag) os << "*";
      os << r;
      flag = 1;
    }
  }
};

struct AddExpr : Printable {
  std::map<MulExpr, int64_t> terms;
  int64_t c = 0;
  AddExpr() {}
  AddExpr(Reg r) { terms[MulExpr(r)] = 1; }
  AddExpr(int64_t value) { c = value; }
  bool is_zero() const { return !c && terms.empty(); }
  bool is_const() const { return terms.empty(); }
  bool is_const(std::function<bool(Reg)> f) const {
    for (auto &kv : terms) {
      for (Reg r : kv.first.terms)
        if (!f(r)) return 0;
    }
    return 1;
  }
  // t1+t2+...+tn+c
  AddExpr operator+(const AddExpr &w) const { return add(w, 1); }
  AddExpr operator-(const AddExpr &w) const { return add(w, -1); }
  AddExpr operator-() const { return add(AddExpr(), -1); }
  AddExpr operator*(int64_t x) const { return *this * AddExpr(x); }
  AddExpr operator*(const AddExpr &w) const {
    AddExpr res;
    for (auto &kv1 : terms) {
      res.terms[kv1.first] += kv1.second * w.c;
      for (auto &kv2 : w.terms) {
        res.terms[kv1.first * kv2.first] += kv1.second * kv2.second;
      }
    }
    for (auto &kv2 : w.terms) {
      res.terms[kv2.first] += c * kv2.second;
    }
    for (auto it = res.terms.begin(); it != res.terms.end();) {
      if (it->second)
        ++it;
      else {
        auto del = it++;
        res.terms.erase(del);
      }
    }
    res.c += c * w.c;
    return res;
  }
  std::optional<std::pair<AddExpr, AddExpr>> as_linear(Reg r) const {
    std::pair<AddExpr, AddExpr> res;
    for (auto &kv : terms) {
      int x = kv.first.terms.count(r);
      if (x == 0)
        res.first.terms[kv.first] = kv.second;
      else if (x == 1) {
        auto key = kv.first;
        key.terms.erase(r);
        if (key.terms.empty())
          res.second.c = kv.second;
        else
          res.second.terms[key] = kv.second;
      } else
        return std::nullopt;
    }
    res.first.c = c;
    return res;
  }
  AddExpr add(const AddExpr &w, int64_t k) const {
    AddExpr res = *this;
    for (auto &kv : w.terms) {
      if (!(res.terms[kv.first] += kv.second * k)) {
        res.terms.erase(res.terms.find(kv.first));
      }
    }
    res.c += w.c * k;
    return res;
  }
  size_t size() const {
    size_t s = 0;
    for (auto &kv : terms) s += kv.first.size();
    return s;
  }
  void print(std::ostream &os) const override {
    for (auto &kv1 : terms) {
      os << kv1.first;
      if (kv1.second != 1) os << "*" << kv1.second;
      os << " + ";
    }
    if (c || terms.empty()) os << c;
  }
};

struct ArrayIndexExpr : Printable {
  std::vector<std::optional<AddExpr>> terms;
  ArrayIndexExpr operator-(const ArrayIndexExpr &w) const {
    size_t n = terms.size();
    assert(n == w.terms.size());
    ArrayIndexExpr res;
    for (size_t i = 0; i < n; ++i) {
      if (terms[i] && w.terms[i])
        res.terms.emplace_back(*terms[i] - *w.terms[i]);
      else
        res.terms.emplace_back();
    }
    return res;
  }
  // mem[t1][t2]...[tn]
  void print(std::ostream &os) const override {
    for (const auto &x : terms) {
      os << "[";
      if (x)
        os << *x;
      else
        os << "(null)";
      os << "]";
    }
  }
};

}  // namespace ExprUtil

// note: only work on array-ssa
std::unordered_map<BB *, LoopInfo> check_loop(NormalFunc *f, int type) {
  SetPrintContext _(f);
  dbg << "## check loop: " << f->name << "\n";

  auto S = build_dom_tree(f);
  auto defs = build_defs(f);
  auto i2bb = build_in2bb(f);
  auto A = infer_array_regs(f);
  print_cfg(S, f);

  std::unordered_map<BB *, LoopInfo> LI;

  std::function<void(BB *)> dfs;
  dfs = [&](BB *w) {
    auto &sw = S[w];
    assert(sw.loop_rt);
    for (BB *u : sw.loop_ch) {
      if (S[u].loop_rt) dfs(u);
    }

    auto &liw = LI[w];

    auto chk = [&](BB *u) {
      u->for_each([&](Instr *i) {
        Case(MemUse, i0, i) { liw.rw[i0->data].r_any = 1; }
        else Case(MemEffect, i0, i) {
          liw.rw[i0->data].w_any = 1;
        }
        else Case(MemRead, i0, i) {
          liw.rw[i0->data].r_addr.insert(i0->addr);
        }
        else Case(MemWrite, i0, i) {
          liw.rw[i0->data].w_addr.insert(i0->addr);
        }
        else Case(MemDef, i0, i) {
          // TODO
        }
      });
    };

    for (BB *u : sw.loop_ch) {
      if (S[u].loop_rt) {
        auto &liu = LI.at(u);
        for (auto &kv : liu.rw) {
          liw.rw[kv.first].update(kv.second);
        }
      } else {
        chk(u);
      }
    }
    chk(w);

    if (!sw.loop_simple) return;
    BB *pre_exit = sw.loop_pre_exit, *exit = *sw.loop_exit.begin();

    if (pre_exit != w) return;  // TODO

    liw.simple = 1;

    auto is_c = [&](Reg r) { return S.at(i2bb.at(defs.at(r))).sdom(sw); };

    std::unordered_map<Reg, int> loop_uses;
    loop_tree_for_each(S, w, [&](BB *u) { get_use_count(loop_uses, u); });

    w->for_each([&](Instr *i) {
      Case(PhiInstr, phi, i) {
        Reg r = phi->d1;
        auto &var = liw.loop_var[r];
        var.mem = A.at(r);
        assert(phi->uses.size() == 2);
        if (!var.mem) {
          auto &u0 = phi->uses.at(0);
          auto &u1 = phi->uses.at(1);
          if (S[u0.second].sdom(sw)) std::swap(u0, u1);
          assert(in_loop(S, u0.second, w));
          if (in_loop(S, u1.second, w)) {
            std::cerr << *w << std::endl;
            std::cerr << sw.loop_pre->name << std::endl;
            std::cerr << sw.loop_last->name << std::endl;
            std::cerr << *u0.second << std::endl;
            std::cerr << *u1.second << std::endl;
          }
          assert(!in_loop(S, u1.second, w));
          // u0: last loop
          // u1: before loop
          Reg r0 = u0.first;
          auto i0 = defs.at(r0);
          Case(BinaryOpInstr, bop, i0) {
            switch (bop->op.type) {
              case BinaryOp::ADD:
              case BinaryOp::SUB:
              case BinaryOp::MUL:
              case BinaryOp::DIV: {
                Reg s1 = bop->s1, s2 = bop->s2;
                if (s2 == r && bop->op.comm()) std::swap(s1, s2);
                if (s1 == r) {
                  // r0 = r op s2
                  // r = phi(r0,c)
                  var.reduce.base = u1.first;
                  var.reduce.step = s2;
                  var.reduce.op = bop->op.type;
                  if (is_c(s2)) {
                    var.reduce.ind_var = 1;
                  } else {
                    var.reduce.reduce_var =
                        (loop_uses.at(r0) == 1 && loop_uses.at(r) == 1) &&
                        (bop->op.type == BinaryOp::ADD);
                  }
                }
                break;
              }
            }
          }
        }
      }
    });

    Case(BranchInstr, i0, pre_exit->back()) {
      bool rev = 0;
      if (i0->target1 == exit) {
        rev = 1;  // while(s1 !op s2)
      } else if (i0->target0 == exit) {
        rev = 0;  // while(s1 op s2)
      } else
        assert(0);
      Case(BinaryOpInstr, bop, defs.at(i0->cond)) {
        Reg s1 = bop->s1, s2 = bop->s2;
        auto tp = bop->op.type;
        if (rev) {
          if (tp == BinaryOp::LESS)
            tp = BinaryOp::LEQ;
          else if (tp == BinaryOp::LEQ)
            tp = BinaryOp::LESS;
          std::swap(s1, s2);
          rev = 0;
        }
        if (is_c(s1)) {
          std::swap(s1, s2);
          rev = 1;
        }
        if (is_c(s2)) {
          if (liw.loop_var.count(s1)) {
            auto &var = liw.loop_var.at(s1);
            if (var.reduce.ind_var) {
              // while(rev ^ (s1 op c2))
              switch (tp) {
                case BinaryOp::LESS:
                case BinaryOp::LEQ:
                  liw.simple_cond = SimpleCond{tp, rev, s1, s2};
                  break;
              }
            }
          }
        }
      }
    }
    else assert(0);

    if (type == 2)
      if (liw.simple_cond) {
        dbg << "```cpp\n";
        // is parallel for?
        liw.parallel_for = 1;

        using namespace ExprUtil;
        std::unordered_map<Reg, std::optional<AddExpr>> add_expr;
        std::unordered_map<Reg, ArrayIndexExpr> ai_expr;
        std::function<const std::optional<AddExpr> &(Reg)> get_expr;
        auto _get_expr = [&](Reg r) -> std::optional<AddExpr> & {
          auto &ret = add_expr[r];
          auto i = defs.at(r);
          Case(LoadConst, i0, i) { return ret = AddExpr(i0->value); }
          if (!is_c(r)) Case(BinaryOpInstr, i0, i) {
              auto s1 = get_expr(i0->s1), s2 = get_expr(i0->s2);
              if (s1 && s2) switch (i0->op.type) {
                  case BinaryOp::ADD:
                    return ret = (*s1) + (*s2);
                  case BinaryOp::SUB:
                    return ret = (*s1) - (*s2);
                  case BinaryOp::MUL:
                    return ret = (*s1) * (*s2);
                }
            }
          return ret = AddExpr(i->d1);
        };
        get_expr = [&](Reg r) -> const std::optional<AddExpr> & {
          if (add_expr.count(r)) {
            return add_expr.at(r);
          }
          auto &ret = _get_expr(r);
          dbg << ">>> " << *defs.at(r) << ": " << ret << "\n";
          if (ret && ret->size() > 16) {
            ret = std::nullopt;
            // dbg<<" [del]";
          }
          return ret;
        };

        auto &cond = *liw.simple_cond;

        Reg ind = cond.ind;
        Reg base = liw.loop_var.at(ind).reduce.base;
        Reg step = liw.loop_var.at(ind).reduce.step;
        Reg limit = cond.c;

        if (liw.loop_var.at(ind).reduce.op != BinaryOp::ADD)
          liw.parallel_for = 0;

        dbg << "ind: " << ind << "\n";
        dbg << "base: " << base << "\n";
        dbg << "step: " << step << "\n";
        dbg << "limit: " << limit << "\n";

        auto o_base_e = get_expr(base);

        auto base_e = get_expr(base).value();
        auto step_e = get_expr(step).value();
        auto limit_e = get_expr(limit).value();
        AddExpr loop_range;

        switch (cond.op) {
          case BinaryOp::LESS: {
            if (cond.rev) {
              //>=
              loop_range = base_e - limit_e;
            } else {
              //<
              loop_range = limit_e - base_e - AddExpr(1);
            }
            break;
          }
          case BinaryOp::LEQ: {
            if (cond.rev) {
              //>
              loop_range = base_e - limit_e - AddExpr(1);
            } else {
              //<=
              loop_range = limit_e - base_e;
            }
            break;
          }
          default:
            assert(0);
        }

        auto get_array_index_expr = [&](Reg r) -> const ArrayIndexExpr & {
          if (ai_expr.count(r)) {
            return ai_expr.at(r);
          }
          auto i = defs.at(r);
          ArrayIndexExpr &res = ai_expr[r];
          for (;;) {
            Case(LoadAddr, i0, i) break;
            Case(LoadArg, i0, i) break;
            Case(ArrayIndex, i0, i) {
              res.terms.emplace_back(get_expr(i0->s2));
              i = defs.at(i0->s1);
            }
            else assert(0);
          }
          return res;
        };

        for (auto &kv : liw.loop_var) {
          Reg r = kv.first;
          auto &var = kv.second;
          if (var.mem) {
            dbg << r << ": mem " << *var.mem << "\n";
            auto &rw = liw.rw.at(var.mem);
            if (rw.w_any || (rw.r_any && rw.w_addr.size())) {
              liw.parallel_for = 0;
              break;
            }
            auto add_cond = [&](Reg r1, Reg r2) {
              const auto &v1 = get_array_index_expr(r1);
              const auto &v2 = get_array_index_expr(r2);
              size_t n = v1.terms.size();
              dbg << "check: " << r1 << "," << r2 << "\n";
              dbg << r1 << ": " << v1 << "\n";
              dbg << r2 << ": " << v2 << "\n";
              bool flag = 0;
              for (size_t i = 0; i < n; ++i) {
                const auto &t1 = v1.terms.at(i);
                const auto &t2 = v2.terms.at(i);
                if (t1 && t2)
                  for (auto &kv : liw.loop_var) {
                    if (!kv.second.reduce.ind_var) continue;
                    Reg r = kv.first;
                    auto l1 = t1->as_linear(r);
                    auto l2 = t2->as_linear(r);
                    if (l1 && l2) {
                      auto c1 = l1->first - l2->first;
                      auto c2 = l1->second - l2->second;
                      auto &_a = l1->first;
                      auto &_b = l1->second;
                      auto &_c = l2->first;
                      if (!c2.is_zero()) continue;
                      if (!_b.is_const(is_c)) continue;

                      if (!_b.is_zero()) {
                        if (_a.is_const(is_c) && _c.is_const(is_c) &&
                            _b.is_const()) {
                          if (step_e.is_const() && c1.is_const() &&
                              std::abs(c1.c) < std::abs(step_e.c)) {
                            // b!=0, |a-c|<min|i-j| --> a+bi != c+bj
                            dbg << "case 1: ind " << r << "\n";
                            flag = 1;
                          } /*else{
                                   // b!=0, |a-c|>b*max|i-j| --> a+bi != c+bj
                                   auto
                           abs_loop_range=loop_range*std::abs(_b.c); auto
                           d1=c1-abs_loop_range; auto d2=-c1-abs_loop_range;
                                   dbg<<"abs_loop_range:
                           "<<abs_loop_range<<"\n"; dbg<<"d1: "<<d1<<"\n";
                                   dbg<<"d2: "<<d2<<"\n";
                                   if(d1.is_const()&&d1.c>=0||d2.is_const()&&d2.c>=0){
                                           dbg<<"case 3: ind "<<r<<"\n";
                                           flag=1;
                                   }
                           }*/
                            // abs(a-c)<abs(step)*abs(b); TODO
                            // ||
                            // abs(a-c)>abs(loop_range)*abs(b); TODO
                        }
                      } else {
                        // b=0, a-c!=0 --> a+bi != c+bj
                        if (c1.is_const() && !c1.is_zero() &&
                            _a.is_const(is_c)) {
                          dbg << "case 2: ind " << r << "\n";
                          flag = 1;
                        }
                      }
                    }
                  }
              }
              if (!flag) liw.parallel_for = 0;
            };
            for (Reg wa1 : rw.w_addr) {
              for (Reg wa2 : rw.w_addr) {
                add_cond(wa1, wa2);
                if (wa1 == wa2) break;
              }
              for (Reg ra1 : rw.r_addr) {
                if (!rw.w_addr.count(ra1)) add_cond(wa1, ra1);
              }
            }
          } else {
            dbg << r << ": int " << var.reduce.ind_var << " "
                << var.reduce.reduce_var << "\n";
            if (!var.reduce.ind_var && !var.reduce.reduce_var) {
              liw.parallel_for = 0;
              break;
            }
          }
        }
        if (liw.parallel_for) {
          auto os = dbg.sync(std::cout);
          os << "\x1b[34m\n";
          if (liw.parallel_for)
            os << "parallel for: " << w->name << "\n";
          else
            os << "simple cond for: " << w->name << "\n";

          os << "depth: " << S[w].loop_depth << "\n";
          for (BB *u : S[w].loop_ch) {
            if (S[u].loop_rt) os << "child: " << u->name << "\n";
          }
          if (S[w].loop_fa) os << "parent: " << S[w].loop_fa->name << "\n";

          auto &cond = liw.simple_cond.value();
          os << (cond.ind) << " " << (cond.rev ? "!" : "") << BinaryOp(cond.op)
             << " " << get_expr(cond.c) << "\n";
          for (auto &kv : liw.loop_var) {
            Reg r = kv.first;
            auto &var = kv.second;
            os << f->get_name(r) << ": ";
            if (var.mem)
              os << "mem " << *var.mem;
            else {
              bool flag = 1;
              if (var.reduce.ind_var)
                os << "ind";
              else if (var.reduce.reduce_var)
                os << "reduce";
              else
                flag = 0;
              if (flag) {
                os << " (";
                os << "base: " << get_expr(var.reduce.base) << " ";
                os << "step: " << get_expr(var.reduce.step) << " ";
                os << "op: " << BinaryOp(var.reduce.op) << " ";
                os << ")";
              }
            }
            os << "\n";
          }
          os << "\x1b[0m\n";
        }
        dbg << "```\n\n";
      }

    /*for(auto &kv:liw.loop_var){
            Reg cur=kv.first;

            std::unordered_set<Reg> visited;
            std::function<void(Reg)> find_dep;
            find_dep=[&](Reg r){
                    if(!visited.insert(r).second)return;
                    auto i=defs.at(r);
                    if(S.at(i2bb.at(i)).sdom(sw))return;
                    i->map_use(find_dep);
            };
            find_dep(cur);
    }*/
  };

  f->for_each([&](BB *w) {
    auto &sw = S[w];
    if (sw.loop_rt && !sw.loop_fa) dfs(w);
  });

  return LI;
}

std::unordered_map<BB *, double> estimate_BB_prob(NormalFunc *f) {
  auto S = build_dom_tree(f);
  std::unordered_map<BB *, double> f1, f2;
  f->for_each([&](BB *w) { f1[w] = 1; });
  for (int t = 0; t < 10; ++t) {
    f2.clear();
    const double decay = 0.5, break_prob = 0.15, if_prob = 0.7;
    f->for_each([&](BB *w) {
      BB *cur_loop = S[w].get_loop_rt();
      double p = f1[w];
      f2[w] += p * decay;
      p *= (1 - decay);
      Instr *x = w->back();
      Case(JumpInstr, y, x) { f2[y->target] += p; }
      else Case(BranchInstr, y, x) {
        double p1 = p * 0.5, p0 = p * 0.5;
        bool b1 = in_loop(S, y->target1, cur_loop);
        bool b0 = in_loop(S, y->target0, cur_loop);
        if (b1 != b0) {
          if (b1)
            p1 = p * (1 - break_prob), p0 = p * break_prob;
          else
            p1 = p * break_prob, p0 = p * (1 - break_prob);
        } else if (0) {
          b1 = (y->target1->instrs.empty());
          b0 = (y->target1->instrs.empty());
          if (b1 != b0) {
            if (b1)
              p1 = p * (1 - if_prob), p0 = p * if_prob;
            else
              p1 = p * if_prob, p0 = p * (1 - if_prob);
          }
        }
        f2[y->target1] += p1;
        f2[y->target0] += p0;
      }
      else Case(ReturnInstr, y, x) {
        f2[f->entry] += p;
      }
      else assert(0);
    });
    f1 = std::move(f2);
  }
  return f1;
}

void loop_unroll(const std::unordered_map<BB *, LoopInfo> &LI, NormalFunc *f) {
  auto Const = build_const_int(f);
  auto S = build_dom_tree(f);
  std::vector<BB *> bbs;
  f->for_each([&](BB *w) {
    auto &sw = S[w];
    if (!sw.loop_rt) return;
    if (!sw.loop_simple) return;
    for (BB *u : sw.loop_ch) {
      auto &su = S[u];
      if (su.loop_rt) return;
    }
    if (w->name.find("_dw2w") != std::string::npos) return;
    assert(S[sw.loop_pre].out.size() == 1);
    bbs.push_back(w);
  });

  std::unordered_set<Reg> dup_reg;

  for (BB *w : bbs) {
    auto &sw = S[w];
    BB *entry = sw.loop_pre;
    BB *exit = *sw.loop_exit.begin();
    const auto &liw = LI.at(w);
    if (!(liw.simple_cond)) continue;
    if (w->for_each_until([&](Instr *x) {
          Case(MemWrite, x0, x) return 1;
          Case(MemEffect, x0, x) return 1;
          Case(StoreInstr, x0, x) return 1;
          Case(CallInstr, x0, x) return 1;
          return 0;
        }))
      continue;

    const auto &cond = liw.simple_cond.value();
    const auto &ind = liw.loop_var.at(cond.ind).reduce;
    assert(ind.ind_var);
    if (!Const.count(ind.step)) continue;
    if (std::abs(Const.at(ind.step)) > 2) continue;

    bool type1 = (ind.op == BinaryOp::ADD);
    bool type2 = 0;
    PassIsEnabled("fixed-loop-unroll") type2 =
        (Const.count(ind.base) && Const.count(cond.c));

    int UNROLL_CNT = 4;
    int tot_instr = 0, BB_cnt = 0;
    loop_tree_for_each(S, w, [&](BB *bb0) {
      tot_instr += bb0->instrs.size() + 1;
      BB_cnt += 1;
    });

    if (type2) {
      int i = Const.at(ind.base);
      int c = Const.at(cond.c);
      int step = Const.at(ind.step);
      int cnt = 0;
      while (cnt <= 33) {
        int s1 = i, s2 = c;
        if (cond.rev) std::swap(s1, s2);
        if (!BinaryOp(cond.op).compute(s1, s2)) break;
        i = BinaryOp(ind.op).compute(i, step);
        ++cnt;
      }
      if (cnt < 1 || cnt > 33 || tot_instr * cnt > 1000)
        type2 = 0;
      else {
        UNROLL_CNT = cnt;
        type1 = 0;
      }
    }

    if (type1) {
      UNROLL_CNT = 4;
      if (tot_instr > 20 || BB_cnt > 2) UNROLL_CNT = 2;
      if (tot_instr > 200) type1 = 0;
    }

    if (!type1 && !type2) continue;

    {
      auto os = dbg.sync(std::cout);
      os << "\x1b[34m\n";
      os << "unroll for: " << w->name << "\n";
      if (type1) os << "type1: " << UNROLL_CNT << "\n";
      if (type2) {
        os << "type2: " << UNROLL_CNT << "\n";
        os << Const.at(ind.base) << " " << Const.at(ind.step)
           << BinaryOp(ind.op) << " " << Const.at(cond.c) << "\n";
      }
      auto &cond = liw.simple_cond.value();
      // os<<(cond.ind)<<" "<<(cond.rev?"!":"")<<BinaryOp(cond.op)<<"
      // "<<get_expr(cond.c)<<"\n";
      os << "\x1b[0m\n";
    }

    std::vector<std::unordered_map<BB *, BB *>> mp_bb(UNROLL_CNT);

    BB *new_entry2 = f->new_BB("NewEntry");
    BB *new_entry = f->new_BB("newEntry");
    BB *new_exit = f->new_BB("newExit");
    BB *new_exit2 = f->new_BB("NewExit");

    for (int i = 0; i < UNROLL_CNT; ++i) {
      loop_tree_for_each(S, w, [&](BB *bb0) {
        mp_bb[i][bb0] = f->new_BB(bb0->name + "_" + std::to_string(i) + "_");
        mp_bb[i][bb0]->disable_schedule_early = 1;
      });
    }

    Reg unroll_cnt = f->new_Reg();
    Reg new_step = f->new_Reg();
    Reg new_c = f->new_Reg();

    new_entry2->push(new LoadConst(unroll_cnt, UNROLL_CNT));
    new_entry2->push(
        new BinaryOpInstr(new_step, ind.step, unroll_cnt, BinaryOp::MUL));
    new_entry2->push(new BinaryOpInstr(new_c, cond.c, new_step, BinaryOp::SUB));

    loop_tree_for_each(S, w, [&](BB *bb0) {
      for (int i = 0; i < UNROLL_CNT; ++i) {
        BB *bb1 = mp_bb[i].at(bb0);
        bb0->for_each([&](Instr *x) {
          Case(RegWriteInstr, x0, x) { dup_reg.insert(x0->d1); }
          Case(JumpInstr, x0, x) {
            if (x0->target == w) {
              if (type1) {
                bb1->push(new JumpInstr(mp_bb[(i + 1) % UNROLL_CNT].at(w)));
              } else if (type2) {
                if (i == UNROLL_CNT - 1) {
                  bb1->push(new JumpInstr(new_exit));
                } else {
                  bb1->push(new JumpInstr(mp_bb[i + 1].at(w)));
                }
              } else
                assert(0);
              return;
            }
          }
          if (bb0 == w) {
            Case(BranchInstr, x0, x) {
              BB *tg1 = x0->target1;
              BB *tg0 = x0->target0;
              if (type1) {
                if (i > 0) {
                  BB *tg = (tg1 == exit ? tg0 : tg1);
                  bb1->push(new JumpInstr(mp_bb[i].at(tg)));
                } else {
                  if (tg1 == exit)
                    tg1 = new_exit, tg0 = mp_bb[i].at(tg0);
                  else
                    tg0 = new_exit, tg1 = mp_bb[i].at(tg1);
                  Reg cond1 = f->new_Reg();
                  Reg s1 = cond.ind, s2 = new_c;
                  if (cond.rev) std::swap(s1, s2);
                  bb1->push(new BinaryOpInstr(cond1, s1, s2, cond.op));
                  bb1->push(new BranchInstr(cond1, tg1, tg0));
                }
              } else if (type2) {
                BB *tg = (tg1 == exit ? tg0 : tg1);
                bb1->push(new JumpInstr(mp_bb[i].at(tg)));
              } else
                assert(0);
              return;
            }
            Case(PhiInstr, phi, x) {
              if (i > 0) {
                for (auto &kv : phi->uses) {
                  if (kv.second != entry) {
                    assert(kv.second == sw.loop_last);
                    bb1->push(new UnaryOpInstr(phi->d1, kv.first, UnaryOp::ID));
                    return;
                  }
                }
                assert(0);
              } else if (type2) {
                for (auto &kv : phi->uses) {
                  if (kv.second == entry) {
                    bb1->push(new UnaryOpInstr(phi->d1, kv.first, UnaryOp::ID));
                    return;
                  }
                }
              }
            }
          }
          bb1->push(x->map([&](Reg &reg) {},
                           [&](BB *&bb) {
                             if (bb == entry)
                               bb = new_entry;
                             else
                               bb = mp_bb[i].at(bb);
                           },
                           [](MemObject *&) {}));
        });
      }
    });

    if (type1)
      for_each_phi(w, [&](PhiInstr *phi) {
        for (auto &kv : phi->uses) {
          if (kv.second == entry) {
            kv.first = phi->d1;
            kv.second = new_exit2;
          }
        }
      });

    if (type2) {
      for (auto it = w->instrs.begin(); it != w->instrs.end(); ++it) {
        Instr *x = it->get();
        Case(PhiInstr, phi, x) {
          Instr *x1 = NULL;
          for (auto &kv : phi->uses) {
            if (kv.second != entry) {
              assert(kv.second == sw.loop_last);
              x1 = new UnaryOpInstr(phi->d1, kv.first, UnaryOp::ID);
              break;
            }
          }
          assert(x1);
          *it = std::unique_ptr<Instr>(x1);
        }
        else Case(BranchInstr, x0, x) {
          Reg tmp = f->new_Reg();
          w->instrs.insert(it, std::unique_ptr<Instr>(
                                   new LoadConst(tmp, x0->target1 == exit)));
          x0->cond = tmp;
        }
      }
    }

    Reg s1 = ind.base, s2 = new_c;
    if (cond.rev) std::swap(s1, s2);
    Reg cond2 = f->new_Reg();
    // new_entry2->push(new BinaryOpInstr(cond2,s1,s2,cond.op));
    // new_entry2->push(new BranchInstr(cond2,new_entry,no_unroll));
    new_entry2->push(new JumpInstr(new_entry));

    new_entry->push(new JumpInstr(mp_bb[0].at(w)));

    // no_unroll->push(new JumpInstr(new_exit2));
    new_exit->push(new JumpInstr(new_exit2));

    new_exit2->push(new JumpInstr(w));

    entry->back()->map_BB(partial_map(w, new_entry2));

    BB *w0 = S[w].loop_last;
    auto f = partial_map(mp_bb[0].at(w0), mp_bb[UNROLL_CNT - 1].at(w0));
    for_each_phi(mp_bb[0].at(w), [&](PhiInstr *phi) { phi->map_BB(f); });
    w->map_BB(partial_map(entry, new_exit2));
  }
  {
    dbg << "## loop unroll: " << f->name << "\n";
    auto S = build_dom_tree(f);
    print_cfg(S, f);
  }
  ssa_construction(f, [&](Reg r) { return dup_reg.count(r); });
}

void seperate_loops(NormalFunc *f) {
  auto S = build_dom_tree(f);
  f->for_each([&](BB *w) {
    Case(BranchInstr, x0, w->back()) {
      if (S[x0->target1].loop_depth > S[w].loop_depth) assert(0);
      if (S[x0->target0].loop_depth > S[w].loop_depth) assert(0);
    }
  });
}

void loop_tree_dfs(DomTree &S, NormalFunc *f, std::function<void(BB *)> F) {
  std::vector<BB *> bbs;
  f->for_each([&](BB *w) {
    auto &sw = S[w];
    if (!sw.loop_rt) return;
    if (sw.loop_fa) return;
    bbs.push_back(w);
  });
  std::function<void(BB *)> dfs;
  dfs = [&](BB *bb) {
    F(bb);
    for (BB *ch : S[bb].loop_ch)
      if (S[ch].loop_rt) dfs(ch);
  };
  for (BB *bb : bbs) dfs(bb);
}

void loop_parallel(const std::unordered_map<BB *, LoopInfo> &LI, CompileUnit &c,
                   NormalFunc *f) {
  dbg << "## loop parallel: " << f->name << "\n";
  seperate_loops(f);
  auto os = dbg.sync(std::cout);
  os << "\x1b[31m\n";
  auto Const = build_const_int(f);
  auto S = build_dom_tree(f);

  std::unordered_set<Reg> new_reg;

  std::vector<BB *> new_exits;

  loop_tree_dfs(S, f, [&](BB *bb) {
    auto &liw = LI.at(bb);
    if (!liw.parallel_for) return;

    for (auto &kv : liw.loop_var) {
      if (kv.second.reduce.ind_var && kv.second.reduce.op != BinaryOp::ADD)
        return;
    }

    const auto &cond = liw.simple_cond.value();
    if (cond.rev) return;

    const auto &ind = liw.loop_var.at(cond.ind).reduce;
    assert(ind.ind_var);
    if (!Const.count(ind.step)) return;
    if (Const.at(ind.step) != 1) return;

    bool has_call = 0;
    loop_tree_for_each(S, bb, [&](BB *bb1) {
      bb1->for_each([&](Instr *x) {
        Case(CallInstr, call, x) { has_call = 1; }
      });
    });
    if (has_call) return;

    int cost = 1;
    for (BB *bb1 : S[bb].loop_ch) {
      if (S[bb1].loop_rt) {
        cost = 10;
      }
    }

    os << "parallel: " << bb->name << "\n";
    os << "cost: " << cost << "\n";

    int MIN_PAR_LOOP_TIMES =
        global_config.get_int_arg("min-par-loop-times", 1000);
    int THREAD_CNT = global_config.get_int_arg("num-thread", 4);

    MIN_PAR_LOOP_TIMES = std::max(THREAD_CNT + 1, MIN_PAR_LOOP_TIMES / cost);

    BB *entry = S[bb].loop_pre;
    BB *exit = *S[bb].loop_exit.begin();

    BB *new_entry = f->new_BB("NewEntry");
    BB *new_entry_par = f->new_BB("NewEntryPar");
    BB *new_entry_nopar = f->new_BB("NewEntryNoPar");
    std::vector<BB *> new_entry_par_2(THREAD_CNT), new_exit_par(THREAD_CNT);
    for (int i = 0; i < THREAD_CNT; ++i) {
      new_entry_par_2[i] = f->new_BB("new_entry_" + std::to_string(i) + "par");
      new_exit_par[i] = f->new_BB("NewExit" + std::to_string(i) + "Par");
    }

    BB *new_exit = f->new_BB("NewExit");

    new_exits.push_back(new_exit);

    Reg loop_times = f->new_Reg();
    Reg min_par_loop_times = f->new_Reg();
    Reg use_par = f->new_Reg();

    new_entry->push(
        new BinaryOpInstr(loop_times, cond.c, ind.base, BinaryOp::SUB));
    new_entry->push(new LoadConst(min_par_loop_times, MIN_PAR_LOOP_TIMES));
    new_entry->push(new BinaryOpInstr(use_par, min_par_loop_times, loop_times,
                                      BinaryOp::LESS));
    new_entry->push(new BranchInstr(use_par, new_entry_par, new_entry_nopar));

    new_entry_nopar->push(new JumpInstr(bb));

    std::vector<std::unordered_map<BB *, BB *>> mp_bb(THREAD_CNT);
    loop_tree_for_each(S, bb, [&](BB *w) {
      for (int i = 0; i < THREAD_CNT; ++i)
        mp_bb[i][w] = f->new_BB(w->name + "_" + std::to_string(i) + "_par");
      w->for_each([&](Instr *i) {
        Case(RegWriteInstr, i0, i) { new_reg.insert(i0->d1); }
      });
    });

    for (int i = 0; i < THREAD_CNT; ++i)
      for (auto &kv : mp_bb[i]) {
        BB *w = kv.first, *u = kv.second;
        if (w == exit) continue;
        w->for_each([&](Instr *i) {
          auto j = i->copy();
          u->push(j);
        });
        u->map_BB(partial_map(mp_bb[i]));
        u->map_BB(partial_map(entry, new_entry_par_2[i]));
        u->map_BB(partial_map(exit, new_exit_par[i]));
      }

    std::vector<BB *> new_w(THREAD_CNT);
    for (int i = 0; i < THREAD_CNT; ++i) new_w[i] = mp_bb[i].at(bb);

    Reg thread_id = f->new_Reg("thread_id");
    Reg new_c = f->new_Reg("new_c");
    Reg thread_cnt = f->new_Reg("thread_cnt");
    Reg block_size = f->new_Reg("block_size");

    new_reg.insert(new_c);

    new_entry_par->push(new LoadConst(thread_cnt, THREAD_CNT));
    new_entry_par->push(
        new BinaryOpInstr(block_size, loop_times, thread_cnt, BinaryOp::DIV));
    new_entry_par->push(new CallInstr(thread_id, c.lib_funcs.at(".fork").get(),
                                      {thread_cnt}, 0));

    std::vector<Reg> block_offset(THREAD_CNT);

    BB *cur = new_entry_par;
    for (int i = 0; i < THREAD_CNT; ++i) {
      BB *nep2 = new_entry_par_2[i];

      block_offset[i] = f->new_Reg("block_offset");
      Reg new_base = f->new_Reg("new_base");
      Reg thread_id_imm = f->new_Reg();
      nep2->push(new LoadConst(thread_id_imm, i));
      nep2->push(new BinaryOpInstr(block_offset[i], block_size, thread_id_imm,
                                   BinaryOp::MUL));

      if (i == THREAD_CNT - 1) {
        cur->push(new JumpInstr(nep2));
        nep2->push(new UnaryOpInstr(new_c, cond.c, UnaryOp::ID));
      } else {
        {
          BB *bb1 = f->new_BB();
          bb1->push(new JumpInstr(nep2));

          BB *bb2 = f->new_BB();
          Reg r1 = f->new_Reg();
          Reg r2 = f->new_Reg();
          cur->push(new LoadConst(r1, i));
          cur->push(new BinaryOpInstr(r2, thread_id, r1, BinaryOp::EQ));
          cur->push(new BranchInstr(r2, bb1, bb2));
          cur = bb2;
        }

        nep2->push(new BinaryOpInstr(new_base, ind.base, block_offset[i],
                                     BinaryOp::ADD));

        nep2->push(
            new BinaryOpInstr(new_c, new_base, block_size, BinaryOp::ADD));
        if (cond.op == BinaryOp::LEQ) {
          Reg c1 = f->new_Reg();
          nep2->push(new LoadConst(c1, 1));
          nep2->push(new BinaryOpInstr(new_c, new_c, c1, BinaryOp::SUB));
        }
      }
    }

    std::vector<Reg> thread_id_imm(THREAD_CNT);
    for (int i = 0; i < THREAD_CNT; ++i) {
      thread_id_imm[i] = f->new_Reg();
      new_exit_par[i]->push(new LoadConst(thread_id_imm[i], i));
    }

    for (int i = 0; i < THREAD_CNT; ++i)
      for_each_phi(new_w[i], [&](PhiInstr *phi) {
        /*if(phi->d1==cond.ind){
                for(auto &kv:phi->uses){
                        if(kv.second==new_entry_par_2){
                                assert(kv.first==ind.base);
                                kv.first=new_base;
                        }
                }
        }*/
        const auto &var = liw.loop_var.at(phi->d1).reduce;
        if (var.ind_var) {
          Reg r1 = f->new_Reg();
          Reg r2 = f->new_Reg();
          new_entry_par_2[i]->push(
              new BinaryOpInstr(r1, block_offset[i], var.step, BinaryOp::MUL));
          new_entry_par_2[i]->push(
              new BinaryOpInstr(r2, r1, var.base, BinaryOp::ADD));
          for (auto &kv : phi->uses) {
            if (kv.second == new_entry_par_2[i]) {
              assert(kv.first == var.base);
              kv.first = r2;
            }
          }

          if (i == THREAD_CNT - 1) {
            auto m0 = c.scope.new_MemObject(f->get_name(phi->d1) + "_ind");
            m0->size = 4;
            m0->global = 1;
            m0->arg = 0;
            m0->dims = {};

            {
              Reg r1 = f->new_Reg();
              new_exit_par[i]->push(new LoadAddr(r1, m0));
              new_exit_par[i]->push(new StoreInstr(r1, phi->d1));
            }
            {
              Reg r1 = f->new_Reg();
              new_exit->push(new LoadAddr(r1, m0));
              new_exit->push(new LoadInstr(phi->d1, r1));
            }
          }
        } else if (var.reduce_var) {
          Reg c0 = f->new_Reg();
          new_entry_par_2[i]->push(new LoadConst(c0, 0));
          for (auto &kv : phi->uses) {
            if (kv.second == new_entry_par_2[i]) {
              assert(kv.first == var.base);
              kv.first = c0;
            }
          }

          assert(var.op == BinaryOp::ADD);

          if (i == 0) {
            auto m0 = c.scope.new_MemObject(f->get_name(phi->d1) + "_reduce");
            m0->size = THREAD_CNT * 4;
            m0->global = 1;
            m0->arg = 0;
            m0->dims = {THREAD_CNT};

            for (int i = 0; i < THREAD_CNT; ++i) {
              Reg r1 = f->new_Reg();
              Reg r2 = f->new_Reg();
              new_exit_par[i]->push(new LoadAddr(r1, m0));
              new_exit_par[i]->push(
                  new ArrayIndex(r2, r1, thread_id_imm[i], 4, -1));
              new_exit_par[i]->push(new StoreInstr(r2, phi->d1));
            }

            new_exit->push(new UnaryOpInstr(phi->d1, var.base, UnaryOp::ID));

            Reg r1 = f->new_Reg();
            new_exit->push(new LoadAddr(r1, m0));

            for (int i = 0; i < THREAD_CNT; ++i) {
              Reg r2 = f->new_Reg();
              Reg r3 = f->new_Reg();
              Reg r4 = f->new_Reg();
              new_exit->push(new LoadConst(r2, i));
              new_exit->push(new ArrayIndex(r3, r1, r2, 4, -1));
              new_exit->push(new LoadInstr(r4, r3));
              new_exit->push(
                  new BinaryOpInstr(phi->d1, phi->d1, r4, BinaryOp::ADD));
            }
          }
        } else
          assert(0);
      });

    for (int i = 0; i < THREAD_CNT; ++i) {
      new_entry_par_2[i]->push(new JumpInstr(new_w[i]));

      new_exit_par[i]->push(new CallInstr(f->new_Reg(),
                                          c.lib_funcs.at(".join").get(),
                                          {thread_id_imm[i], thread_cnt}, 1));
      new_exit_par[i]->push(new JumpInstr(new_exit));
    }

    new_exit->push(new JumpInstr(exit));

    for (int i = 0; i < THREAD_CNT; ++i)
      Case(BranchInstr, br, new_w[i]->back()) {
        int cnt = 0;
        new_w[i]->for_each([&](Instr *i) {
          Case(BinaryOpInstr, i0, i) {
            if (i0->d1 == br->cond) {
              i0->map_use(partial_map(cond.c, new_c));
              ++cnt;
            }
          }
        });
        assert(cnt == 1);
      }
    else assert(0);

    entry->map_BB(partial_map(bb, new_entry));
    bb->map_BB(partial_map(entry, new_entry_nopar));
    assert(S[exit].in.size() == 1);
  });

  /*{
          dbg<<"## loop parallel end: "<<f->name<<"\n";
          auto S=build_dom_tree(f);
          print_cfg(S,f);
  }*/
  os << "\x1b[0m\n";
  ssa_construction(f, [&](Reg r) { return new_reg.count(r); });
  {
    dbg << "## loop parallel end: " << f->name << "\n";
    auto S = build_dom_tree(f);
    print_cfg(S, f);
  }
  for (BB *bb : new_exits) {
    for_each_phi(bb, [&](auto phi) { assert(0); });
  }
}

void do_while_to_while(NormalFunc *f) {
  if (f->bbs.size() > 100) return;
  {
    std::unordered_set<BB *> del;
    auto S = build_dom_tree(f);
    loop_tree_dfs(S, f, [&](BB *w) {
      auto &sw = S[w];
      if (!sw.loop_simple) return;
      if (w == sw.loop_pre_exit) return;
      if (S[sw.loop_last].out.size() != 1) return;
      if (S[sw.loop_last].in.size() != 1) return;
      if (sw.loop_last->instrs.size() != 1) return;
      if (!sw.loop_pre_exit) return;
      bool flag = 0;
      sw.loop_pre_exit->back()->map_BB([&](BB *&bb) {
        if (bb == sw.loop_last) {
          bb = w;
          flag = 1;
        }
      });
      if (!flag) return;
      Case(JumpInstr, x0, sw.loop_last->back()) { assert(x0->target == w); }
      else assert(0);
      /*{
                      auto os=dbg.sync(std::cout);
                      os<<"\x1b[34m\n";
                      os<<"normalize do while: "<<w->name<<"\n";
                      for(BB *pre:S[sw.loop_last].in){
                              os<<pre->name<<"\n";
                      }
                      os<<*sw.loop_last<<"\n";
                      os<<*sw.loop_pre_exit<<"\n";
                      os<<*w<<"\n";
                      os<<"\x1b[0m\n";
      }*/
      w->map_BB(partial_map(sw.loop_last, sw.loop_pre_exit));
      /*f->for_each([&](BB *bb0){
              bb0->for_each([&](Instr *i){
                      i->map_BB([&](BB *&bb1){
                              if(bb1==sw.loop_last){
                                      std::cout<<*bb0<<std::endl;
                                      assert(0);
                              }
                      });
              });
      });*/
      del.insert(sw.loop_last);
    });
    f->bbs.erase(remove_if(f->bbs.begin(), f->bbs.end(),
                           [&](auto &bb) { return del.count(bb.get()); }),
                 f->bbs.end());
  }

  auto S = build_dom_tree(f);
  auto defs = build_defs(f);
  std::unordered_set<Reg> dup_reg;

  loop_tree_dfs(S, f, [&](BB *w) {
    auto &sw = S[w];
    if (sw.loop_depth > 3) return;
    if (!sw.loop_simple) return;
    if (w == sw.loop_pre_exit) return;
    if (S[sw.loop_last].out.size() != 2) return;
    BB *w0 = S[w].loop_last;
    Case(BranchInstr, br, w0->back()) {
      Case(BinaryOpInstr, bop, defs.at(br->cond)) {
        if (bop->op.type == BinaryOp::LESS)
          ;
        else if (bop->op.type == BinaryOp::LEQ)
          ;
        else
          return;
      }
      else return;
    }
    else assert(0);

    {
      auto os = dbg.sync(std::cout);
      os << "\x1b[34m\n";
      os << "unroll do while: " << w->name << "\n";
      os << "\x1b[0m\n";
    }

    BB *entry = sw.loop_pre;
    BB *exit = *sw.loop_exit.begin();

    std::unordered_map<BB *, BB *> mp_bb;

    BB *new_entry2 = f->new_BB("dw2w_NewEntry");
    BB *new_entry = f->new_BB("dw2w_newEntry");
    BB *new_exit = f->new_BB("dw2w_newExit");
    BB *new_exit2 = f->new_BB("dw2w_NewExit");

    loop_tree_for_each(
        S, w, [&](BB *bb0) { mp_bb[bb0] = f->new_BB(bb0->name + "_dw2w"); });

    loop_tree_for_each(S, w, [&](BB *bb0) {
      BB *bb1 = mp_bb.at(bb0);
      bb0->for_each([&](Instr *x) {
        Case(RegWriteInstr, x0, x) { dup_reg.insert(x0->d1); }
        Case(BranchInstr, x0, x) {
          if (x0->target1 == exit || x0->target0 == exit) {
            bb1->push(new JumpInstr(new_exit));
            return;
          }
        }
        if (bb0 == w) {
          Case(PhiInstr, phi, x) {
            for (auto &kv : phi->uses) {
              if (kv.second == entry) {
                bb1->push(new UnaryOpInstr(phi->d1, kv.first, UnaryOp::ID));
                return;
              }
            }
          }
        }
        bb1->push(x->map([&](Reg &reg) {},
                         [&](BB *&bb) {
                           if (bb == entry)
                             bb = new_entry;
                           else
                             bb = mp_bb.at(bb);
                         },
                         [](MemObject *&) {}));
      });
    });

    new_entry2->push(new JumpInstr(new_entry));

    new_entry->push(new JumpInstr(mp_bb.at(w)));

    new_exit->push(new JumpInstr(new_exit2));

    entry->back()->map_BB(partial_map(w, new_entry2));

    for (auto it = w->instrs.begin(); it != w->instrs.end();) {
      bool del = 0;
      Case(PhiInstr, phi, it->get()) {
        Reg r;
        for (auto &kv : phi->uses) {
          if (kv.second != entry) {
            r = kv.first;
          }
        }
        assert(r.id);
        /*for(auto &kv:phi->uses){
                if(kv.second==entry){
                        kv.first=r;
                        kv.second=new_exit;
                }
        }*/
        // new_exit2->instrs.emplace_back(std::move(*it));
        new_exit2->push(new UnaryOpInstr(phi->d1, r, UnaryOp::ID));
        del = 1;
      }
      if (del) {
        auto it0 = it++;
        w->instrs.erase(it0);
      } else
        ++it;
    }
    Case(BranchInstr, br, w0->back()) {
      new_exit2->push(defs.at(br->cond)->copy());
    }
    else assert(0);
    new_exit2->instrs.emplace_back(std::move(w0->instrs.back()));
    w0->instrs.pop_back();
    w0->push(new JumpInstr(new_exit2));
    // std::cout<<*new_exit2<<std::endl;
    assert(S[exit].in.size() == 1);
  });

  {
    dbg << "## do while to while: " << f->name << "\n";
    auto S = build_dom_tree(f);
    print_cfg(S, f);
  }
  dbg << "ssa\n";
  ssa_construction(f, [&](Reg r) { return dup_reg.count(r); });
}
