#include "optimizer/pass.hpp"

void global_int_to_local(CompileUnit &c) {
  std::unordered_map<MemObject *, MemObject *> S;
  c.scope.for_each([&](MemObject *m) {
    if (m->is_single_var()) {
      S[m] = NULL;
    }
  });
  c.for_each([&](NormalFunc *f) {
    if (f->name != "main") {
      f->for_each([&](Instr *i) {
        Case(LoadAddr, i0, i) {
          auto m = i0->offset;
          if (m->global) S.erase(m);
        }
      });
    }
  });
  NormalFunc *main = c.funcs["main"].get();
  for (auto &kv : S) {
    kv.second = main->scope.new_MemObject(kv.first->name);
    kv.second->size = 4;
  }
  main->for_each([&](Instr *i) {
    Case(LoadAddr, i0, i) {
      auto &m = i0->offset;
      auto it = S.find(m);
      if (it != S.end()) m = it->second;
    }
  });
  for (auto &kv : S) {
    auto &s = main->entry->instrs;
    Reg r1 = main->new_Reg();
    Reg r2 = main->new_Reg();
    Reg r3 = main->new_Reg();
    s.emplace_front(new StoreInstr(r3, r2));
    s.emplace_front(new LoadAddr(r3, kv.second));
    s.emplace_front(new LoadInstr(r2, r1));
    s.emplace_front(new LoadAddr(r1, kv.first));
  }
  dbg << "### global int to local\n";
  dbg << "```cpp\n" << c << "```\n\n";
}

void mem2reg_local_int(NormalFunc *f) {
  struct State {
    MemObject *m = NULL;
    Reg r;
  };
  std::unordered_map<Reg, State> RM;        // map reg to addr of memobj
  std::unordered_map<MemObject *, Reg> MR;  // map memobj to reg
  std::unordered_set<Reg> new_reg;
  f->for_each([&](Instr *x) {
    Case(LoadAddr, x0, x) {
      if (!x0->offset->global && x0->offset->is_single_var()) {
        auto &rm = RM[x0->d1];
        rm.m = x0->offset;
        auto &mr = MR[x0->offset];
        if (!mr.id) new_reg.insert(mr = f->new_Reg(x0->offset->name));
        rm.r = mr;
      }
    }
  });
  for (Reg r : new_reg) {
    f->entry->push_front(new LoadConst(r, 0));
  }
  f->for_each([&](BB *bb) {
    for (auto it = bb->instrs.begin(); it != bb->instrs.end(); ++it) {
      Case(LoadInstr, x0, it->get()) {
        if (RM.count(x0->addr)) {
          Reg d1 = x0->d1, s1 = RM[x0->addr].r;
          *it = std::unique_ptr<Instr>(new UnaryOpInstr(d1, s1, UnaryOp::ID));
        }
      }
      else Case(StoreInstr, x0, it->get()) {
        if (RM.count(x0->addr)) {
          Reg d1 = RM[x0->addr].r, s1 = x0->s1;
          *it = std::unique_ptr<Instr>(new UnaryOpInstr(d1, s1, UnaryOp::ID));
        }
      }
    }
  });
  ssa_construction(f, [&](Reg r) { return new_reg.count(r); });

  dbg << "### mem2reg local int: " << f->name << "\n";
  dbg << "```cpp\n" << *f << "```\n\n";
}

void local_array_to_global(CompileUnit &c) {}

void mem2reg(CompileUnit &c) {
  dbg << "## mem to reg\n";
  global_int_to_local(c);
  c.for_each(mem2reg_local_int);
  remove_unused_memobj(c);
  gvn(c);
}
