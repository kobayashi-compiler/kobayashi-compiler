#pragma once

#include <cstring>

#include "debug.hpp"
#include "ir.hpp"
#define PassIsEnabled(name) if (!global_config.disabled_passes.count(name))
using namespace IR;

struct DomTreeState {
  std::vector<BB *> in, out, dom_ch, loop_ch;
  // in: in edges to self
  // out: out edges from self
  // dom_ch: children on dom tree
  // loop_ch: children on loop tree

  int dfn = 0, dfn2 = 0, tag = 0;
  bool loop_rt = 0;
  // self is a root of a natural loop

  BB *dom_fa = NULL, *loop_fa = NULL, *self = NULL, *loop_pre = NULL,
     *loop_pre_exit = NULL, *loop_last = NULL;
  bool loop_simple = 0;
  int loop_depth = 0;
  // dom_fa: parent on dom tree
  // loop_fa: parent on loop tree
  // self: this basic block

  std::unordered_set<BB *> DF, loop_exit;

  // self dominates s.self
  bool dom(const DomTreeState &s) const {
    return dfn <= s.dfn && s.dfn <= dfn2;
  }

  // self strictly dominates s.self
  bool sdom(const DomTreeState &s) const {
    return dfn < s.dfn && s.dfn <= dfn2;
  }

  // get the most inner loop that self is in it
  BB *get_loop_rt() const { return loop_rt ? self : loop_fa; }
};

typedef std::unordered_map<BB *, DomTreeState> DomTree;

struct IndVarInfo {
  Reg base, step;
  BinaryOp::Type op;
  BB *bb;
};

struct SimpleTypeInfo {
  MemObject *base = NULL;
};

struct ArrayRW {
  struct ArgInfo {
    bool may_write = 0;
    std::unordered_set<MemObject *> maybe;
  };
  std::unordered_map<MemObject *, bool>
      global_rw;                            // global array access not from arg
  std::unordered_map<int, ArgInfo> arg_rw;  // arg array access
  std::vector<CallInstr *> rec_calls, normal_calls;
  std::unordered_map<MemObject *, std::unordered_set<MemObject *>> alias;
  std::unordered_map<Reg, SimpleTypeInfo> types;
};

struct ArrayRWInfo {
  std::unordered_set<Reg> r_addr, w_addr;
  bool r_any = 0, w_any = 0;
  void update(const ArrayRWInfo &v) {
    for (Reg x : v.r_addr) r_addr.insert(x);
    for (Reg x : v.w_addr) w_addr.insert(x);
    r_any |= v.r_any;
    w_any |= v.w_any;
  }
};

struct ReduceVar {
  Reg base, step;
  BinaryOp::Type op;
  bool ind_var = 0, reduce_var = 0;
  // variables of form: a <- a op b
  // b is const: ind var, value only depend on loop times
  // b is computed in loop: reduce var
};

struct SimpleCond {
  BinaryOp::Type op;  //<,<=
  bool rev;
  Reg ind, c;
};

struct LoopVarInfo {
  // std::unordered_set<Reg> dep; //TODO: effect of branches ?
  MemObject *mem = NULL;  // this is a state of mem, or mem=NULL
  ReduceVar reduce;
};

struct LoopInfo {
  std::unordered_map<MemObject *, ArrayRWInfo> rw;

  bool simple = 0;
  // only for simple loop:
  std::optional<SimpleCond> simple_cond;
  std::unordered_map<Reg, LoopVarInfo> loop_var;
  bool parallel_for = 0;
};

// check whether a Reg is the address in a MemObject
std::unordered_map<Reg, SimpleTypeInfo> simple_type_check(NormalFunc *f);

// NormalFunc side effect analysis
// arg array alias analysis
std::unordered_map<Func *, ArrayRW> get_array_rw(CompileUnit &c);

// check the input
void cfg_check(CompileUnit &c);

// print ssa
void print_ssa(NormalFunc *f);

// remove RegWriteInstr s.t. d1 is not used and no other side effect
void remove_unused_def(NormalFunc *f);

// remove unused NormalFunc
void remove_unused_func(CompileUnit &c);

// remove unused MemObject
void remove_unused_memobj(CompileUnit &c);

// remove pure loop
bool remove_unused_loop(NormalFunc *f);

// remove unreachable BBs, merge some BBs
bool dce_BB(NormalFunc *f);

// build dominator tree
// build loop tree
DomTree build_dom_tree(NormalFunc *f);

// construct ssa from tac
// or reconstruct ssa after some optimizations
// assume x is a well-formed def in ssa when check(x)=0
void ssa_construction(
    NormalFunc *f, std::function<bool(Reg)> check = [](Reg r) { return 1; });

void array_ssa_passes(CompileUnit &c, int _check_loop = 0);

// merge arrays with similar access pattern
void array_to_struct(CompileUnit &c);

// rewrite ArrayIndex as * and +
void array_index_to_muladd(NormalFunc *f);

// constant propegation
// copy propegation
// remove duplicated computions
// move instrs as early as possible
void gvn(CompileUnit &c);

// move instrs to a good position
void gcm(CompileUnit &c);

// move if out of while
bool gcm_move_branch_out_of_loop(NormalFunc *f);

// TODO
void ind_var_discovery(NormalFunc *f);

// rewrite local variable access as register access
void mem2reg(CompileUnit &c);

// inline all non-recursive calls
// warning: cannot used on ssa now
void func_inline(CompileUnit &c);

void instruction_scheduling(CompileUnit &c);

// compute MemObject::offset and MemScope::size
void compute_data_offset(CompileUnit &c);

// sort the basic blocks to reduce jumps in machine code
void code_reorder(NormalFunc *f);

// run IR
int exec(CompileUnit &c);

// compute def instr of Reg
std::unordered_map<Reg, RegWriteInstr *> build_defs(NormalFunc *f);

// find constants
std::unordered_map<Reg, int> build_const_int(NormalFunc *f);

// compute use count of Reg
std::unordered_map<Reg, int> build_use_count(NormalFunc *f);

// compute each Instr's parent BB
std::unordered_map<Instr *, BB *> build_in2bb(NormalFunc *f);

// compute def instr of Reg in a BB
void get_defs(std::unordered_map<Reg, RegWriteInstr *> &ret, BB *w);

// compute use count of Reg in a BB
void get_use_count(std::unordered_map<Reg, int> &ret, BB *w);

// get out edges of a BB
std::vector<BB *> get_BB_out(BB *w);

// topology sort the functions, ret[i] can only call ret[0..i]
std::vector<NormalFunc *> get_call_order(CompileUnit &c);

// dfs on dominator tree, pre-order
void dom_tree_dfs(DomTree &S, std::function<void(BB *)> F);

// dfs on dominator tree, in reversed order of dom_tree_dfs
void dom_tree_rdfs(DomTree &S, std::function<void(BB *)> F);

// traverse loop tree from w
void loop_tree_for_each(DomTree &S, BB *w, std::function<void(BB *)> F);

// check whether w is in loop
bool in_loop(DomTree &S, BB *w, BB *loop);

// print CFG/dominator tree/loop tree info
void print_cfg(DomTree &S, NormalFunc *f);
void print_dom_tree(DomTree &S, NormalFunc *f);
void print_loop_tree(DomTree &S, NormalFunc *f);

// all passes
void optimize_passes(CompileUnit &c);

// when bb_old is renamed to bb_cur, update phi uses second from bb_old to
// bb_cur
void phi_src_rewrite(BB *bb_cur, BB *bb_old);

// find properties about each loop
std::unordered_map<BB *, LoopInfo> check_loop(NormalFunc *f, int type);

// unroll  do xxx while(y); to xxx while(y)xxx
void do_while_to_while(NormalFunc *f);

// unroll some special loop like for(int i=L;i<R;i+=C){}
void loop_unroll(const std::unordered_map<BB *, LoopInfo> &LI, NormalFunc *f);

// similar to pragma omp parallel for
void loop_parallel(const std::unordered_map<BB *, LoopInfo> &LI, CompileUnit &c,
                   NormalFunc *f);

// find MemObject correspond to each Reg
std::unordered_map<Reg, MemObject *> infer_array_regs(NormalFunc *f);

// find frequently visited BBs
std::unordered_map<BB *, double> estimate_BB_prob(NormalFunc *f);
