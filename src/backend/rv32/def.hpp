#pragma once

#include <cstdint>
#include <map>
#include <utility>

#include "optimizer/ir.hpp"
#include "backend/rv32/archinfo.hpp"

namespace RV32 {

struct StackObject {
  int32_t size, position;
  StackObject(int32_t _size, int32_t _pos = -1): size(_size), position(_pos) {}
};


struct StackObject;
struct Block;
struct Func;

struct MappingInfo {
  std::map<IR::MemObject *, StackObject *> obj_mapping;
  std::map<int, Reg> reg_mapping;
  std::map<IR::BB *, Block *> block_mapping;
  std::map<Block *, IR::BB *> rev_block_mapping;
  std::map<int, int32_t> constant_value;
  std::map<int, std::pair<StackObject *, int32_t>> constant_addr;
  int reg_n;

  MappingInfo();

  Reg new_reg();
  Reg from_ir_reg(IR::Reg ir_reg);
};

}