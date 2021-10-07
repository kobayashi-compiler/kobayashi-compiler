#pragma once

namespace ARMv7 {
struct Func;
struct Program;
void more_constant_info(Func *func);
void inline_constant(Func *func);
void spill_constant_to_first_use(Func *func);
void remove_unused(Func *func);
void remove_identical_move(Func *func);
void remove_no_effect(Func *func);
void direct_jump(Func *func);
void remove_unreachable(Func *func);
void eliminate_branch(Func *func);
void optimize_before_reg_alloc(Program *prog);
void optimize_after_reg_alloc(Func *func);
}  // namespace ARMv7