#include "backend/rv32/backend_passes.hpp"

#include <limits>
#include <map>
#include <memory>
#include <set>
#include <vector>

#include "backend/rv32/inst.hpp"
#include "backend/rv32/program.hpp"

using std::make_unique;
using std::map;
using std::set;
using std::unique_ptr;
using std::vector;
namespace RV32 {

static map<int32_t, int> log2_map = []() {
  map<int32_t, int> ret;
  for (int i = 0; i < 31; ++i) ret[1 << i] = i;
  ret[std::numeric_limits<int32_t>::min()] = 31;
  return ret;
}();

static unique_ptr<Inst> optimize_reg_reg_inst(Func *func, RegRegInst *rr) {
  if (rr->op == RegRegInst::Add || rr->op == RegRegInst::And ||
      rr->op == RegRegInst::Or || rr->op == RegRegInst::Xor) {
    RegImmInst::Type new_op;
    switch (rr->op) {
      case RegRegInst::Add:
        new_op = RegImmInst::Addi;
        break;
      case RegRegInst::And:
        new_op = RegImmInst::Andi;
        break;
      case RegRegInst::Or:
        new_op = RegImmInst::Ori;
        break;
      case RegRegInst::Xor:
        new_op = RegImmInst::Xori;
        break;
      default:
        unreachable();
    }
    if (func->constant_reg.find(rr->lhs) != func->constant_reg.end()) {
      int32_t lv = func->constant_reg[rr->lhs];
      if (is_imm12(lv)) {
        return make_unique<RegImmInst>(new_op, rr->dst, rr->rhs, lv);
      }
    }
    if (func->constant_reg.find(rr->rhs) != func->constant_reg.end()) {
      int32_t rv = func->constant_reg[rr->rhs];
      if (is_imm12(rv)) {
        return make_unique<RegImmInst>(new_op, rr->dst, rr->lhs, rv);
      }
    }
  } else if (rr->op == RegRegInst::Sll || rr->op == RegRegInst::Srl ||
             rr->op == RegRegInst::Sra || rr->op == RegRegInst::Slt ||
             rr->op == RegRegInst::Sltu) {
    RegImmInst::Type new_op;
    switch (rr->op) {
      case RegRegInst::Sll:
        new_op = RegImmInst::Slli;
        break;
      case RegRegInst::Srl:
        new_op = RegImmInst::Srli;
        break;
      case RegRegInst::Sra:
        new_op = RegImmInst::Srai;
        break;
      case RegRegInst::Slt:
        new_op = RegImmInst::Slti;
        break;
      case RegRegInst::Sltu:
        new_op = RegImmInst::Sltiu;
        break;
      default:
        unreachable();
    }
    if (func->constant_reg.find(rr->rhs) != func->constant_reg.end()) {
      int32_t rv = func->constant_reg[rr->rhs];
      if (is_imm12(rv)) {
        return make_unique<RegImmInst>(new_op, rr->dst, rr->lhs, rv);
      }
    }
  } else if (rr->op == RegRegInst::Sub) {
    if (func->constant_reg.find(rr->rhs) != func->constant_reg.end()) {
      int32_t rv = func->constant_reg[rr->rhs];
      if (is_imm12(-rv)) {
        return make_unique<RegImmInst>(RegImmInst::Addi, rr->dst, rr->lhs, -rv);
      }
    }
  } else if (rr->op == RegRegInst::Mul) {
    if (func->constant_reg.find(rr->lhs) != func->constant_reg.end()) {
      int32_t lv = func->constant_reg[rr->lhs];
      if (log2_map.find(lv) != log2_map.end()) {
        return make_unique<RegImmInst>(RegImmInst::Slli, rr->dst, rr->rhs,
                                       log2_map[lv]);
      }
    }
    if (func->constant_reg.find(rr->rhs) != func->constant_reg.end()) {
      int32_t rv = func->constant_reg[rr->rhs];
      if (log2_map.find(rv) != log2_map.end()) {
        return make_unique<RegImmInst>(RegImmInst::Slli, rr->dst, rr->lhs,
                                       log2_map[rv]);
      }
    }
  }
  return nullptr;
}

void inline_constant(Func *func) {
  for (auto &block : func->blocks)
    for (auto &inst : block->insts) {
      if (RegRegInst *rr = inst->as<RegRegInst>()) {
        auto replace = optimize_reg_reg_inst(func, rr);
        if (replace) inst = std::move(replace);
      }
    }
}

// F1: (Inst*) -> bool
// F2: (Inst*, Reg, Inst*) -> unique_ptr<Inst>
template <class F1, class F2>
static void merge_inst(Func *func, F1 check_def, F2 check_use) {
  func->build_def_use();
  for (int r = RegCount; r < func->reg_n; ++r) {
    if (func->reg_def[r].size() != 1) continue;
    OccurPoint def = *func->reg_def[r].begin();
    Inst *def_inst = def.it->get();
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
      replace.push_back(std::move(cur_replace));
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

void merge_addi_lw_sw(Func *func) {
  merge_inst(
      func,
      [](Inst *def) -> bool {
        if (RegImmInst *ri = def->as<RegImmInst>()) {
          return ri->op == RegImmInst::Addi;
        } else
          return false;
      },
      [](Inst *use, Reg r, Inst *def) -> unique_ptr<Inst> {
        RegImmInst *ri = def->as<RegImmInst>();
        assert(ri->dst == r);
        if (Load *lw = use->as<Load>()) {
          if (lw->base == r && is_imm12(lw->offset + ri->rhs))
            return make_unique<Load>(lw->dst, ri->lhs, lw->offset + ri->rhs);
        } else if (Store *sw = use->as<Store>()) {
          if (sw->base == r && is_imm12(sw->offset + ri->rhs))
            return make_unique<Store>(sw->src, ri->lhs, sw->offset + ri->rhs);
        }
        return nullptr;
      });
}
void remove_unused(Func *func) {
  func->calc_live();
  for (auto &block : func->blocks) {
    set<Reg> live = block->live_out;
    for (auto it = block->insts.rbegin(); it != block->insts.rend();) {
      bool used = (*it)->side_effect();
      for (Reg r : (*it)->def_reg())
        if ((r.is_machine() && !allocable(r.id)) || live.find(r) != live.end())
          used = true;
      if (!used) {
        if (global_config.log_level <= Configuration::DEBUG) {
          debug << "remove unused: " << (*it)->to_string();
        }
        auto base = it.base();
        block->insts.erase(std::prev(base));
        it = std::make_reverse_iterator(base);
      } else {
        (*it)->update_live(live);
        ++it;
      }
    }
  }
}
void remove_identical_move(Func *func) {
  for (auto &block : func->blocks)
    for (auto it = block->insts.begin(); it != block->insts.end();) {
      auto nxt = std::next(it);
      if (Move *mov = (*it)->as<Move>())
        if (mov->dst == mov->src) block->insts.erase(it);
      it = nxt;
    }
}
void direct_jump(Func *func) {
  size_t n = func->blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i) pos[func->blocks[i].get()] = i;
  vector<size_t> parent;
  parent.resize(n);
  for (size_t i = 0; i < n; ++i) {
    parent[i] = i;
    Block *cur = func->blocks[i].get();
    if (cur->insts.size() == 1) {
      if (Jump *b = (*cur->insts.begin())->as<Jump>())
        parent[i] = pos[b->target];
    } else if (cur->insts.size() == 0) {
      assert(i + 1 < n);
      parent[i] = i + 1;
    }
  }
  std::function<size_t(size_t)> get_root = [&](size_t x) {
    return parent[x] == x ? x : parent[x] = get_root(parent[x]);
  };
  for (size_t i = 0; i < n; ++i) {
    auto &block = func->blocks[i];
    for (auto &inst : block->insts)
      if (Branch *b = inst->as<Branch>()) {
        size_t final_target = get_root(pos[b->target]);
        b->target = func->blocks[final_target].get();
        b->target->label_used = true;
      } else if (Jump *j = inst->as<Jump>()) {
        size_t final_target = get_root(pos[j->target]);
        j->target = func->blocks[final_target].get();
        j->target->label_used = true;
      }
  }
}

void optimize_before_reg_alloc(Program *prog) {
  for (auto &f : prog->funcs) inline_constant(f.get());
  for (auto &f : prog->funcs) merge_addi_lw_sw(f.get());
  for (auto &f : prog->funcs) remove_unused(f.get());
}

void optimize_after_reg_alloc(Func *func) {
  remove_unused(func);
  remove_identical_move(func);
  direct_jump(func);
}

}  // namespace RV32