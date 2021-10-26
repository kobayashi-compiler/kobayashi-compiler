#include "backend/armv7/backend_passes.hpp"

#include <functional>
#include <iterator>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "backend/armv7/merge_inst.hpp"

using std::make_unique;
using std::map;
using std::numeric_limits;
using std::pair;
using std::set;
using std::string;
using std::vector;
namespace ARMv7 {

void more_constant_info(Func *func) {
  for (auto &block : func->blocks)
    for (auto &inst : block->insts) {
      if (RegImmInst *x = inst->as<RegImmInst>())
        if (func->stack_addr_reg.find(x->lhs) != func->stack_addr_reg.end()) {
          if (x->op == RegImmInst::Add) {
            func->stack_addr_reg[x->dst] = func->stack_addr_reg[x->lhs];
            func->stack_addr_reg[x->dst].second += x->rhs;
          } else if (x->op == RegImmInst::Sub) {
            func->stack_addr_reg[x->dst] = func->stack_addr_reg[x->lhs];
            func->stack_addr_reg[x->dst].second -= x->rhs;
          }
        }
      if (RegRegInst *x = inst->as<RegRegInst>()) {
        if (func->stack_addr_reg.find(x->lhs) != func->stack_addr_reg.end() &&
            func->constant_reg.find(x->rhs) != func->constant_reg.end()) {
          if (x->op == RegRegInst::Add) {
            func->stack_addr_reg[x->dst] = func->stack_addr_reg[x->lhs];
            func->stack_addr_reg[x->dst].second += func->constant_reg[x->rhs];
          } else if (x->op == RegRegInst::Sub) {
            func->stack_addr_reg[x->dst] = func->stack_addr_reg[x->lhs];
            func->stack_addr_reg[x->dst].second -= func->constant_reg[x->rhs];
          }
        } else if (func->stack_addr_reg.find(x->rhs) !=
                       func->stack_addr_reg.end() &&
                   func->constant_reg.find(x->lhs) !=
                       func->constant_reg.end()) {
          if (x->op == RegRegInst::Add) {
            func->stack_addr_reg[x->dst] = func->stack_addr_reg[x->rhs];
            func->stack_addr_reg[x->dst].second += func->constant_reg[x->lhs];
          }
        }
      }
    }
}

static map<int32_t, int> log2_map = []() {
  map<int32_t, int> ret;
  for (int i = 0; i < 31; ++i) ret[1 << i] = i;
  ret[numeric_limits<int32_t>::min()] = 31;
  return ret;
}();

void inline_constant(Func *func) {
  for (auto &block : func->blocks) {
    bool used_cpsr = false;
    auto inst = block->insts.end();
    while (inst != block->insts.begin()) {
      inst--;
      if ((*inst)->change_cpsr()) {
        used_cpsr = false;
      }
      if ((*inst)->use_cpsr()) {
        used_cpsr = true;
      }
      if (RegRegInst *x = (*inst)->as<RegRegInst>()) {
        assert((*inst)->change_cpsr() == false);
        if (x->op == RegRegInst::Div && !used_cpsr) {
          if (func->constant_reg.find(x->rhs) != func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->rhs];
            if (log2_map.find(v) != log2_map.end()) {
              int shift_v = log2_map[v];
              if (v == 1) {
                *inst = set_cond(make_unique<MoveReg>(x->dst, x->lhs), x->cond);
              } else if (shift_v <= 8) {
                Reg tmp(func->reg_n++);
                block->insts.insert(
                    inst,
                    set_cond(make_unique<RegImmCmp>(RegImmCmp::Cmp, x->lhs, 0),
                             InstCond::Always));
                block->insts.insert(inst,
                                    set_cond(make_unique<MoveReg>(tmp, x->lhs),
                                             InstCond::Always));
                block->insts.insert(inst, set_cond(make_unique<RegImmInst>(
                                                       RegImmInst::Add, tmp,
                                                       tmp, (1 << shift_v) - 1),
                                                   InstCond::Lt));
                *inst = set_cond(make_unique<ShiftInst>(
                                     x->dst, tmp, Shift(Shift::ASR, shift_v)),
                                 InstCond::Always);
              }
            }
          }
        }
      }
    }
  }
  for (auto &block : func->blocks)
    for (auto &inst : block->insts) {
      if (RegRegInst *x = inst->as<RegRegInst>()) {
        if (x->op == RegRegInst::Add) {
          if (func->constant_reg.find(x->lhs) != func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->lhs];
            if (is_legal_immediate(v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Add, x->dst, x->rhs, v),
                  x->cond);
            } else if (is_legal_immediate(-v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Sub, x->dst, x->rhs, -v),
                  x->cond);
            }
          } else if (func->constant_reg.find(x->rhs) !=
                     func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->rhs];
            if (is_legal_immediate(v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Add, x->dst, x->lhs, v),
                  x->cond);
            } else if (is_legal_immediate(-v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Sub, x->dst, x->lhs, -v),
                  x->cond);
            }
          }
        } else if (x->op == RegRegInst::Sub) {
          if (func->constant_reg.find(x->lhs) != func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->lhs];
            if (is_legal_immediate(v)) {
              inst = set_cond(make_unique<RegImmInst>(RegImmInst::RevSub,
                                                      x->dst, x->rhs, v),
                              x->cond);
            }
          } else if (func->constant_reg.find(x->rhs) !=
                     func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->rhs];
            if (is_legal_immediate(v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Sub, x->dst, x->lhs, v),
                  x->cond);
            } else if (is_legal_immediate(-v)) {
              inst = set_cond(
                  make_unique<RegImmInst>(RegImmInst::Add, x->dst, x->lhs, -v),
                  x->cond);
            }
          }
        } else if (x->op == RegRegInst::Mul) {
          if (func->constant_reg.find(x->lhs) != func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->lhs];
            if (log2_map.find(v) != log2_map.end() ||
                log2_map.find(v - 1) != log2_map.end() ||
                (v < numeric_limits<int32_t>::max() &&
                 log2_map.find(v + 1) != log2_map.end()))
              std::swap(x->lhs, x->rhs);
          }
          if (func->constant_reg.find(x->rhs) != func->constant_reg.end()) {
            int32_t v = func->constant_reg[x->rhs];
            if (log2_map.find(v) != log2_map.end()) {
              if (v == 1)
                inst = set_cond(make_unique<MoveReg>(x->dst, x->lhs), x->cond);
              else
                inst = set_cond(
                    make_unique<ShiftInst>(x->dst, x->lhs,
                                           Shift{Shift::LSL, log2_map[v]}),
                    x->cond);
            } else if (log2_map.find(v - 1) != log2_map.end()) {
              inst = set_cond(make_unique<RegRegInst>(
                                  RegRegInst::Add, x->dst, x->lhs, x->lhs,
                                  Shift{Shift::LSL, log2_map[v - 1]}),
                              x->cond);
            } else if (v < numeric_limits<int32_t>::max() &&
                       log2_map.find(v + 1) != log2_map.end()) {
              inst = set_cond(make_unique<RegRegInst>(
                                  RegRegInst::RevSub, x->dst, x->lhs, x->lhs,
                                  Shift{Shift::LSL, log2_map[v + 1]}),
                              x->cond);
            }
          }
        }
      } else if (ML *ml = inst->as<ML>()) {
        if (func->constant_reg.find(ml->s1) != func->constant_reg.end() &&
            log2_map.find(func->constant_reg[ml->s1]) != log2_map.end())
          std::swap(ml->s1, ml->s2);
        if (func->constant_reg.find(ml->s2) != func->constant_reg.end() &&
            log2_map.find(func->constant_reg[ml->s2]) != log2_map.end()) {
          if (ml->op == ML::Mla)
            inst = set_cond(
                make_unique<RegRegInst>(
                    RegRegInst::Add, ml->dst, ml->s3, ml->s1,
                    Shift{Shift::LSL, log2_map[func->constant_reg[ml->s2]]}),
                ml->cond);
          else
            inst = set_cond(
                make_unique<RegRegInst>(
                    RegRegInst::Sub, ml->dst, ml->s3, ml->s1,
                    Shift{Shift::LSL, log2_map[func->constant_reg[ml->s2]]}),
                ml->cond);
        }
      } else if (Load *load = inst->as<Load>()) {
        if (func->stack_addr_reg.find(load->base) !=
            func->stack_addr_reg.end()) {
          auto info = func->stack_addr_reg[load->base];
          inst = set_cond(
              make_unique<LoadStack>(load->dst, load->offset_imm + info.second,
                                     info.first),
              load->cond);
        }
      } else if (Store *store = inst->as<Store>()) {
        if (func->stack_addr_reg.find(store->base) !=
            func->stack_addr_reg.end()) {
          auto info = func->stack_addr_reg[store->base];
          inst = set_cond(
              make_unique<StoreStack>(
                  store->src, store->offset_imm + info.second, info.first),
              store->cond);
        }
      } else if (MoveReg *move_reg = inst->as<MoveReg>()) {
        if (func->constant_reg.find(move_reg->src) !=
            func->constant_reg.end()) {
          int32_t value = func->constant_reg[move_reg->src];
          if (is_legal_immediate(value)) {
            inst = set_cond(
                make_unique<MoveImm>(MoveImm::Mov, move_reg->dst, value),
                move_reg->cond);
          } else if (is_legal_immediate(~value)) {
            inst = set_cond(
                make_unique<MoveImm>(MoveImm::Mvn, move_reg->dst, ~value),
                move_reg->cond);
          }
        }
      } else if (RegRegCmp *cmp = inst->as<RegRegCmp>()) {
        if (func->constant_reg.find(cmp->rhs) != func->constant_reg.end()) {
          int32_t value = func->constant_reg[cmp->rhs];
          if (is_legal_immediate(value)) {
            inst = set_cond(
                make_unique<RegImmCmp>(RegImmCmp::Cmp, cmp->lhs, value),
                cmp->cond);
          } else if (is_legal_immediate(-value)) {
            inst = set_cond(
                make_unique<RegImmCmp>(RegImmCmp::Cmn, cmp->lhs, -value),
                cmp->cond);
          }
        }
      }
    }
}

void spill_constant_to_first_use(Func *func) {
  map<Reg, int32_t> constant_reg = func->constant_reg;
  for (auto &block : func->blocks) {
    map<Reg, Reg> def_this_block;
    for (auto it = block->insts.begin(); it != block->insts.end(); ++it) {
      bool def_constant = false;
      for (Reg r : (*it)->def_reg())
        if (constant_reg.find(r) != constant_reg.end()) {
          def_constant = true;
          break;
        }
      if (def_constant) continue;
      for (Reg r : (*it)->use_reg())
        if (constant_reg.find(r) != constant_reg.end()) {
          if (def_this_block.find(r) == def_this_block.end()) {
            Reg tmp{func->reg_n++};
            int32_t value = constant_reg[r];
            func->constant_reg[tmp] = value;
            insert(block->insts, it, load_imm(tmp, value));
            def_this_block[r] = tmp;
          }
          (*it)->replace_reg(r, def_this_block[r]);
        }
    }
  }
}

void remove_unused(Func *func) {
  func->calc_live();
  for (auto &block : func->blocks) {
    set<Reg> live = block->live_out;
    bool use_cpsr = false;
    for (auto it = block->insts.rbegin(); it != block->insts.rend();) {
      bool used = (*it)->side_effect();
      if ((*it)->change_cpsr() && use_cpsr) used = true;
      for (Reg r : (*it)->def_reg())
        if ((r.is_machine() && !allocable(r.id)) || live.find(r) != live.end()) used = true;
      if (!used) {
        if (global_config.log_level <= Configuration::DEBUG) {
          debug << "remove unused: " << (*it)->to_string();
        }
        auto base = it.base();
        block->insts.erase(std::prev(base));
        it = std::make_reverse_iterator(base);
      } else {
        if ((*it)->change_cpsr()) use_cpsr = false;
        if ((*it)->use_cpsr()) use_cpsr = true;
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
      if (MoveReg *move_reg = (*it)->as<MoveReg>())
        if (move_reg->dst == move_reg->src) block->insts.erase(it);
      it = nxt;
    }
}

void remove_no_effect(Func *func) {
  vector<int> branch_in_deg = func->get_branch_in_deg();
  struct RegStatus {
    enum { Unknown, Constant, Symbol, StackValue } type;
    int32_t const_value;
    string symbol;
    pair<StackObject *, int32_t> stack_value;

    RegStatus() : type(Unknown) {}
    bool operator==(const RegStatus &y) const {
      if (type != y.type) return false;
      switch (type) {
        case Constant:
          return const_value == y.const_value;
        case Symbol:
          return symbol == y.symbol;
        case StackValue:
          return stack_value.first == y.stack_value.first &&
                 stack_value.second == y.stack_value.second;
        default:
          return false;
      }
    }
  };
  RegStatus status[RegCount];
  for (size_t i = 0; i < func->blocks.size(); ++i) {
    if (branch_in_deg[i] > 0) {
      for (auto &s : status) s.type = RegStatus::Unknown;
    }
    auto &block = func->blocks[i];
    for (auto it = block->insts.begin(); it != block->insts.end();) {
      if (MoveImm *mi = (*it)->as<MoveImm>()) {
        if (allocable(mi->dst.id) &&
            status[mi->dst.id].type == RegStatus::Constant &&
            status[mi->dst.id].const_value == mi->src) {
          debug << "remove no effect: " << (*it)->to_string();
          auto nxt = std::next(it);
          block->insts.erase(it);
          it = nxt;
          continue;
        }
        if (allocable(mi->dst.id) && mi->cond == Always) {
          status[mi->dst.id].type = RegStatus::Constant;
          status[mi->dst.id].const_value = mi->src;
          ++it;
          continue;
        }
      } else if (MoveW *mw = (*it)->as<MoveW>()) {
        int dst = mw->dst.id;
        if (allocable(dst)) {
          auto nxt = std::next(it);
          if (nxt != block->insts.end())
            if (MoveT *mt = (*nxt)->as<MoveT>())
              if (mw->dst == mt->dst) {
                if (mw->cond == mt->cond &&
                    status[dst].type == RegStatus::Constant &&
                    status[dst].const_value == concat(mw->src, mt->src)) {
                  debug << "remove no effect: " << (*it)->to_string();
                  block->insts.erase(it);
                  it = std::next(nxt);
                  debug << "remove no effect: " << (*nxt)->to_string();
                  block->insts.erase(nxt);
                  continue;
                }
                if (mw->cond == Always && mt->cond == Always) {
                  status[dst].type = RegStatus::Constant;
                  status[dst].const_value = concat(mw->src, mt->src);
                  ++it;
                  ++it;
                  continue;
                }
              }
          if (status[dst].type == RegStatus::Constant &&
              status[dst].const_value == mw->src) {
            debug << "remove no effect: " << (*it)->to_string();
            block->insts.erase(it);
            it = nxt;
            continue;
          }
          if (mw->cond == Always) {
            status[dst].type = RegStatus::Constant;
            status[dst].const_value = mw->src;
            ++it;
            continue;
          }
        }
      } else if (LoadSymbolAddrLower16 *l16 =
                     (*it)->as<LoadSymbolAddrLower16>()) {
        int dst = l16->dst.id;
        if (allocable(dst)) {
          auto nxt = std::next(it);
          if (nxt != block->insts.end())
            if (LoadSymbolAddrUpper16 *u16 =
                    (*nxt)->as<LoadSymbolAddrUpper16>())
              if (l16->dst == u16->dst && l16->symbol == u16->symbol) {
                if (l16->cond == u16->cond &&
                    status[dst].type == RegStatus::Symbol &&
                    status[dst].symbol == l16->symbol) {
                  debug << "remove no effect: " << (*it)->to_string();
                  block->insts.erase(it);
                  it = std::next(nxt);
                  debug << "remove no effect: " << (*nxt)->to_string();
                  block->insts.erase(nxt);
                  continue;
                }
                if (l16->cond == Always && u16->cond == Always) {
                  status[dst].type = RegStatus::Symbol;
                  status[dst].symbol = l16->symbol;
                  ++it;
                  ++it;
                  continue;
                }
              }
        }
      } else if (LoadStack *ls = (*it)->as<LoadStack>()) {
        int dst = ls->dst.id;
        if (allocable(dst)) {
          if (status[dst].type == RegStatus::StackValue &&
              status[dst].stack_value.first == ls->src &&
              status[dst].stack_value.second == ls->offset) {
            auto nxt = std::next(it);
            debug << "remove no effect: " << (*it)->to_string();
            block->insts.erase(it);
            it = nxt;
            continue;
          }
          if (ls->cond == Always) {
            status[dst].type = RegStatus::StackValue;
            status[dst].stack_value.first = ls->src;
            status[dst].stack_value.second = ls->offset;
            ++it;
            continue;
          }
        }
      } else if (StoreStack *ss = (*it)->as<StoreStack>()) {
        int src = ss->src.id;
        if (allocable(src)) {
          if (status[src].type == RegStatus::StackValue &&
              status[src].stack_value.first == ss->target &&
              status[src].stack_value.second == ss->offset) {
            auto nxt = std::next(it);
            debug << "remove no effect: " << (*it)->to_string();
            block->insts.erase(it);
            it = nxt;
            continue;
          }
          if (ss->cond == Always) {
            for (int i = 0; i < RegCount; ++i)
              if (status[i].type == RegStatus::StackValue &&
                  status[i].stack_value.first == ss->target &&
                  status[i].stack_value.second == ss->offset)
                status[i].type = RegStatus::Unknown;
            status[src].type = RegStatus::StackValue;
            status[src].stack_value.first = ss->target;
            status[src].stack_value.second = ss->offset;
            ++it;
            continue;
          }
        }
      } else if (MoveReg *mr = (*it)->as<MoveReg>()) {
        int dst = mr->dst.id, src = mr->src.id;
        if (allocable(dst) && allocable(src)) {
          if (status[dst] == status[src]) {
            auto nxt = std::next(it);
            debug << "remove no effect: " << (*it)->to_string();
            block->insts.erase(it);
            it = nxt;
            continue;
          }
          if (mr->cond == Always && status[src].type != RegStatus::Unknown) {
            status[dst] = status[src];
            ++it;
            continue;
          }
        }
      }
      for (Reg r : (*it)->def_reg())
        if (allocable(r.id)) status[r.id].type = RegStatus::Unknown;
      if ((*it)->side_effect())
        for (RegStatus &s : status)
          if (s.type == RegStatus::StackValue) s.type = RegStatus::Unknown;
      ++it;
    }
  }
}

void direct_jump(Func *func) {
  vector<size_t> parent;
  size_t n = func->blocks.size();
  parent.resize(n);
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i) pos[func->blocks[i].get()] = i;
  for (size_t i = 0; i < n; ++i) {
    parent[i] = i;
    Block *cur = func->blocks[i].get();
    if (cur->insts.size() == 1) {
      if (Branch *b = (*cur->insts.begin())->as<Branch>())
        if (b->cond == InstCond::Always) parent[i] = pos[b->target];
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
      }
  }
  vector<int> in_deg = func->get_in_deg();
  vector<std::unique_ptr<Block>> useful_blocks;
  for (size_t i = 0; i < n; ++i)
    if (in_deg[i]) useful_blocks.push_back(std::move(func->blocks[i]));
  func->blocks = std::move(useful_blocks);
}

void remove_unreachable(Func *func) {
  for (auto &block : func->blocks) {
    auto it = block->insts.begin();
    for (; it != block->insts.end(); ++it) {
      if (Branch *b = (*it)->as<Branch>()) {
        if (b->cond == InstCond::Always) {
          ++it;
          break;
        }
      } else if (Return *r = (*it)->as<Return>()) {
        ++it;
        break;
      }
    }
    while (it != block->insts.end()) {
      auto nxt = std::next(it);
      block->insts.erase(it);
      it = nxt;
    }
  }
}

void eliminate_branch(Func *func) {
  remove_unreachable(func);
  size_t n = func->blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i) pos[func->blocks[i].get()] = i;
  vector<int> branch_in_deg = func->get_branch_in_deg();
  for (size_t i = 0; i < func->blocks.size(); ++i) {
    auto &block = func->blocks[i];
    while (block->insts.size() > 0 &&
           (*std::prev(block->insts.end()))->as<Branch>()) {
      Branch *b = (*std::prev(block->insts.end()))->as<Branch>();
      if (pos[b->target] == i + 1) {
        block->insts.erase(std::prev(block->insts.end()));
        continue;
      }
      if (pos[b->target] == i + 2 && b->cond != InstCond::Always &&
          branch_in_deg[i + 1] == 0) {
        bool ok = true;
        if (func->blocks[i + 1]->insts.size() > 2) ok = false;
        for (auto it = func->blocks[i + 1]->insts.begin();
             ok && it != func->blocks[i + 1]->insts.end(); ++it) {
          if ((*it)->as<Return>()) ok = false;
          if ((*it)->use_cpsr()) ok = false;
          if ((*it)->change_cpsr() &&
              std::next(it) != func->blocks[i + 1]->insts.end())
            ok = false;
        }
        if (!ok) break;
        InstCond c = logical_not(b->cond);
        block->insts.erase(std::prev(block->insts.end()));
        for (auto &succ_inst : func->blocks[i + 1]->insts)
          block->insts.push_back(set_cond(std::move(succ_inst), c));
        pos.erase(func->blocks[i + 1].get());
        branch_in_deg.erase(branch_in_deg.begin() + i + 1);
        func->blocks.erase(func->blocks.begin() + i + 1);
        for (auto &pos_i : pos)
          if (pos_i.second > i) --pos_i.second;
        continue;
      }
      break;
    }
  }
}

void optimize_before_reg_alloc(Program *prog) {
  for (auto &f : prog->funcs) more_constant_info(f.get());
  for (auto &f : prog->funcs) inline_constant(f.get());
  // for (auto &f : prog->funcs) spill_constant_to_first_use(f.get());
  for (auto &f : prog->funcs) merge_shift_binary_op(f.get());
  for (auto &f : prog->funcs) merge_add_ldr_str(f.get());
  for (auto &f : prog->funcs) remove_unused(f.get());
}

void optimize_after_reg_alloc(Func *func) {
  remove_unused(func);
  remove_identical_move(func);
  remove_no_effect(func);
  direct_jump(func);
  eliminate_branch(func);
}
}  // namespace ARMv7