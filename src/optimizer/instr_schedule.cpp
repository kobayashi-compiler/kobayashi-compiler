#include "pass.hpp"

namespace InstrSchedule {
void instruction_scheduling(NormalFunc *f) {
  f->for_each([&](BB *bb) {
    std::unordered_map<Reg, unique_ptr<Instr>> r2i;
    std::list<unique_ptr<Instr>> newInstrs;
    assert(bb->instrs.size() != 0);
    auto end = bb->instrs.end();
    end--;
    Instr *x = end->get();
    CaseNot(ControlInstr, x) { assert(false); }
    for (auto it = bb->instrs.begin(); it != end; ++it) {
      Instr *x = it->get();
      Case(LoadConst, x0, x) { r2i[x0->d1] = std::move(*it); }
      else Case(UnaryOpInstr, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(BinaryOpInstr, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        if (r2i.count(x0->s2)) {
          newInstrs.emplace_back(std::move(r2i[x0->s2]));
          r2i.erase(r2i.find(x0->s2));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(LoadInstr, x0, x) {
        if (r2i.count(x0->addr)) {
          newInstrs.emplace_back(std::move(r2i[x0->addr]));
          r2i.erase(r2i.find(x0->addr));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(StoreInstr, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        if (r2i.count(x0->addr)) {
          newInstrs.emplace_back(std::move(r2i[x0->addr]));
          r2i.erase(r2i.find(x0->addr));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(BranchInstr, x0, x) {
        if (r2i.count(x0->cond)) {
          newInstrs.emplace_back(std::move(r2i[x0->cond]));
          r2i.erase(r2i.find(x0->cond));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(ReturnInstr, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(CallInstr, x0, x) {
        std::unordered_set<Reg> regs;
        for (auto reg : x0->args) {
          if (r2i.count(reg)) {
            regs.insert(reg);
          }
        }
        for (auto reg : regs) {
          newInstrs.emplace_back(std::move(r2i[reg]));
          r2i.erase(r2i.find(reg));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(ArrayIndex, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        if (r2i.count(x0->s2)) {
          newInstrs.emplace_back(std::move(r2i[x0->s2]));
          r2i.erase(r2i.find(x0->s2));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(PhiInstr, x0, x) {
        std::unordered_set<Reg> regs;
        for (auto p : x0->uses) {
          if (r2i.count(p.first)) {
            regs.insert(p.first);
          }
        }
        for (auto reg : regs) {
          newInstrs.emplace_back(std::move(r2i[reg]));
          r2i.erase(r2i.find(reg));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(MemUse, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(MemEffect, x0, x) {
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(MemRead, x0, x) {
        if (r2i.count(x0->mem)) {
          newInstrs.emplace_back(std::move(r2i[x0->mem]));
          r2i.erase(r2i.find(x0->mem));
        }
        if (r2i.count(x0->addr)) {
          newInstrs.emplace_back(std::move(r2i[x0->addr]));
          r2i.erase(r2i.find(x0->addr));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else Case(MemWrite, x0, x) {
        if (r2i.count(x0->mem)) {
          newInstrs.emplace_back(std::move(r2i[x0->mem]));
          r2i.erase(r2i.find(x0->mem));
        }
        if (r2i.count(x0->addr)) {
          newInstrs.emplace_back(std::move(r2i[x0->addr]));
          r2i.erase(r2i.find(x0->addr));
        }
        if (r2i.count(x0->s1)) {
          newInstrs.emplace_back(std::move(r2i[x0->s1]));
          r2i.erase(r2i.find(x0->s1));
        }
        newInstrs.emplace_back(std::move(*it));
      }
      else {
        newInstrs.emplace_back(std::move(*it));
      }
    }
    for (auto &p : r2i) {
      newInstrs.emplace_back(std::move(p.second));
    }
    newInstrs.emplace_back(std::move(*end));
    assert(newInstrs.size() == bb->instrs.size());
    bb->instrs = std::move(newInstrs);
  });
}
}  // namespace InstrSchedule

void instruction_scheduling(CompileUnit &c) {
  c.for_each(InstrSchedule::instruction_scheduling);
}