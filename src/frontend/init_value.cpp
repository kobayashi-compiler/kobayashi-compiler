#include "init_value.hpp"

#include <climits>

#include "common/common.hpp"
#include "common/errors.hpp"
#include "frontend/symbol_table.hpp"

#define check_and_return(x)                                            \
  if ((x) < INT32_MIN || (x) > INT32_MAX)                              \
    throw CompileTimeValueEvalFail("compile-time value out of bound"); \
  return CompileTimeValue{static_cast<int32_t>(x)};

CompileTimeValue CompileTimeValue::operator-() {
  if (value == INT_MIN) return CompileTimeValue{INT_MIN};
  int64_t res = -static_cast<int64_t>(value);
  check_and_return(res)
}
CompileTimeValue CompileTimeValue::operator!() {
  return CompileTimeValue{!value};
}

CompileTimeValue operator+(CompileTimeValue lhs, CompileTimeValue rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) + static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

CompileTimeValue operator-(CompileTimeValue lhs, CompileTimeValue rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) - static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

CompileTimeValue operator*(CompileTimeValue lhs, CompileTimeValue rhs) {
  int64_t res =
      static_cast<int64_t>(lhs.value) * static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

CompileTimeValue operator/(CompileTimeValue lhs, CompileTimeValue rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) / static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

CompileTimeValue operator%(CompileTimeValue lhs, CompileTimeValue rhs) {
  if (rhs.value == 0)
    throw CompileTimeValueEvalFail(
        "division by zero in compile-time constant evaluation");
  int64_t res =
      static_cast<int64_t>(lhs.value) % static_cast<int64_t>(rhs.value);
  check_and_return(res)
}

CompileTimeValue operator<(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value < rhs.value};
}

CompileTimeValue operator<=(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value <= rhs.value};
}

CompileTimeValue operator==(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value == rhs.value};
}

CompileTimeValue operator!=(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value != rhs.value};
}

CompileTimeValue operator&&(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value && rhs.value};
}

CompileTimeValue operator||(CompileTimeValue lhs, CompileTimeValue rhs) {
  return CompileTimeValue{lhs.value || rhs.value};
}
