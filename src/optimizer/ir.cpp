#include "optimizer/ir.hpp"

namespace IR {

PrintContext print_ctx;

ostream &operator<<(ostream &os, const Printable &r) {
  r.print(os);
  return os;
}

void Reg::print(ostream &os) const {
  auto f = print_ctx.f;
  if (f)
    os << f->get_name(*this) << "(" << id << ")";
  else
    os << "R[" << id << "]";
}

void MemObject::print(ostream &os) const {
  os << "&" << name << " (" << (global ? "gp" : "sp") << "+" << offset
     << "): size " << size;
  if (arg) os << " (arg)";
}

void MemScope::print(ostream &os) const {
  os << "MemScope(" << name << "){\n";
  for (auto &x : objects) os << "\t" << *x << "\n";
  os << "}\n";
}

void BB::print(ostream &os) const {
  os << "BB(" << name << "){\n";
  for (auto &x : instrs) {
    os << "\t" << *x;
    if (print_ctx.instr_comment.count(x.get())) {
      os << "  // " << print_ctx.instr_comment[x.get()];
    }
    os << "\n";
  }
  os << "}\n";
}

void NormalFunc::print(ostream &os) const {
  SetPrintContext _(this);
  os << "NormalFunc: " << name << "\n";
  os << scope << "\n";
  os << "entry: " << entry->name << "\n";
  for (auto &bb : bbs) os << *bb;
  os << "\n";
}

void CompileUnit::print(ostream &os) const {
  print_ctx.c = this;
  os << scope << "\n";
  for (auto &f : funcs) os << *f.second;
  print_ctx.c = NULL;
}

void LoadAddr::print(ostream &os) const { os << d1 << " = " << *offset; }
void LoadConst::print(ostream &os) const { os << d1 << " = " << value; }
void LoadArg::print(ostream &os) const { os << d1 << " = arg" << id; }
void ArrayIndex::print(ostream &os) const {
  os << d1 << " = " << s1 << " + " << s2 << " * " << size << " : " << limit;
}
void LocalVarDef::print(ostream &os) const { os << "define " << data->name; }
void UnaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << op << " " << s1;
}
void BinaryOpInstr::print(ostream &os) const {
  os << d1 << " = " << s1 << " " << op << " " << s2;
}
void LoadInstr::print(ostream &os) const { os << d1 << " = M[" << addr << "]"; }
void StoreInstr::print(ostream &os) const {
  os << "M[" << addr << "] = " << s1;
}
void JumpInstr::print(ostream &os) const { os << "goto " << target->name; }
void BranchInstr::print(ostream &os) const {
  os << "goto " << cond << " ? " << target1->name << " : " << target0->name;
}
void ReturnInstr::print(ostream &os) const { os << "return " << s1; }

void CallInstr::print(ostream &os) const {
  os << d1 << " = " << f->name;
  char c = '(';
  for (auto s : args) {
    os << c << s;
    c = ',';
  }
  if (c == '(') os << c;
  os << ')';
}
void PhiInstr::print(ostream &os) const {
  os << d1 << " = phi";
  char c = '(';
  for (auto s : uses) {
    os << c << " " << s.first << ":" << s.second->name;
    c = ',';
  }
  if (c == '(') os << c;
  os << " )";
}
void MemDef::print(ostream &os) const {
  os << d1 << " = "
     << " memdef " << data->name;
}
void MemUse::print(ostream &os) const { os << s1 << " used"; }
void MemEffect::print(ostream &os) const {
  os << d1 << " = " << s1 << " updated";
}
void MemRead::print(ostream &os) const {
  os << d1 << " = " << mem << " at " << addr;
}
void MemWrite::print(ostream &os) const {
  os << d1 << " = " << mem << " at " << addr << " write " << s1;
}

CompileUnit::CompileUnit() : scope("global", 1) {
  LibFunc *f;
  f = new_LibFunc(".fork", 0);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc(".join", 1);
  f->in = 1;
  f->out = 1;

  f = new_LibFunc("getint", 0);
  f->in = 1;
  f = new_LibFunc("getch", 0);
  f->in = 1;
  f = new_LibFunc("getarray", 0);
  f->array_args[0] = 1;  // write arg0
  f->in = 1;

  f = new_LibFunc("putint", 1);
  f->out = 1;
  f = new_LibFunc("putch", 1);
  f->out = 1;
  f = new_LibFunc("putarray", 1);
  f->array_args[1] = 0;  // read arg1
  f->out = 1;

  f = new_LibFunc("putf", 1);
  f->array_args[0] = 0;  // read arg0
  f->out = 1;

  f = new_LibFunc("starttime", 1);
  f->in = 1;
  f->out = 1;
  f = new_LibFunc("stoptime", 1);
  f->in = 1;
  f->out = 1;

  auto _in = scope.new_MemObject("stdin");    // input
  auto _out = scope.new_MemObject("stdout");  // output
  scope.set_arg(0, _in);
  scope.set_arg(1, _out);
}

void Instr::map_use(function<void(Reg &)> f1) {
  auto f2 = [](BB *&x) {};
  auto f3 = [](MemObject *&x) {};
  Case(RegWriteInstr, w, this) {
    map(
        [&](Reg &x) {
          if (&x != &w->d1) f1(x);
        },
        f2, f3, 0);
  }
  else {
    map(f1, f2, f3, 0);
  }
}

Instr *Instr::map(function<void(Reg &)> f1, function<void(BB *&)> f2,
                  function<void(MemObject *&)> f3, bool copy) {
  Case(LoadAddr, w, this) {
    auto u = w;
    if (copy) u = new LoadAddr(*w);
    f1(u->d1);
    f3(u->offset);
    return u;
  }
  Case(LoadConst, w, this) {
    auto u = w;
    if (copy) u = new LoadConst(*w);
    f1(u->d1);
    return u;
  }
  Case(LoadArg, w, this) {
    auto u = w;
    if (copy) u = new LoadArg(*w);
    f1(u->d1);
    return u;
  }
  Case(LocalVarDef, w, this) {
    auto u = w;
    if (copy) u = new LocalVarDef(*w);
    f3(u->data);
    return u;
  }
  Case(UnaryOpInstr, w, this) {
    auto u = w;
    if (copy) u = new UnaryOpInstr(*w);
    f1(u->d1);
    f1(u->s1);
    return u;
  }
  Case(BinaryOpInstr, w, this) {
    auto u = w;
    if (copy) u = new BinaryOpInstr(*w);
    f1(u->d1);
    f1(u->s1);
    f1(u->s2);
    return u;
  }
  Case(ArrayIndex, w, this) {
    auto u = w;
    if (copy) u = new ArrayIndex(*w);
    f1(u->d1);
    f1(u->s1);
    f1(u->s2);
    return u;
  }
  Case(LoadInstr, w, this) {
    auto u = w;
    if (copy) u = new LoadInstr(*w);
    f1(u->d1);
    f1(u->addr);
    return u;
  }
  Case(StoreInstr, w, this) {
    auto u = w;
    if (copy) u = new StoreInstr(*w);
    f1(u->addr);
    f1(u->s1);
    return u;
  }
  Case(JumpInstr, w, this) {
    auto u = w;
    if (copy) u = new JumpInstr(*w);
    f2(u->target);
    return u;
  }
  Case(BranchInstr, w, this) {
    auto u = w;
    if (copy) u = new BranchInstr(*w);
    f1(u->cond);
    f2(u->target1);
    f2(u->target0);
    return u;
  }
  Case(ReturnInstr, w, this) {
    auto u = w;
    if (copy) u = new ReturnInstr(*w);
    f1(u->s1);
    return u;
  }
  Case(CallInstr, w, this) {
    auto u = w;
    if (copy) u = new CallInstr(*w);
    f1(u->d1);
    for (auto &x : u->args) f1(x);
    return u;
  }
  Case(PhiInstr, w, this) {
    auto u = w;
    if (copy) u = new PhiInstr(*w);
    f1(u->d1);
    for (auto &x : u->uses) f1(x.first), f2(x.second);
    return u;
  }
  Case(MemDef, w, this) {
    auto u = w;
    if (copy) u = new MemDef(*w);
    f1(u->d1);
    f3(u->data);
    return u;
  }
  Case(MemUse, w, this) {
    auto u = w;
    if (copy) u = new MemUse(*w);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  Case(MemEffect, w, this) {
    auto u = w;
    if (copy) u = new MemEffect(*w);
    f1(u->d1);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  Case(MemRead, w, this) {
    auto u = w;
    if (copy) u = new MemRead(*w);
    f1(u->d1);
    f1(u->mem);
    f1(u->addr);
    f3(u->data);
    return u;
  }
  Case(MemWrite, w, this) {
    auto u = w;
    if (copy) u = new MemWrite(*w);
    f1(u->d1);
    f1(u->mem);
    f1(u->addr);
    f1(u->s1);
    f3(u->data);
    return u;
  }
  assert(0);
  return NULL;
}

int UnaryOpInstr::compute(int s1) { return op.compute(s1); }
int UnaryOp::compute(int s1) {
  switch (type) {
    case UnaryOp::LNOT:
      return !s1;
    case UnaryOp::NEG:
      return -s1;
    case UnaryOp::ID:
      return s1;
    default:
      assert(0);
      return 0;
  }
}

int BinaryOpInstr::compute(int s1, int s2) { return op.compute(s1, s2); }
int BinaryOp::compute(int s1, int s2) {
  switch (type) {
    case BinaryOp::ADD:
      return s1 + s2;
    case BinaryOp::SUB:
      return s1 - s2;
    case BinaryOp::MUL:
      return s1 * s2;
    case BinaryOp::DIV:
      return (s2 && !(s1 == -2147483648 && s2 == -1) ? s1 / s2 : 0);
    case BinaryOp::LESS:
      return (s1 < s2);
    case BinaryOp::LEQ:
      return (s1 <= s2);
    case BinaryOp::EQ:
      return (s1 == s2);
    case BinaryOp::NEQ:
      return (s1 != s2);
    case BinaryOp::MOD:
      return (s2 ? s1 % s2 : 0);
    default:
      assert(0);
      return 0;
  }
}

void map_use(NormalFunc *f, const std::unordered_map<Reg, Reg> &mp_reg) {
  f->for_each([&](Instr *x) {
    x->map_use([&](Reg &r) {
      auto it = mp_reg.find(r);
      if (it != mp_reg.end()) r = it->second;
    });
  });
}

}  // namespace IR
