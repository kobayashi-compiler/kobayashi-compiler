#pragma once

#include <cassert>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <vector>

#include "backend/rv32/archinfo.hpp"
#include "backend/rv32/def.hpp"
#include "backend/rv32/program.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"

namespace RV32 {

inline std::ostream &operator<<(std::ostream &os, const Reg &reg) {
  if (reg.id == zero)
    os << "zero";
  else if (reg.id == ra)
    os << "ra";
  else if (reg.id == sp)
    os << "sp";
  else if (reg.id == gp)
    os << "gp";
  else if (reg.id == tp)
    os << "tp";
  else
    os << 'x' << reg.id;
  return os;
}

struct Inst {
  virtual ~Inst() = default;

  virtual std::vector<Reg> def_reg() { return {}; }
  virtual std::vector<Reg> use_reg() { return {}; }
  virtual std::vector<Reg *> regs() { return {}; }
  virtual bool side_effect() {
    return false;
  }  // side effect apart from assigning value to def_reg() registers
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) = 0;
  virtual void print(std::ostream &out) { gen_asm(out, nullptr); }
  virtual void maintain_sp(int32_t &sp_offset) {}

  std::string to_string() {
    std::ostringstream buf;
    print(buf);
    return buf.str();
  }
  template <class T>
  T *as() {
    return dynamic_cast<T *>(this);
  }
  void update_live(std::set<Reg> &live) {
    for (Reg i : def_reg())
      if (i.is_pseudo() || allocable(i.id)) live.erase(i);
    for (Reg i : use_reg())
      if (i.is_pseudo() || allocable(i.id)) live.insert(i);
  }
  bool def(Reg reg) {
    for (Reg r : def_reg())
      if (r == reg) return true;
    return false;
  }
  bool use(Reg reg) {
    for (Reg r : use_reg())
      if (r == reg) return true;
    return false;
  }
  bool relate(Reg reg) { return def(reg) || use(reg); }
  void replace_reg(Reg before, Reg after) {
    for (Reg *i : regs())
      if ((*i) == before) (*i) = after;
  }
};

struct RegRegInst : Inst {
  enum Type {
    Add,
    Sub,
    Mul,
    Div,
    Rem,
    Sll,
    Srl,
    Sra,
    And,
    Or,
    Xor,
    Slt,
    Sltu
  } op;
  Reg dst, lhs, rhs;
  RegRegInst(Type _op, Reg _dst, Reg _lhs, Reg _rhs)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {lhs, rhs}; }
  virtual std::vector<Reg *> regs() override { return {&dst, &lhs, &rhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    static const std::map<Type, std::string> asm_name{
        {Add, "add"}, {Sub, "sub"}, {Mul, "mul"},  {Div, "div"}, {Rem, "rem"},
        {Sll, "sll"}, {Srl, "srl"}, {Sra, "sra"},  {And, "and"}, {Or, "or"},
        {Xor, "xor"}, {Slt, "slt"}, {Sltu, "sltu"}};
    out << asm_name.find(op)->second << ' ' << dst << ", " << lhs << ", " << rhs
        << '\n';
  }

  static Type from_ir_binary_op(IR::BinaryOp::Type t) {
    switch (t) {
      case IR::BinaryOp::ADD:
        return Add;
      case IR::BinaryOp::SUB:
        return Sub;
      case IR::BinaryOp::MUL:
        return Mul;
      case IR::BinaryOp::DIV:
        return Div;
      case IR::BinaryOp::MOD:
        return Rem;
      default:
        unreachable();
    }
  }
};

struct RegImmInst : Inst {
  enum Type { Addi, Slli, Srli, Srai, Andi, Ori, Xori, Slti, Sltiu } op;
  Reg dst, lhs;
  int32_t rhs;
  RegImmInst(Type _op, Reg _dst, Reg _lhs, int32_t _rhs)
      : op(_op), dst(_dst), lhs(_lhs), rhs(_rhs) {
    if (op == Slli || op == Srli || op == Srai) {
      assert(rhs >= 0 && rhs < 32);
    } else {
      assert(is_imm12(rhs));
    }
  }

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {lhs}; }
  virtual std::vector<Reg *> regs() override { return {&dst, &lhs}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    static const std::map<Type, std::string> asm_name{
        {Addi, "addi"}, {Slli, "slli"}, {Srli, "srli"},
        {Srai, "srai"}, {Andi, "andi"}, {Ori, "ori"},
        {Xori, "xori"}, {Slti, "slti"}, {Sltiu, "sltiu"}};
    out << asm_name.find(op)->second << ' ' << dst << ", " << lhs << ", " << rhs
        << '\n';
  }
};

struct LoadImm : Inst {
  Reg dst;
  int32_t value;
  LoadImm(Reg _dst, int32_t _value) : dst(_dst), value(_value) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "li " << dst << ", " << value << '\n';
  }
};

struct Jump : Inst {
  Block *target;
  Jump(Block *_target) : target(_target) { target->label_used = true; }

  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "j " << target->name << '\n';
  }
};

struct Branch : Inst {
  Block *target;
  Reg lhs, rhs;
  Compare op;
  Branch(Block *_target, Reg _lhs, Reg _rhs, Compare _op)
      : target(_target), lhs(_lhs), rhs(_rhs), op(_op) {
    target->label_used = true;
  }

  virtual std::vector<Reg> use_reg() override { return {lhs, rhs}; }
  virtual std::vector<Reg *> regs() override { return {&lhs, &rhs}; }
  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << 'b' << op << ' ' << lhs << ", " << rhs << ", " << target->name
        << '\n';
  }
};

struct FuncCall : Inst {
  std::string name;
  int arg_cnt;
  FuncCall(std::string _name, int _arg_cnt)
      : name(std::move(_name)), arg_cnt(_arg_cnt) {}

  virtual std::vector<Reg> def_reg() override {
    std::vector<Reg> ret;
    for (int i = 0; i < RegCount; ++i)
      if (REGISTER_USAGE[i] == caller_save) ret.emplace_back(i);
    ret.emplace_back(ra);
    return ret;
  }
  virtual std::vector<Reg> use_reg() override {
    std::vector<Reg> ret;
    for (int i = 0; i < std::min(arg_cnt, ARGUMENT_REGISTER_COUNT); ++i)
      ret.emplace_back(ARGUMENT_REGISTERS[i]);
    return ret;
  }
  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "call ";
    // call is a pseudo instruction, let the linker decide to use jal or auipc +
    // jalr
    if (name == "putf")
      out << "printf";
    else if (name == "starttime")
      out << "_sysy_starttime";
    else if (name == "stoptime")
      out << "_sysy_stoptime";
    else
      out << name;
    out << '\n';
  }
};
struct Return : Inst {
  bool has_return_value;
  Return(bool _has_return_value) : has_return_value(_has_return_value) {}

  virtual std::vector<Reg> use_reg() override {
    if (has_return_value)
      return {Reg{ARGUMENT_REGISTERS[0]}};
    else
      return {};
  }
  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    ctx->epilogue(out);
    out << "ret\n";
  }
  virtual void print(std::ostream &out) override { out << "ret\n"; }
};

struct Load : Inst {
  Reg dst, base;
  int32_t offset;
  Load(Reg _dst, Reg _base, int32_t _offset)
      : dst(_dst), base(_base), offset(_offset) {
    assert(is_imm12(offset));
  }

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {base}; }
  virtual std::vector<Reg *> regs() override { return {&dst, &base}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "lw " << dst << ", " << offset << '(' << base << ")\n";
  }
};

struct Store : Inst {
  Reg src, base;
  int32_t offset;
  Store(Reg _src, Reg _base, int32_t _offset)
      : src(_src), base(_base), offset(_offset) {
    assert(is_imm12(offset));
  }

  virtual std::vector<Reg> use_reg() override { return {src, base}; }
  virtual std::vector<Reg *> regs() override { return {&src, &base}; }
  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "sw " << src << ", " << offset << '(' << base << ")\n";
  }
};

struct LoadStack : Inst {
  StackObject *base;
  Reg dst;
  int32_t offset;
  LoadStack(StackObject *_base, Reg _dst, int32_t _offset)
      : base(_base), dst(_dst), offset(_offset) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    throw RuntimeError("LoadStack should be replaced");
  }
  virtual void print(std::ostream &out) override {
    out << dst << " = LoadStack " << offset << '(' << base << ")\n";
  }
};

struct StoreStack : Inst {
  StackObject *base;
  Reg src;
  int32_t offset;
  StoreStack(StackObject *_base, Reg _src, int32_t _offset)
      : base(_base), src(_src), offset(_offset) {}

  virtual std::vector<Reg> use_reg() override { return {src}; }
  virtual std::vector<Reg *> regs() override { return {&src}; }
  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    throw RuntimeError("StoreStack should be replaced");
  }
  virtual void print(std::ostream &out) override {
    out << "StoreStack " << src << ", " << offset << '(' << base << ")\n";
  }
};

struct LoadStackAddr : Inst {
  StackObject *base;
  Reg dst;
  int32_t offset;
  LoadStackAddr(StackObject *_base, Reg _dst, int32_t _offset)
      : base(_base), dst(_dst), offset(_offset) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    throw RuntimeError("LoadStackAddr should be replaced");
  }
  virtual void print(std::ostream &out) override {
    out << dst << " = LoadStackAddr " << offset << '(' << base << ")\n";
  }
};

struct Move : Inst {
  Reg dst, src;
  Move(Reg _dst, Reg _src) : dst(_dst), src(_src) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg> use_reg() override { return {src}; }
  virtual std::vector<Reg *> regs() override { return {&dst, &src}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "mv " << dst << ", " << src << '\n';
  }
};

struct MoveSP : Inst {  // for passing arguments
  int32_t offset;       // imm12
  MoveSP(int32_t _offset) : offset(_offset) { assert(is_imm12(_offset)); }

  virtual bool side_effect() override { return true; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "addi sp, sp, " << offset << '\n';
    ctx->temp_sp_offset += offset;
  }
  virtual void print(std::ostream &out) override {
    out << "MoveSP(" << offset << ")\n";
  }
  virtual void maintain_sp(int32_t &sp_offset) override { sp_offset += offset; }
};

struct LoadLabelAddr : Inst {
  Reg dst;
  std::string label;
  LoadLabelAddr(Reg _dst, std::string _label)
      : dst(_dst), label(std::move(_label)) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    out << "la " << dst << ", " << label << '\n';
  }
};

struct VirtualDefPoint : Inst {  // a def point that should be removed, which is
                                 // for comparing operation
  Reg dst;
  VirtualDefPoint(Reg _dst) : dst(_dst) {}

  virtual std::vector<Reg> def_reg() override { return {dst}; }
  virtual std::vector<Reg *> regs() override { return {&dst}; }
  virtual void gen_asm(std::ostream &out, AsmContext *ctx) override {
    throw RuntimeError("VirtualDefPoint should be removed");
  }
  virtual void print(std::ostream &out) override {
    out << "VirtualDefPoint(" << dst << ")\n";
  }
};

}  // namespace RV32
