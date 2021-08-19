#include "pass.hpp"

void ind_var_discovery(NormalFunc *f) {
  // TODO: debug
  dbg << "## ind var discovery: " << f->name << "\n";
  auto S = build_dom_tree(f);
  auto defs = build_defs(f);
  auto i2bb = build_in2bb(f);
  std::unordered_set<Reg> la;
  std::unordered_map<Reg, Reg> mp_reg;
  dom_tree_dfs(S, [&](BB *w) {
    w->for_each([&](Instr *i) {
      Case(LoadAddr, i0, i) { la.insert(i0->d1); }
      else Case(BinaryOpInstr, i0, i) {
        if (la.count(i0->s1) || la.count(i0->s2)) la.insert(i0->d1);
      }
    });
  });
  dom_tree_dfs(S, [&](BB *w) {
    auto &sw = S[w];
    if (sw.loop_rt) {
      auto get_BB = [&](Reg r) { return i2bb[defs[r]]; };
      auto is_c = [&](Reg r) { return r.id && S[get_BB(r)].sdom(sw); };
      std::unordered_map<Reg, IndVarInfo> indvar;
      for (auto it = w->instrs.begin(); it != w->instrs.end(); ++it) {
        Instr *i = it->get();
        Case(PhiInstr, phi, i) {
          if (phi->uses.size() != 2) continue;
          auto u1 = phi->uses[0];
          auto u2 = phi->uses[1];
          if (!is_c(u1.first)) std::swap(u1, u2);
          if (!is_c(u1.first)) continue;
          Case(BinaryOpInstr, bop, defs[u2.first]) {
            auto op = bop->op.type;
            if (op == BinaryOp::ADD || op == BinaryOp::SUB)
              if (bop->s1 == phi->d1 && is_c(bop->s2)) {
                // phi.d1 = u1.first + k * bop.s2
                auto &s = indvar[phi->d1];
                s.base = u1.first;
                s.step = bop->s2;
                s.op = op;
                BB *bb1 = get_BB(s.base);
                BB *bb2 = get_BB(s.step);
                s.bb = S[bb1].dom(S[bb2]) ? bb2 : bb1;

                auto &iv2 = s;
                dbg << "basic ind var " << f->get_name(iv2.base) << ","
                    << f->get_name(iv2.step) << " : " << f->get_name(bop->d1)
                    << "\n";
              }
          }
        }
        else Case(BinaryOpInstr, bop, i) {
          if (bop->op.type == BinaryOp::MUL || bop->op.type == BinaryOp::ADD) {
            auto ind = bop->s1, step = bop->s2;
            if (!(indvar.count(ind) && is_c(step))) std::swap(ind, step);
            if (!(indvar.count(ind) && is_c(step))) continue;

            if (bop->op.type == BinaryOp::ADD) {
              // only rewrite as indvar for array index compute
              if (!la.count(bop->s1) && !la.count(bop->s2)) continue;
            }

            auto &iv1 = indvar[ind];
            auto &iv2 = indvar[bop->d1];
            iv2.base = f->new_Reg();
            iv2.op = iv1.op;

            Reg base1 = f->new_Reg();
            Reg base2 = f->new_Reg();
            mp_reg[bop->d1] = base1;

            BB *bb = iv2.bb = iv1.bb;
            if (bop->op.type == BinaryOp::MUL) {
              iv2.step = f->new_Reg();
              iv1.bb->push1(
                  new BinaryOpInstr(iv2.base, iv1.base, step,
                                    BinaryOp::MUL));  // base = base0 * step
              iv1.bb->push1(
                  new BinaryOpInstr(iv2.step, iv1.step, step,
                                    BinaryOp::MUL));  // step = step0 * step
            } else {
              iv2.step = iv1.step;
              iv1.bb->push1(
                  new BinaryOpInstr(iv2.base, iv1.base, step,
                                    BinaryOp::ADD));  // base = base0 + step
            }

            dbg << "ind var " << f->get_name(iv2.base) << ","
                << f->get_name(iv2.step) << " : " << f->get_name(base1) << ","
                << f->get_name(base2) << "\n";

            PhiInstr *phi = new PhiInstr(base1);
            w->push_front(phi);
            for (BB *u : sw.in) {
              phi->add_use(sw.dom(S[u]) ? base2 : iv2.base, u);
            }
            *it = std::unique_ptr<Instr>(new BinaryOpInstr(
                base2, base1, iv2.step, iv2.op));  // base += step
          }
        }
      }
    }
  });

  map_use(f, mp_reg);

  remove_unused_def(f);
}
