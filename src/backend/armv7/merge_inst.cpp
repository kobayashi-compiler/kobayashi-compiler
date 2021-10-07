#include "backend/armv7/merge_inst.hpp"

#include <cassert>
#include <vector>
using std::function;
using std::make_unique;
using std::unique_ptr;
using std::vector;

namespace ARMv7 {

void merge_inst(Func *func, function<bool(Inst *)> check_def,
                function<unique_ptr<Inst>(Inst *, Reg, Inst *)> check_use) {
  func->build_def_use();
  for (int r = RegCount; r < func->reg_n; ++r) {
    if (func->reg_def[r].size() != 1) continue;
    OccurPoint def = *func->reg_def[r].begin();
    Inst *def_inst = def.it->get();
    if (def_inst->cond != Always) continue;
    if (!check_def(def_inst)) continue;
    bool flag = true;
    int max_pos = -1;
    vector<OccurPoint> uses;
    vector<unique_ptr<Inst>> replace;
    for (const OccurPoint &use : func->reg_use[r]) {
      unique_ptr<Inst> cur_replace = check_use(use.it->get(), Reg{r}, def_inst);
      if (!cur_replace) {
        flag = false;
        break;
      }
      uses.push_back(use);
      replace.push_back(set_cond(std::move(cur_replace), (*use.it)->cond));
      if (use.b == def.b) max_pos = std::max(max_pos, use.pos);
    }
    if (!flag) continue;
    for (Reg rely : (*def.it)->use_reg()) {
      for (const OccurPoint &rely_def : func->reg_def[rely.id])
        if (rely_def.b == def.b && rely_def.pos >= def.pos &&
            rely_def.pos <= max_pos) {
          flag = false;
          break;
        }
      if (!flag) break;
    }
    if (!flag) continue;
    for (size_t i = 0; i < uses.size(); ++i) {
      func->erase_def_use(uses[i], uses[i].it->get());
      *uses[i].it = std::move(replace[i]);
      func->add_def_use(uses[i], uses[i].it->get());
    }
  }
}

void merge_shift_binary_op(Func *func) {
  merge_inst(
      func, [](Inst *def) -> bool { return def->as<ShiftInst>(); },
      [](Inst *use, Reg r, Inst *def) -> unique_ptr<Inst> {
        ShiftInst *s = def->as<ShiftInst>();
        assert(s->dst == r);
        if (RegRegInst *b = use->as<RegRegInst>()) {
          if (b->op == RegRegInst::Mul || b->op == RegRegInst::Div)
            return nullptr;
          if (b->lhs == r && b->rhs == r) return nullptr;
          if (b->lhs != r && b->rhs != r) return nullptr;
          if (b->lhs == r) {
            if (b->shift.w != 0) return nullptr;
            switch (b->op) {
              case RegRegInst::Add:
                return make_unique<RegRegInst>(RegRegInst::Add, b->dst, b->rhs,
                                               s->src, s->shift);
              case RegRegInst::Sub:
                return make_unique<RegRegInst>(RegRegInst::RevSub, b->dst,
                                               b->rhs, s->src, s->shift);
              case RegRegInst::RevSub:
                return make_unique<RegRegInst>(RegRegInst::Sub, b->dst, b->rhs,
                                               s->src, s->shift);
              default:
                unreachable();
            }
          } else {  // b->rhs == r
            if (b->shift.w != 0) {
              if (b->shift.type != s->shift.type ||
                  !Shift::legal_width(b->shift.w + s->shift.w))
                return nullptr;
              Shift new_shift = s->shift;
              new_shift.w += b->shift.w;
              switch (b->op) {
                case RegRegInst::Add:
                  return make_unique<RegRegInst>(RegRegInst::Add, b->dst,
                                                 b->lhs, s->src, new_shift);
                case RegRegInst::Sub:
                  return make_unique<RegRegInst>(RegRegInst::RevSub, b->dst,
                                                 b->lhs, s->src, new_shift);
                case RegRegInst::RevSub:
                  return make_unique<RegRegInst>(RegRegInst::Sub, b->dst,
                                                 b->lhs, s->src, new_shift);
                default:
                  unreachable();
              }
            } else {
              switch (b->op) {
                case RegRegInst::Add:
                  return make_unique<RegRegInst>(RegRegInst::Add, b->dst,
                                                 b->lhs, s->src, s->shift);
                case RegRegInst::Sub:
                  return make_unique<RegRegInst>(RegRegInst::RevSub, b->dst,
                                                 b->lhs, s->src, s->shift);
                case RegRegInst::RevSub:
                  return make_unique<RegRegInst>(RegRegInst::Sub, b->dst,
                                                 b->lhs, s->src, s->shift);
                default:
                  unreachable();
              }
            }
          }
          unreachable();
          return nullptr;
        } else if (ComplexStore *cs = use->as<ComplexStore>()) {
          if (cs->base == r && cs->offset == r) return nullptr;
          if (cs->base != r && cs->offset != r) return nullptr;
          if (cs->offset == r) {
            if (cs->shift.w == 0)
              return make_unique<ComplexStore>(cs->src, cs->base, s->src,
                                               s->shift);
            else if (cs->shift.type == s->shift.type &&
                     Shift::legal_width(cs->shift.w + s->shift.w)) {
              Shift new_shift = s->shift;
              new_shift.w += cs->shift.w;
              return make_unique<ComplexStore>(cs->src, cs->base, s->src,
                                               new_shift);
            } else
              return nullptr;
          } else {  // cs->base == r
            if (cs->shift.w == 0)
              return make_unique<ComplexStore>(cs->src, cs->offset, s->src,
                                               s->shift);
            return nullptr;
          }
        } else if (ComplexLoad *l = use->as<ComplexLoad>()) {
          if (l->base == r && l->offset == r) return nullptr;
          if (l->base != r && l->offset != r) return nullptr;
          if (l->offset == r) {
            if (l->shift.w == 0)
              return make_unique<ComplexLoad>(l->dst, l->base, s->src,
                                              s->shift);
            else if (l->shift.type == s->shift.type &&
                     Shift::legal_width(l->shift.w + s->shift.w)) {
              Shift new_shift = s->shift;
              new_shift.w += l->shift.w;
              return make_unique<ComplexLoad>(l->dst, l->base, s->src,
                                              new_shift);
            } else
              return nullptr;
          } else {  // l->base == r
            if (l->shift.w == 0)
              return make_unique<ComplexLoad>(l->dst, l->offset, s->src,
                                              s->shift);
            return nullptr;
          }
        } else
          return nullptr;
      });
}

void merge_add_ldr_str(Func *func) {
  merge_inst(
      func,
      [](Inst *def) -> bool {
        if (RegRegInst *rr = def->as<RegRegInst>()) {
          return rr->op == RegRegInst::Add;
        } else if (RegImmInst *ri = def->as<RegImmInst>()) {
          return ri->op == RegImmInst::Add || ri->op == RegImmInst::Sub;
        } else
          return false;
      },
      [](Inst *use, Reg r, Inst *def) -> unique_ptr<Inst> {
        if (RegRegInst *rr = def->as<RegRegInst>()) {
          assert(rr->dst == r);
          if (Load *l = use->as<Load>()) {
            if (l->offset_imm == 0 && l->base == r)
              return make_unique<ComplexLoad>(l->dst, rr->lhs, rr->rhs,
                                              rr->shift);
            else
              return nullptr;
          } else if (Store *s = use->as<Store>()) {
            if (s->offset_imm == 0 && s->base == r)
              return make_unique<ComplexStore>(s->src, rr->lhs, rr->rhs,
                                               rr->shift);
            else
              return nullptr;
          } else
            return nullptr;
        } else {
          RegImmInst *ri = def->as<RegImmInst>();
          assert(ri->dst == r);
          Reg base = ri->lhs;
          int64_t offset = ri->rhs;
          if (ri->op == RegImmInst::Sub) offset = -offset;
          if (Load *l = use->as<Load>()) {
            if (l->base == r && load_store_offset_range(offset + l->offset_imm))
              return make_unique<Load>(
                  l->dst, base, static_cast<int32_t>(offset + l->offset_imm));
            else
              return nullptr;
          } else if (Store *s = use->as<Store>()) {
            if (s->base == r && load_store_offset_range(offset + s->offset_imm))
              return make_unique<Store>(
                  s->src, base, static_cast<int32_t>(offset + s->offset_imm));
            else
              return nullptr;
          } else
            return nullptr;
        }
      });
}

}  // namespace ARMv7