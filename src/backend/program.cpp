#include "program.hpp"

#include <bitset>
#include <functional>
#include <map>
#include <memory>
#include <ostream>
#include <queue>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "archinfo.hpp"
#include "backend_passes.hpp"
#include "coloring_alloc.hpp"
#include "common.hpp"
#include "inst.hpp"
#include "pass.hpp"
#include "simple_coloring_alloc.hpp"

using std::bitset;
using std::deque;
using std::make_unique;
using std::map;
using std::ostream;
using std::pair;
using std::set;
using std::string;
using std::unique_ptr;
using std::vector;

namespace ARMv7 {

Block::Block(string _name) : prob(1), name(_name), label_used(false) {}

void Block::construct(IR::BB *ir_bb, Func *func, MappingInfo *info,
                      Block *next_block, map<Reg, CmpInfo> &cmp_info) {
  for (auto &i : ir_bb->instrs) {
    IR::Instr *cur = i.get();
    if (auto loadaddr = dynamic_cast<IR::LoadAddr *>(cur)) {
      Reg dst = info->from_ir_reg(loadaddr->d1);
      if (loadaddr->offset->global) {
        push_back(load_symbol_addr(
            dst, mangle_global_var_name(loadaddr->offset->name)));
        func->symbol_reg[dst] = mangle_global_var_name(loadaddr->offset->name);
      } else {
        push_back(make_unique<LoadStackAddr>(
            dst, 0, info->obj_mapping[loadaddr->offset]));
        func->stack_addr_reg[dst] = std::pair<StackObject *, int32_t>{
            info->obj_mapping[loadaddr->offset], 0};
      }
    } else if (auto loadconst = dynamic_cast<IR::LoadConst *>(cur)) {
      Reg dst = info->from_ir_reg(loadconst->d1);
      func->constant_reg[dst] = loadconst->value;
      push_back(load_imm(dst, loadconst->value));
    } else if (auto loadarg = dynamic_cast<IR::LoadArg *>(cur)) {
      push_back(make_unique<MoveReg>(info->from_ir_reg(loadarg->d1),
                                     func->arg_reg[loadarg->id]));
    } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(unary->d1),
          src = info->from_ir_reg(unary->s1);
      switch (unary->op.type) {
        case IR::UnaryOp::LNOT:
          push_back(make_unique<MoveImm>(MoveImm::Mov, dst, 0));
          push_back(make_unique<RegImmCmp>(RegImmCmp::Cmp, src, 0));
          push_back(set_cond(make_unique<MoveImm>(MoveImm::Mov, dst, 1), Eq));
          // TODO: this can be done better with "rsbs dst, src, #0; adc dst,
          // src, dst" or "clz dst, src; lsr dst, dst, #5"
          break;
        case IR::UnaryOp::NEG:
          push_back(make_unique<RegImmInst>(RegImmInst::RevSub, dst, src, 0));
          break;
        case IR::UnaryOp::ID:
          push_back(make_unique<MoveReg>(dst, src));
          break;
        default:
          unreachable();
      }
    } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(binary->d1),
          s1 = info->from_ir_reg(binary->s1),
          s2 = info->from_ir_reg(binary->s2);
      if (binary->op.type == IR::BinaryOp::ADD ||
          binary->op.type == IR::BinaryOp::SUB ||
          binary->op.type == IR::BinaryOp::MUL ||
          binary->op.type == IR::BinaryOp::DIV) {
        push_back(make_unique<RegRegInst>(
            RegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else if (binary->op.type == IR::BinaryOp::LESS ||
                 binary->op.type == IR::BinaryOp::LEQ ||
                 binary->op.type == IR::BinaryOp::EQ ||
                 binary->op.type == IR::BinaryOp::NEQ) {
        push_back(make_unique<MoveImm>(MoveImm::Mov, dst, 0));
        push_back(make_unique<RegRegCmp>(RegRegCmp::Cmp, s1, s2));
        push_back(set_cond(make_unique<MoveImm>(MoveImm::Mov, dst, 1),
                           from_ir_binary_op(binary->op.type)));
        cmp_info[dst].cond = from_ir_binary_op(binary->op.type);
        cmp_info[dst].lhs = s1;
        cmp_info[dst].rhs = s2;
      } else if (binary->op.type == IR::BinaryOp::MOD) {
        Reg k = info->new_reg();
        push_back(make_unique<RegRegInst>(RegRegInst::Div, k, s1, s2));
        push_back(make_unique<ML>(ML::Mls, dst, s2, k, s1));
      } else
        unreachable();
    } else if (auto load = dynamic_cast<IR::LoadInstr *>(cur)) {
      Reg dst = info->from_ir_reg(load->d1),
          addr = info->from_ir_reg(load->addr);
      push_back(make_unique<Load>(dst, addr, 0));
    } else if (auto store = dynamic_cast<IR::StoreInstr *>(cur)) {
      Reg addr = info->from_ir_reg(store->addr),
          src = info->from_ir_reg(store->s1);
      push_back(make_unique<Store>(src, addr, 0));
    } else if (auto jump = dynamic_cast<IR::JumpInstr *>(cur)) {
      Block *jump_target = info->block_mapping[jump->target];
      if (jump_target != next_block)
        push_back(make_unique<Branch>(jump_target));
      out_edge.push_back(jump_target);
      jump_target->in_edge.push_back(this);
    } else if (auto branch = dynamic_cast<IR::BranchInstr *>(cur)) {
      Reg cond = info->from_ir_reg(branch->cond);
      Block *true_target = info->block_mapping[branch->target1],
            *false_target = info->block_mapping[branch->target0];
      if (cmp_info.find(cond) != cmp_info.end()) {
        push_back(make_unique<RegRegCmp>(RegRegCmp::Cmp, cmp_info[cond].lhs,
                                         cmp_info[cond].rhs));
        if (false_target == next_block)
          push_back(
              set_cond(make_unique<Branch>(true_target), cmp_info[cond].cond));
        else if (true_target == next_block)
          push_back(set_cond(make_unique<Branch>(false_target),
                             logical_not(cmp_info[cond].cond)));
        else {
          push_back(
              set_cond(make_unique<Branch>(true_target), cmp_info[cond].cond));
          push_back(make_unique<Branch>(false_target));
        }
      } else {
        push_back(make_unique<RegImmCmp>(RegImmCmp::Cmp, cond, 0));
        if (false_target == next_block)
          push_back(set_cond(make_unique<Branch>(true_target), Ne));
        else if (true_target == next_block)
          push_back(set_cond(make_unique<Branch>(false_target), Eq));
        else {
          push_back(set_cond(make_unique<Branch>(true_target), Ne));
          push_back(make_unique<Branch>(false_target));
        }
      }
      out_edge.push_back(true_target);
      out_edge.push_back(false_target);
      true_target->in_edge.push_back(this);
      false_target->in_edge.push_back(this);
    } else if (auto ret = dynamic_cast<IR::ReturnInstr *>(cur)) {
      if (ret->ignore_return_value) {
        push_back(make_unique<Return>(false));
      } else {
        push_back(make_unique<MoveReg>(Reg{ARGUMENT_REGISTERS[0]},
                                       info->from_ir_reg(ret->s1)));
        push_back(make_unique<Return>(true));
      }
    } else if (auto call = dynamic_cast<IR::CallInstr *>(cur)) {
      for (size_t i = call->args.size() - 1; i < call->args.size(); --i)
        if (static_cast<int>(i) >= ARGUMENT_REGISTER_COUNT) {
          push_back(
              make_unique<Push>(vector<Reg>{info->from_ir_reg(call->args[i])}));
        } else {
          push_back(make_unique<MoveReg>(Reg{ARGUMENT_REGISTERS[i]},
                                         info->from_ir_reg(call->args[i])));
        }
      push_back(make_unique<FuncCall>(call->f->name,
                                      static_cast<int>(call->args.size()),
                                      !call->ignore_return_value));
      if (static_cast<int>(call->args.size()) > ARGUMENT_REGISTER_COUNT)
        push_back(sp_move(
            (static_cast<int>(call->args.size()) - ARGUMENT_REGISTER_COUNT) *
            INT_SIZE));
      if (!call->ignore_return_value)
        push_back(make_unique<MoveReg>(info->from_ir_reg(call->d1),
                                       Reg{ARGUMENT_REGISTERS[0]}));
      if (call->f->name == ".fork") {
        func->spilling_reg.insert(info->from_ir_reg(call->d1));
        debug << "thread_id: " << call->d1 << " -> "
              << info->from_ir_reg(call->d1) << " is forbidden to be spilled\n";
      }
    } else if (auto local_var_def = dynamic_cast<IR::LocalVarDef *>(cur)) {
      // do nothing
    } else if (auto array_index = dynamic_cast<IR::ArrayIndex *>(cur)) {
      Reg dst = info->from_ir_reg(array_index->d1),
          s1 = info->from_ir_reg(array_index->s1),
          s2 = info->from_ir_reg(array_index->s2);
      // TODO: optimize when size=2^k
      Reg step = info->new_reg();
      push_back(load_imm(step, array_index->size));
      push_back(make_unique<ML>(ML::Mla, dst, s2, step, s1));
    } else if (auto phi = dynamic_cast<IR::PhiInstr *>(cur)) {
      // do nothing
    } else
      unreachable();
  }
}

void Block::push_back(unique_ptr<Inst> inst) {
  insts.push_back(std::move(inst));
}

void Block::push_back(std::list<unique_ptr<Inst>> inst_list) {
  for (auto &i : inst_list) insts.push_back(std::move(i));
}

void Block::insert_before_jump(unique_ptr<Inst> inst) {
  auto i = insts.end();
  while (i != insts.begin()) {
    auto prev_i = std::prev(i);
    if ((*prev_i)->as<Branch>()) {
      i = prev_i;
    } else {
      break;
    }
  }
  insts.insert(i, std::move(inst));
}

void Block::gen_asm(ostream &out, AsmContext *ctx) {
  ctx->temp_sp_offset = 0;
  if (label_used) out << name << ":\n";
  for (auto &i : insts) i->gen_asm(out, ctx);
}

void Block::print(ostream &out) {
  out << '\n' << name << ":\n";
  for (auto &i : insts) i->print(out);
}

MappingInfo::MappingInfo() : reg_n(RegCount) {}

Reg MappingInfo::new_reg() { return Reg{reg_n++}; }

Reg MappingInfo::from_ir_reg(IR::Reg ir_reg) {
  auto it = reg_mapping.find(ir_reg.id);
  if (it != reg_mapping.end()) return it->second;
  Reg ret = new_reg();
  reg_mapping[ir_reg.id] = ret;
  return ret;
}

Func::Func(Program *prog, std::string _name, IR::NormalFunc *ir_func)
    : name(_name), entry(nullptr), reg_n(0) {
  MappingInfo info;
  for (size_t i = 0; i < ir_func->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir_func->scope.objects[i].get();
    if (cur->size == 0) continue;
    unique_ptr<StackObject> res = make_unique<StackObject>();
    res->size = cur->size;
    res->position = -1;
    info.obj_mapping[cur] = res.get();
    stack_objects.push_back(std::move(res));
  }
  entry = new Block(".entry_" + name);
  blocks.emplace_back(entry);
  std::unordered_map<IR::BB *, double> bb_prob = estimate_BB_prob(ir_func);
  for (size_t i = 0; i < ir_func->bbs.size(); ++i) {
    IR::BB *cur = ir_func->bbs[i].get();
    string cur_name = ".L" + std::to_string(prog->block_n++);
    unique_ptr<Block> res = make_unique<Block>(cur_name);
    res->prob = bb_prob[cur];
    info.block_mapping[cur] = res.get();
    info.rev_block_mapping[res.get()] = cur;
    blocks.push_back(std::move(res));
  }
  int arg_n = 0;
  for (auto &bb : ir_func->bbs)
    for (auto &inst : bb->instrs)
      if (auto *cur = dynamic_cast<IR::LoadArg *>(inst.get()))
        arg_n = std::max(arg_n, cur->id + 1);
  for (int i = 0; i < arg_n; ++i) {
    Reg cur_arg = info.new_reg();
    if (i < ARGUMENT_REGISTER_COUNT) {
      entry->push_back(
          make_unique<MoveReg>(cur_arg, Reg{ARGUMENT_REGISTERS[i]}));
    } else {
      unique_ptr<StackObject> t = make_unique<StackObject>();
      t->size = INT_SIZE;
      t->position = -1;
      entry->push_back(make_unique<LoadStack>(cur_arg, 0, t.get()));
      caller_stack_object.push_back(std::move(t));
    }
    arg_reg.push_back(cur_arg);
  }
  Block *real_entry = info.block_mapping[ir_func->entry];
  if (blocks[1].get() != real_entry)
    entry->push_back(make_unique<Branch>(real_entry));
  entry->out_edge.push_back(real_entry);
  real_entry->in_edge.push_back(entry);
  map<Reg, CmpInfo> cmp_info;
  for (size_t i = 0; i < blocks.size(); ++i)
    if (blocks[i].get() != entry) {
      IR::BB *cur_ir_bb = info.rev_block_mapping[blocks[i].get()];
      Block *next_block = nullptr;
      if (i + 1 < blocks.size()) next_block = blocks[i + 1].get();
      blocks[i]->construct(
          cur_ir_bb, this, &info, next_block,
          cmp_info);  // maintain in_edge, out_edge, call_subroutine,
                      // reg_mapping, ignore phi function
    }
  struct PendingMove {
    Block *block;
    Reg to, from;
  };
  vector<PendingMove> pending_moves;
  for (auto &bb : ir_func->bbs)
    for (auto &inst : bb->instrs)
      if (auto *cur = dynamic_cast<IR::PhiInstr *>(inst.get()))
        for (auto &prev : cur->uses) {
          Block *b = info.block_mapping[prev.second];
          Reg mid = info.new_reg();
          b->insert_before_jump(
              make_unique<MoveReg>(mid, info.from_ir_reg(prev.first.id)));
          pending_moves.push_back({b, info.from_ir_reg(cur->d1.id), mid});
        }
  for (PendingMove &i : pending_moves)
    i.block->insert_before_jump(make_unique<MoveReg>(i.to, i.from));
  reg_n = info.reg_n;
}

void Func::erase_def_use(const OccurPoint &p, Inst *inst) {
  for (Reg r : inst->def_reg()) reg_def[r.id].erase(p);
  for (Reg r : inst->use_reg()) reg_use[r.id].erase(p);
}

void Func::add_def_use(const OccurPoint &p, Inst *inst) {
  for (Reg r : inst->def_reg()) reg_def[r.id].insert(p);
  for (Reg r : inst->use_reg()) reg_use[r.id].insert(p);
}

void Func::build_def_use() {
  reg_def.clear();
  reg_use.clear();
  reg_def.resize(reg_n);
  reg_use.resize(reg_n);
  OccurPoint p;
  for (auto &block : blocks) {
    p.b = block.get();
    p.pos = 0;
    for (p.it = block->insts.begin(); p.it != block->insts.end();
         ++p.it, ++p.pos) {
      add_def_use(p, p.it->get());
    }
  }
}

void Func::calc_live() {
  deque<pair<Block *, Reg>> update;
  for (auto &block : blocks) {
    block->live_use.clear();
    block->def.clear();
    for (auto it = block->insts.rbegin(); it != block->insts.rend(); ++it) {
      for (Reg r : (*it)->def_reg()) {
        block->live_use.erase(r);
        block->def.insert(r);
      }
      for (Reg r : (*it)->use_reg()) {
        block->def.erase(r);
        block->live_use.insert(r);
      }
    }
    for (Reg r : block->live_use) update.emplace_back(block.get(), r);
    block->live_in = block->live_use;
    block->live_out.clear();
  }
  while (!update.empty()) {
    pair<Block *, Reg> cur = update.front();
    update.pop_front();
    for (Block *prev : cur.first->in_edge)
      if (prev->live_out.find(cur.second) == prev->live_out.end()) {
        prev->live_out.insert(cur.second);
        if (prev->def.find(cur.second) == prev->def.end() &&
            prev->live_in.find(cur.second) == prev->live_in.end()) {
          prev->live_in.insert(cur.second);
          update.emplace_back(prev, cur.second);
        }
      }
  }
}

vector<int> Func::get_in_deg() {
  size_t n = blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i) pos[blocks[i].get()] = i;
  vector<int> ret;
  ret.resize(n, 0);
  ret[0] = 1;
  for (size_t i = 0; i < n; ++i) {
    auto &block = blocks[i];
    bool go_next = true;
    for (auto &inst : block->insts)
      if (Branch *b = inst->as<Branch>()) {
        ++ret[pos[b->target]];
        if (b->cond == InstCond::Always) go_next = false;
      } else if (Return *r = inst->as<Return>()) {
        go_next = false;
      }
    if (go_next) {
      assert(i + 1 < n);
      ++ret[i + 1];
    }
  }
  return ret;
}

vector<int> Func::get_branch_in_deg() {
  size_t n = blocks.size();
  map<Block *, size_t> pos;
  for (size_t i = 0; i < n; ++i) pos[blocks[i].get()] = i;
  vector<int> ret;
  ret.resize(n, 0);
  for (size_t i = 0; i < n; ++i) {
    auto &block = blocks[i];
    for (auto &inst : block->insts)
      if (Branch *b = inst->as<Branch>()) {
        ++ret[pos[b->target]];
      }
  }
  return ret;
}

vector<int> Func::reg_allocate(RegAllocStat *stat) {
  info << "register allocation for function: " << name << '\n';
  info << "reg_n = " << reg_n << '\n';
  stat->spill_cnt = 0;
  if (reg_n <= 2000) {
    info << "using ColoringAllocator\n";
    while (true) {
      ColoringAllocator allocator(this);
      vector<int> ret = allocator.run(stat);
      if (stat->succeed) return ret;
    }
  } else {
    info << "using SimpleColoringAllocator\n";
    while (true) {
      SimpleColoringAllocator allocator(this);
      vector<int> ret = allocator.run(stat);
      if (stat->succeed) return ret;
    }
  }
}

bool Func::check_store_stack() {
  bool ret = true;
  for (auto &block : blocks) {
    int32_t sp_offset = 0;
    for (auto i = block->insts.begin(); i != block->insts.end(); ++i) {
      (*i)->maintain_sp(sp_offset);
      InstCond cond = (*i)->cond;
      if (auto store_stk = (*i)->as<StoreStack>()) {
        int32_t total_offset =
            store_stk->target->position + store_stk->offset - sp_offset;
        if (!load_store_offset_range(total_offset)) {
          Reg imm{reg_n++};
          block->insts.insert(
              i, set_cond(make_unique<LoadStackOffset>(imm, store_stk->offset,
                                                       store_stk->target),
                          cond));
          *i = set_cond(make_unique<ComplexStore>(store_stk->src, Reg{sp}, imm),
                        cond);
          ret = false;
        }
      }
    }
  }
  return ret;
}

void Func::replace_with_reg_alloc(const vector<int> &reg_alloc) {
  for (auto &block : blocks)
    for (auto &inst : block->insts)
      for (Reg *i : inst->regs())
        if (i->is_pseudo()) i->id = reg_alloc[i->id];
}

void Func::replace_complex_inst() {
  for (auto &block : blocks) {
    int32_t sp_offset = 0;
    for (auto i = block->insts.begin(); i != block->insts.end(); ++i) {
      (*i)->maintain_sp(sp_offset);
      InstCond cond = (*i)->cond;
      if (auto load_stk = (*i)->as<LoadStack>()) {
        int32_t total_offset =
            load_stk->src->position + load_stk->offset - sp_offset;
        if (!load_store_offset_range(total_offset)) {
          Reg dst = load_stk->dst;
          insert(block->insts, i, set_cond(load_imm(dst, total_offset), cond));
          *i = set_cond(make_unique<ComplexLoad>(dst, Reg{sp}, dst), cond);
        }
      } else if (auto load_stk_addr = (*i)->as<LoadStackAddr>()) {
        int32_t total_offset =
            load_stk_addr->src->position + load_stk_addr->offset - sp_offset;
        Reg dst = load_stk_addr->dst;
        replace(block->insts, i,
                set_cond(reg_imm_sum(dst, Reg{sp}, total_offset), cond));
      } else if (auto load_stk_offset = (*i)->as<LoadStackOffset>()) {
        int32_t total_offset = load_stk_offset->src->position +
                               load_stk_offset->offset - sp_offset;
        Reg dst = load_stk_offset->dst;
        replace(block->insts, i, set_cond(load_imm(dst, total_offset), cond));
      }
    }
  }
}

void Func::gen_asm(ostream &out) {
  RegAllocStat stat;
  vector<int> reg_alloc;
  AsmContext ctx;
  std::function<void(ostream & out)> prologue;
  while (true) {
    reg_alloc = reg_allocate(&stat);
    int32_t stack_size = 0;
    for (auto i = stack_objects.rbegin(); i != stack_objects.rend(); ++i) {
      (*i)->position = stack_size;
      stack_size += (*i)->size;
    }
    vector<Reg> save_regs;
    bool used[RegCount] = {};
    for (int i : reg_alloc)
      if (i >= 0) used[i] = true;
    for (int i = 0; i < RegCount; ++i)
      if (REGISTER_USAGE[i] == callee_save && used[i])
        save_regs.emplace_back(i);
    prologue = [save_regs, stack_size](ostream &out) {
      if (save_regs.size()) {
        out << "push {";
        for (size_t i = 0; i < save_regs.size(); ++i) {
          if (i > 0) out << ',';
          out << save_regs[i];
        }
        out << "}\n";
      }
      if (stack_size != 0) sp_move_asm(-stack_size, out);
    };
    ctx.epilogue = [save_regs, stack_size](ostream &out) -> bool {
      if (stack_size != 0) sp_move_asm(stack_size, out);
      bool pop_lr = false;
      if (save_regs.size()) {
        out << "pop {";
        for (size_t i = 0; i < save_regs.size(); ++i) {
          if (i > 0) out << ',';
          if (save_regs[i].id == lr) {
            pop_lr = true;
            out << "pc";
          } else
            out << save_regs[i];
        }
        out << "}\n";
      }
      return pop_lr;
    };
    int cur_pos = stack_size + save_regs.size() * INT_SIZE;
    for (auto &i : caller_stack_object) {
      i->position = cur_pos;
      cur_pos += i->size;
    }
    if (check_store_stack()) break;
  }
  info << "Register allocation:\n"
       << "spill: " << stat.spill_cnt << '\n'
       << "move instructions eliminated: " << stat.move_eliminated << '\n'
       << "callee-save registers used: " << stat.callee_save_used << '\n';
  replace_with_reg_alloc(reg_alloc);
  replace_complex_inst();
  optimize_after_reg_alloc(this);
  out << '\n' << name << ":\n";
  prologue(out);
  for (auto &block : blocks) block->gen_asm(out, &ctx);
}

void Func::print(ostream &out) {
  out << '\n' << name << ":\n[prologue]\n";
  for (auto &block : blocks) block->print(out);
}

Program::Program(IR::CompileUnit *ir) : block_n(0) {
  for (size_t i = 0; i < ir->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir->scope.objects[i].get();
    if (cur->size == 0) continue;
    unique_ptr<GlobalObject> res = make_unique<GlobalObject>();
    res->name = mangle_global_var_name(cur->name);
    res->size = cur->size;
    res->init = cur->initial_value;
    res->is_int = cur->is_int;
    res->is_const = cur->is_const;
    global_objects.push_back(std::move(res));
  }
  for (auto &i : ir->funcs) {
    unique_ptr<Func> res = make_unique<Func>(this, i.first, i.second.get());
    funcs.push_back(std::move(res));
  }
}

void Program::gen_global_var_asm(ostream &out) {
  bool exist_bss = false, exist_data = false;
  for (auto &obj : global_objects) {
    if (obj->init)
      exist_data = true;
    else
      exist_bss = true;
  }
  if (exist_data) {
    out << ".section .data\n";
    for (auto &obj : global_objects)
      if (obj->init) {
        if (obj->is_int) {
          int *init = reinterpret_cast<int *>(obj->init);
          out << ".align\n";
          out << obj->name << ":\n";
          out << "    .4byte ";
          for (int i = 0; i < obj->size / 4; ++i) {
            if (i > 0) out << ',';
            out << init[i];
          }
          out << '\n';
        } else {
          char *init = reinterpret_cast<char *>(obj->init);
          out << obj->name << ":\n";
          out << "    .asciz " << init << '\n';
        }
      }
  }
  if (exist_bss) {
    out << ".section .bss\n";
    for (auto &obj : global_objects)
      if (!obj->init) {
        assert(obj->is_int);
        out << ".align\n";
        out << obj->name << ":\n";
        out << "    .space " << obj->size << '\n';
      }
  }
}

std::string sfork = R"(
.fork:
	push {r4, r5, r6, r7}
	sub r5, r0, #1
	cmp r5, #0
	movle r0, #0
	ble .fork_0
	mov r7, #120
	mov r1, sp
	mov r2, #0
	mov r3, #0
	mov r4, #0
	mov r6, #0
.fork_1:
	mov r0, #273
	swi #0
	cmp r0, #0
	movne r0, r6
	bne .fork_0
	add r6, r6, #1
	cmp r6, r5
	bne .fork_1
	mov r0, r5
.fork_0:
	pop {r4, r5, r6, r7}
	bx lr
)";

std::string sjoin = R"(
.join:
	sub sp, sp, #1024
	sub sp, sp, r0, LSL #10
	push {r4, lr}
	sub r1, r1, #1
	cmp r1, r0
	mov r4, r0
	beq .join_0
	mov r0, #0
	bl wait
.join_0:
	cmp r4, #0
	bne .join_1
	pop {r4, lr}
	add sp, sp, #1024
	bx lr
.join_1:
	mov r0, #0
	bl _exit
)";

void Program::gen_asm(ostream &out) {
  out << ".arch armv7ve\n.arm\n";
  gen_global_var_asm(out);
  out << ".global main\n";
  out << ".section .text\n";
  out << sfork << sjoin;
  for (auto &func : funcs) func->gen_asm(out);
}

}  // namespace ARMv7