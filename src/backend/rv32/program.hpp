#pragma once

#include <map>
#include <functional>
#include "backend/rv32/archinfo.hpp"
#include "backend/rv32/def.hpp"
#include "optimizer/ir.hpp"

namespace RV32 {

struct Inst;

struct RegAllocStat {
  int spill_cnt, move_eliminated, callee_save_used;
  bool succeed;
};

struct AsmContext {
  int32_t temp_sp_offset;
  std::function<void(std::ostream &)> epilogue;
};

struct CmpInfo {
  Compare type;
  Reg lhs, rhs;
};

struct Block {
  double prob;
  std::string name;
  bool label_used;
  std::list<std::unique_ptr<Inst>> insts;
  std::vector<Block *> in_edge, out_edge;
  std::set<Reg> live_use, def, live_in, live_out;

  Block(std::string _name);
  void construct(IR::BB *ir_bb, Func *func, MappingInfo *info,
                 Block *next_block, std::map<Reg, CmpInfo> &cmp_info);
  void push_back(std::unique_ptr<Inst> inst);
  void push_back(std::list<std::unique_ptr<Inst>> inst_list);
  void insert_before_jump(std::unique_ptr<Inst> inst);
  void gen_asm(std::ostream &out, AsmContext *ctx);
  void print(std::ostream &out);
};

struct Program;

struct OccurPoint {
  Block *b;
  std::list<std::unique_ptr<Inst>>::iterator it;
  int pos;
  bool operator<(const OccurPoint &y) const {
    if (b != y.b)
      return std::less<Block *>{}(b, y.b);
    else
      return pos < y.pos;
  }
};

struct Func {
  std::string name;
  std::vector<std::unique_ptr<Block>> blocks;
  std::vector<std::unique_ptr<StackObject>> stack_objects,
      caller_stack_object;  // caller_stack_object is for argument
  std::vector<Reg> arg_reg;
  Block *entry;
  std::set<Reg> spilling_reg;
  std::map<Reg, int32_t> constant_reg;
  std::map<Reg, std::string> symbol_reg;
  std::map<Reg, std::pair<StackObject *, int32_t>> stack_addr_reg;
  int reg_n;

  std::vector<std::set<OccurPoint>> reg_def, reg_use;

  Func(Program *prog, std::string _name, IR::NormalFunc *ir_func);
  void erase_def_use(const OccurPoint &p, Inst *inst);
  void add_def_use(const OccurPoint &p, Inst *inst);
  void build_def_use();
  void calc_live();
  void gen_asm(std::ostream &out);
  void print(std::ostream &out);

 private:
  std::vector<int> reg_allocate(RegAllocStat *stat);
  bool check_store_stack();  // if a StoreStack instruction immediate offset is
                             // out of range, replace with
                             // LoadImm + Add + Store
                             // and return false, else return true
  void replace_with_reg_alloc(const std::vector<int> &reg_alloc);
  void replace_complex_inst();  // replace LoadStack, StoreStack and LoadStackAddr
};

struct GlobalObject {
  std::string name;
  int size;    // only available when is_int
  void *init;
  bool is_int, is_const;
};

struct Program {
  std::vector<std::unique_ptr<Func>> funcs;
  std::vector<std::unique_ptr<GlobalObject>> global_objects;
  int block_n;

  Program(IR::CompileUnit *ir);
  void gen_global_var_asm(std::ostream &out);
  void gen_asm(std::ostream &out);
};

}