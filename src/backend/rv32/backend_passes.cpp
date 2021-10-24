#include "backend/rv32/backend_passes.hpp"

#include <set>

#include "backend/rv32/program.hpp"
#include "backend/rv32/inst.hpp"

using std::set;
namespace RV32 {

void inline_constant(Func *func) {}
void merge_addi_lw_sw(Func *func) {}
void remove_unused(Func *func) {
  func->calc_live();
  for (auto &block : func->blocks) {
    set<Reg> live = block->live_out;
    for (auto it = block->insts.rbegin(); it != block->insts.rend();) {
      bool used = (*it)->side_effect();
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
void direct_jump(Func *func) {}

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

}