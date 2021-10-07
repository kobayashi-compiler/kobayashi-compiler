#pragma once

#include <functional>
#include <memory>

#include "backend/armv7/program.hpp"

namespace ARMv7 {

void merge_inst(Func *func, std::function<bool(Inst *)> check_def,
                std::function<std::unique_ptr<Inst>(Inst *, Reg)> check_use);

void merge_shift_binary_op(Func *func);

void merge_add_ldr_str(Func *func);

}  // namespace ARMv7