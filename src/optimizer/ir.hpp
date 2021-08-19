#pragma once

#include <algorithm>
#include <cassert>
#include <deque>
#include <functional>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <optional>
#include <queue>
#include <set>
#include <stack>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace IR {
using std::function;
using std::list;
using std::ostream;
using std::set;
using std::string;
using std::to_string;
using std::unique_ptr;
using std::vector;

struct CompileUnit;
struct Func;
struct LibFunc;
struct NormalFunc;
struct MemScope;
struct MemObject;
struct BB;
struct Instr;
struct Reg;

struct Printable {
  virtual void print(ostream &os) const = 0;
  virtual ~Printable() {}
};

ostream &operator<<(ostream &os, const Printable &r);

template <class T>
ostream &operator<<(ostream &os, const std::optional<T> &r) {
  if (r)
    os << r.value();
  else
    os << "(nullopt)";
  return os;
}

struct Reg : Printable {
  int id;
  Reg(int id = 0) : id(id) {}
  // id is unique in a NormalFunc
  void print(ostream &os) const override;
  bool operator==(const Reg &r) const { return id == r.id; }
  bool operator<(const Reg &r) const { return id < r.id; }
};
}  // namespace IR
namespace std {
template <>
struct hash<IR::Reg> {
  size_t operator()(const IR::Reg &r) const { return r.id; }
};
}  // namespace std
namespace IR {
struct MemObject : Printable {
  // data stored in memory
  string name;
  int size = 0;    // number of bytes, size%4==0
  int offset = 0;  // offset from gp or sp, computed after machine irrelavant
                   // optimization
  bool global;
  bool arg = 0;
  // global=0 arg=0: local variable, but not arg
  // global=0 arg=1: arg of array type
  // global=1 arg=0: global variable
  // global=1 arg=1: stdin or stdout, for side effect analysis
  void *initial_value = NULL;
  bool is_int = 1;
  // only for global=1, NULL: zero initialization, non-NULL: init by these bytes

  bool is_const = 0;
  // computed in optimize_passes

  std::vector<int> dims;
  // only for int array, array dim size

  void init(int32_t *x, int size) {
    initial_value = x;
    is_int = 1;
    this->size = size;
  }
  void init(char *x, int size) {
    initial_value = x;
    is_int = 0;
    this->size = size;
  }
  bool is_single_var() { return !arg && size == 4 && dims.empty(); }
  void print(ostream &os) const override;
  int at(int x) {
    if (!(0 <= x && x + 4 <= size)) return 0;  // assert
    if (x % 4 != 0) return 0;                  // assert
    if (!initial_value) return 0;
    return ((int *)initial_value)[x / 4];
  }

 private:
  friend struct MemScope;
  MemScope *fa;
  MemObject(string name, bool global) : name(name), global(global) {}
};

struct MemScope : Printable {
  string name;
  bool global;
  // global=0: stack frame
  // global=1: global variables
  vector<unique_ptr<MemObject>> objects;
  // list of MemObjects in this scope
  std::unordered_map<int, MemObject *> array_args;
  // map arg of array type to global=0,arg=1 MemObject
  std::unordered_map<MemObject *, int> array_arg_id;
  // map global=0,arg=1 MemObject to arg id in 0..arg_cnt-1
  int size;
  // computed after machine irrelavant optimization
  MemObject *new_MemObject(string _name) {
    MemObject *m = new MemObject(name + "::" + _name, global);
    objects.emplace_back(m);
    m->fa = this;
    return m;
  }
  void for_each(function<void(MemObject *)> f) {
    for (auto &x : objects) f(x.get());
  }
  void for_each(function<void(MemObject *, MemObject *)> f) {
    for (auto &x : objects) {
      auto y = new MemObject(*x);
      f(x.get(), y);
    }
  }
  void add(MemObject *m) {
    assert(!m->global);
    m->name = name + "::" + m->name;
    objects.emplace_back(m);
    m->fa = this;
  }
  void set_arg(int id, MemObject *m) {
    assert(has(m));
    assert(!array_args.count(id));
    assert(!array_arg_id.count(m));
    m->arg = 1;
    array_args[id] = m;
    array_arg_id[m] = id;
  }
  bool has(MemObject *m) { return m && m->fa == this; }
  void print(ostream &os) const override;

 private:
  friend struct CompileUnit;
  friend struct NormalFunc;
  MemScope(string name, bool global) : name(name), global(global) {}
};

struct BB;

struct Instr : Printable {
  // IR instruction
  void map_use(function<void(Reg &)> f1);
  void map_BB(std::function<void(BB *&)> f) {
    map([](auto &x) {}, f, [](auto &x) {}, 0);
  }
  Instr *copy() {
    return map([](Reg &) {}, [](BB *&) {}, [](MemObject *&) {}, 1);
  }
  Instr *map(function<void(Reg &)> f1, function<void(BB *&)> f2,
             function<void(MemObject *&)> f3, bool copy = 1);
  // copy this Instr, and map each Reg by f1, map each BB* by f2, map each
  // MemObject by f3
};
struct PhiInstr;
struct BB : Printable {
  // basic block
  string name;
  list<unique_ptr<Instr>> instrs;
  int id;
  bool disable_schedule_early = 0;
  // list of instructions in this basic block
  // the last one is ControlInstr, others are RegWriteInstr or StoreInstr
  void print(ostream &os) const override;
  void for_each(function<void(Instr *)> f) {
    for (auto &x : instrs) f(x.get());
  }
  bool for_each_until(function<bool(Instr *)> f) {
    for (auto &x : instrs)
      if (f(x.get())) return 1;
    return 0;
  }
  void push_front(Instr *x) { instrs.emplace_front(x); }
  void push(Instr *x) { instrs.emplace_back(x); }
  void push1(Instr *x) { instrs.insert(--instrs.end(), unique_ptr<Instr>(x)); }
  void pop() { instrs.pop_back(); }
  Instr *back() { return instrs.back().get(); }
  void map_BB(std::function<void(BB *&)> f) {
    for (auto &x : instrs) x->map_BB(f);
  }

 private:
  friend struct NormalFunc;
  BB(string name) : name(name) {}
};

struct Func : Printable {
  // function
  string name;
  bool ignore_return_value = 0;
  Func(string name) : name(name) {}
};

struct LibFunc : Func {
  // extern function
  void print(ostream &os) const override { os << "LibFunc: " << name; }
  std::unordered_map<int, bool> array_args;
  // read/write args of array type
  // (arg_id,1): read and write
  // (arg_id,0): read only
  bool in = 0,
       out = 0;  // IO side effect, in: stdin changed, out: stdout changed
 private:
  friend struct CompileUnit;
  LibFunc(string name) : Func(name) {}
};

struct NormalFunc : Func {
  // function defined in compile unit (.sy file)
  MemScope scope;
  // local variables on stack, and args of array type
  BB *entry = NULL;
  // first basic block to excute
  vector<unique_ptr<BB>> bbs;
  // list of basic blocks in this function
  int max_reg_id = 0, max_bb_id = 0;
  // for id allocation
  vector<string> reg_names;

  std::unordered_set<Reg> thread_local_regs;
  Reg new_Reg() { return new_Reg("R" + to_string(max_reg_id + 1)); }
  Reg new_Reg(string _name) {
    reg_names.push_back(_name);
    // reg id : 1 .. max_reg_id
    // 1 .. param_cnt : arguments
    return Reg(++max_reg_id);
  }
  BB *new_BB(string _name = "BB") {
    BB *bb = new BB(name + "::" + _name + to_string(++max_bb_id));
    bbs.emplace_back(bb);
    return bb;
  }
  void for_each(function<void(BB *)> f) {
    for (auto &bb : bbs) f(bb.get());
  }
  bool for_each_until(function<bool(BB *)> f) {
    for (auto &bb : bbs)
      if (f(bb.get())) return 1;
    return 0;
  }
  void for_each(function<void(Instr *)> f) {
    for (auto &bb : bbs) bb->for_each(f);
  }
  void print(ostream &os) const override;
  string get_name(Reg r) const { return reg_names.at(r.id); }

 private:
  friend struct CompileUnit;
  NormalFunc(string name) : Func(name), scope(name, 0) {
    reg_names.push_back("?");
  }
};

struct CompileUnit : Printable {
  // the whole program
  MemScope scope;  // global arrays
  std::map<string, unique_ptr<NormalFunc>> funcs;
  // functions defined in .sy file
  std::map<string, unique_ptr<LibFunc>> lib_funcs;
  // functions defined in library
  CompileUnit();
  NormalFunc *new_NormalFunc(string _name) {
    NormalFunc *f = new NormalFunc(_name);
    funcs[_name] = unique_ptr<NormalFunc>(f);
    return f;
  }
  void print(ostream &os) const override;
  void map(function<void(CompileUnit &)> f) { f(*this); }
  void for_each(function<void(NormalFunc *)> f) {
    for (auto &kv : funcs) f(kv.second.get());
  }
  void for_each(function<void(MemScope &)> f) {
    f(scope);
    for (auto &kv : funcs) f(kv.second->scope);
  }

 private:
  LibFunc *new_LibFunc(string _name, bool ignore_return_value) {
    LibFunc *f = new LibFunc(_name);
    f->ignore_return_value = ignore_return_value;
    lib_funcs[_name] = unique_ptr<LibFunc>(f);
    return f;
  }
};

struct UnaryOp : Printable {
  enum Type {
    LNOT = 0,
    NEG = 1,
    ID = 2,
  } type;
  int compute(int x);
  UnaryOp(Type x) : type(x) {}
  const char *get_name() const {
    static const char *names[] = {"!", "-", "+"};
    return names[(int)type];
  }
  void print(ostream &os) const override { os << get_name(); }
};

struct BinaryOp : Printable {
  enum Type {
    ADD = 0,
    SUB = 1,
    MUL = 2,
    DIV = 3,
    LESS = 4,
    LEQ = 5,
    EQ = 6,
    NEQ = 7,
    MOD = 8
  } type;
  BinaryOp(Type x) : type(x) {}
  int compute(int x, int y);
  bool comm() {
    return type == ADD || type == MUL || type == EQ || type == NEQ;
  }
  const char *get_name() const {
    static const char *names[] = {"+",  "-",  "*",  "/", "<",
                                  "<=", "==", "!=", "%"};
    return names[(int)type];
  }
  void print(ostream &os) const override { os << get_name(); }
};

struct RegWriteInstr : Instr {
  // any instr that write a reg
  // no instr write multiple regs
  Reg d1;
  RegWriteInstr(Reg d1) : d1(d1) {}
};

struct LoadAddr : RegWriteInstr {
  // load address of offset to d1
  // d1 = addr
  MemObject *offset;
  LoadAddr(Reg d1, MemObject *offset) : RegWriteInstr(d1), offset(offset) {
    assert(!offset->arg);
  }
  void print(ostream &os) const override;
};

struct LoadConst : RegWriteInstr {
  // load value to d1
  // d1 = value
  int value;
  LoadConst(Reg d1, int value) : RegWriteInstr(d1), value(value) {}
  void print(ostream &os) const override;
};

struct LoadArg : RegWriteInstr {
  // load arg with arg_id=id to d1
  // d1 = arg
  int id;
  LoadArg(Reg d1, int id) : RegWriteInstr(d1), id(id) {}
  void print(ostream &os) const override;
};

struct UnaryOpInstr : RegWriteInstr {
  // d1 = op(s1)
  UnaryOpInstr(Reg d1, Reg s1, UnaryOp op)
      : RegWriteInstr(d1), s1(s1), op(op) {}
  Reg s1;
  UnaryOp op;
  int compute(int x);
  void print(ostream &os) const override;
};

struct BinaryOpInstr : RegWriteInstr {
  // d1 = op(s1,s2)
  BinaryOpInstr(Reg d1, Reg s1, Reg s2, BinaryOp op)
      : RegWriteInstr(d1), s1(s1), s2(s2), op(op) {}
  Reg s1, s2;
  BinaryOp op;
  int compute(int x, int y);
  void print(ostream &os) const override;
};

struct LoadInstr : RegWriteInstr {
  // memory read, used in ssa, but not in array-ssa
  // d1 = M[addr]
  LoadInstr(Reg d1, Reg addr) : RegWriteInstr(d1), addr(addr) {}
  Reg addr;
  void print(ostream &os) const override;
};

struct StoreInstr : Instr {
  // memory write, used in ssa, but not in array-ssa
  // M[addr] = s1
  StoreInstr(Reg addr, Reg s1) : addr(addr), s1(s1) {}
  Reg addr;
  Reg s1;
  void print(ostream &os) const override;
};

struct ControlInstr : Instr {
  // any instr except call that change PC
};

struct JumpInstr : ControlInstr {
  // PC = target
  BB *target;
  JumpInstr(BB *target) : target(target) {}
  void print(ostream &os) const override;
};

struct BranchInstr : ControlInstr {
  // if (cond) then PC = value
  Reg cond;
  BB *target1, *target0;
  BranchInstr(Reg cond, BB *target1, BB *target0)
      : cond(cond), target1(target1), target0(target0) {}
  void print(ostream &os) const override;
};

struct ReturnInstr : ControlInstr {
  // return s1
  Reg s1;
  bool ignore_return_value;
  ReturnInstr(Reg s1, bool ignore_return_value)
      : s1(s1), ignore_return_value(ignore_return_value) {}
  void print(ostream &os) const override;
};

struct MemUse;
struct CallInstr : RegWriteInstr {
  // d1 = f(args[0],args[1],...)
  vector<Reg> args;
  Func *f;
  bool ignore_return_value, pure = 0;
  list<MemUse *> in;  // record mem-use and mem-effect in array ssa
  CallInstr(Reg d1, Func *f, vector<Reg> args, bool ignore_return_value)
      : RegWriteInstr(d1),
        args(args),
        f(f),
        ignore_return_value(ignore_return_value) {}
  void print(ostream &os) const override;
};

struct LocalVarDef : Instr {
  // define variable as uninitialized array of bytes
  MemObject *data;
  // array from arg: data->global=0 data->size=0 data->arg=1
  // local variable: data->global=0
  LocalVarDef(MemObject *data) : data(data) { assert(!data->global); }
  void print(ostream &os) const override;
};

struct ArrayIndex : RegWriteInstr {
  // d1 = s1+s2*size, 0 <= s2 < limit
  Reg s1, s2;
  int size, limit;
  ArrayIndex(Reg d1, Reg s1, Reg s2, int size, int limit)
      : RegWriteInstr(d1), s1(s1), s2(s2), size(size), limit(limit) {}
  void print(ostream &os) const override;
};

// for ssa

struct PhiInstr : RegWriteInstr {
  // only for ssa and array-ssa
  vector<std::pair<Reg, BB *>> uses;
  PhiInstr(Reg d1) : RegWriteInstr(d1) {}
  void add_use(Reg r, BB *pos) { uses.emplace_back(r, pos); }
  void print(ostream &os) const override;
};

// for array-ssa

struct MemDef : RegWriteInstr {
  // only for array-ssa
  // d1:int
  MemObject *data;
  // array from arg: data->global=0 data->size=0 data->arg=1 //TODO
  // local array def: data->global=0
  // global array intro: data->global=1
  MemDef(Reg d1, MemObject *data) : RegWriteInstr(d1), data(data) {
    assert(data);
  }
  void print(ostream &os) const override;
};

struct MemUse : Instr {
  // only for array-ssa
  // s1 is used
  // the concrete use is unknown
  // usually inserted before call
  Reg s1;
  MemObject *data;
  MemUse(Reg s1, MemObject *data) : s1(s1), data(data) {}
  void print(ostream &os) const override;
};

struct MemEffect : RegWriteInstr {
  // only for array-ssa
  // d1 is updated from s1
  // the concrete update is unknown
  // usually inserted after call
  Reg s1;
  MemObject *data;
  MemEffect(Reg d1, Reg s1, MemObject *data)
      : RegWriteInstr(d1), s1(s1), data(data) {}
  void print(ostream &os) const override;
};

struct MemRead : RegWriteInstr {
  // only for array-ssa
  // d1:int = read mem at addr
  Reg mem, addr;
  MemObject *data;
  MemRead(Reg d1, Reg mem, Reg addr, MemObject *data)
      : RegWriteInstr(d1), mem(mem), addr(addr), data(data) {}
  void print(ostream &os) const override;
};

struct MemWrite : RegWriteInstr {
  // only for array-ssa
  // d1:mem = write mem at addr with value s1
  Reg mem, addr, s1;
  MemObject *data;
  MemWrite(Reg d1, Reg mem, Reg addr, Reg s1, MemObject *data)
      : RegWriteInstr(d1), mem(mem), addr(addr), s1(s1), data(data) {}
  void print(ostream &os) const override;
};

#define Case(T, a, b) if (auto a = dynamic_cast<T *>(b))
#define CaseNot(T, b)                  \
  if (auto _ = dynamic_cast<T *>(b)) { \
  } else

// for each (R1,R2):mp_reg, change the usage of R1 to R2, but defs are not
// changed
void map_use(NormalFunc *f, const std::unordered_map<Reg, Reg> &mp_reg);

template <class T>
std::function<void(T)> repeat(bool (*f)(T), int max = 1000000) {
  return [=](T x) {
    for (int i = 0; i < max && f(x); ++i)
      ;
  };
}

template <class T>
std::function<void(T &)> partial_map(T from, T to) {
  return [from, to](T &x) {
    if (x == from) x = to;
  };
}
template <class T>
std::function<void(T &)> partial_map(std::unordered_map<T, T> &mp) {
  auto *p_mp = &mp;
  return [p_mp](T &x) {
    auto it = p_mp->find(x);
    if (it != p_mp->end()) x = it->second;
  };
}

}  // namespace IR

namespace std {
template <class T1, class T2>
struct hash<pair<T1, T2>> {
  size_t operator()(const pair<T1, T2> &r) const {
    return hash<T1>()(r.first) * 1844677 + hash<T2>()(r.second) * 41;
  }
};
template <>
struct hash<tuple<int, int, int>> {
  size_t operator()(const tuple<int, int, int> &r) const {
    return get<0>(r) * 293999 + get<1>(r) * 1234577 + get<2>(r) * 29;
  }
};
}  // namespace std

namespace IR {
struct PrintContext {
  const CompileUnit *c = NULL;
  const NormalFunc *f = NULL;
  std::unordered_map<Instr *, string> instr_comment;
};

extern PrintContext print_ctx;

struct SetPrintContext {
  const NormalFunc *f0 = NULL;
  SetPrintContext(const NormalFunc *f) {
    f0 = print_ctx.f;
    print_ctx.f = f;
  }
  ~SetPrintContext() { print_ctx.f = f0; }
};
}  // namespace IR
