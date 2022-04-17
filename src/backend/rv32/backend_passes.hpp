#pragma once

namespace RV32 {

struct Func;
struct Program;

void inline_constant(Func *func);
void merge_addi_lw_sw(Func *func);
void remove_unused(Func *func);
void remove_identical_move(Func *func);
void direct_jump(Func *func);
void optimize_before_reg_alloc(Program *prog);
void optimize_after_reg_alloc(Func *func);

}  // namespace RV32