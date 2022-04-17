#pragma once

#include <array>
#include <cstdint>
#include <ostream>

namespace RV32 {

constexpr int RegCount = 32;
constexpr int zero = 0, ra = 1, sp = 2, gp = 3, tp = 4, fp = 8;

enum RegisterUsage { caller_save, callee_save, special };

constexpr RegisterUsage REGISTER_USAGE[RegCount] = {
    special,     callee_save, special,     special,
    special,                                // zero, ra, sp, gp, tp
    caller_save, caller_save, caller_save,  // t0-t2
    callee_save, callee_save,               // s0-s1
    caller_save, caller_save, caller_save, caller_save,
    caller_save, caller_save, caller_save, caller_save,  // a0-a7
    callee_save, callee_save, callee_save, callee_save,
    callee_save, callee_save, callee_save, callee_save,
    callee_save, callee_save,                           // s2-s11
    caller_save, caller_save, caller_save, caller_save  // t3-t6
};

constexpr bool allocable(int reg_id) {
  return REGISTER_USAGE[reg_id] == caller_save ||
         REGISTER_USAGE[reg_id] == callee_save;
}

constexpr int ALLOCABLE_REGISTER_COUNT = []() constexpr {
  int cnt = 0;
  for (int i = 0; i < RegCount; ++i)
    if (allocable(i)) ++cnt;
  return cnt;
}
();

constexpr std::array<int, ALLOCABLE_REGISTER_COUNT> ALLOCABLE_REGISTERS =
    []() constexpr {
  std::array<int, ALLOCABLE_REGISTER_COUNT> ret{};
  int cnt = 0;
  for (int i = 0; i < RegCount; ++i)
    if (allocable(i)) ret[cnt++] = i;
  return ret;
}
();

constexpr int ARGUMENT_REGISTER_COUNT = 8;

constexpr int ARGUMENT_REGISTERS[ARGUMENT_REGISTER_COUNT] = {10, 11, 12, 13,
                                                             14, 15, 16, 17};

constexpr int32_t IMM12_L = -2048, IMM12_R = 2047;

constexpr bool is_imm12(int32_t value) {
  return value >= IMM12_L && value <= IMM12_R;
}

struct Reg {
  int id;

  Reg(int _id = -1) : id(_id) {}
  bool is_machine() const { return id < RegCount; }
  bool is_pseudo() const { return id >= RegCount; }
  bool operator<(const Reg &rhs) const { return id < rhs.id; }
  bool operator==(const Reg &rhs) const { return id == rhs.id; }
  bool operator>(const Reg &rhs) const { return id > rhs.id; }
  bool operator!=(const Reg &rhs) const { return id != rhs.id; }
};

enum Compare { Eq, Ne, Lt, Le, Gt, Ge };

constexpr Compare logical_not(Compare c) {
  switch (c) {
    case Eq:
      return Ne;
    case Ne:
      return Eq;
    case Lt:
      return Ge;
    case Le:
      return Gt;
    case Gt:
      return Le;
    case Ge:
      return Lt;
  }
}

inline std::ostream &operator<<(std::ostream &os, Compare c) {
  switch (c) {
    case Eq:
      os << "eq";
      break;
    case Ne:
      os << "ne";
      break;
    case Lt:
      os << "lt";
      break;
    case Le:
      os << "le";
      break;
    case Gt:
      os << "gt";
      break;
    case Ge:
      os << "ge";
      break;
  }
  return os;
}

}  // namespace RV32