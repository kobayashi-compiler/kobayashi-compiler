#include "backend/rv32/program.hpp"

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

#include "optimizer/pass.hpp"
#include "common/common.hpp"
#include "backend/rv32/archinfo.hpp"
#include "backend/rv32/inst.hpp"
#include "backend/rv32/coloring_alloc.hpp"
#include "backend/rv32/simple_coloring_alloc.hpp"
#include "backend/rv32/backend_passes.hpp"

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

namespace RV32 {

MappingInfo::MappingInfo() : reg_n(RegCount) {}

Reg MappingInfo::new_reg() { return Reg{reg_n++}; }

Reg MappingInfo::from_ir_reg(IR::Reg ir_reg) {
  auto it = reg_mapping.find(ir_reg.id);
  if (it != reg_mapping.end()) return it->second;
  Reg ret = new_reg();
  reg_mapping[ir_reg.id] = ret;
  return ret;
}

Block::Block(string _name) : prob(1), name(_name), label_used(false) {}

void Block::construct(IR::BB *ir_bb, Func *func, MappingInfo *info, Block *next_block, std::map<Reg, CmpInfo> &cmp_info) {
  for (auto &i : ir_bb->instrs) {
    IR::Instr *cur = i.get();
    if (auto loadaddr = dynamic_cast<IR::LoadAddr *>(cur)) {
      Reg dst = info->from_ir_reg(loadaddr->d1);
      if (loadaddr->offset->global) {
        push_back(make_unique<LoadLabelAddr>(dst, mangle_global_var_name(loadaddr->offset->name)));
        func->symbol_reg[dst] = mangle_global_var_name(loadaddr->offset->name);
      } else {
        push_back(make_unique<LoadStackAddr>(info->obj_mapping[loadaddr->offset], dst, 0));
        func->stack_addr_reg[dst] = std::pair<StackObject *, int32_t>{info->obj_mapping[loadaddr->offset], 0};
      }
    } else if (auto loadconst = dynamic_cast<IR::LoadConst *>(cur)) {
      if (loadconst->value == 0) {
        info->reg_mapping[loadconst->d1.id] = Reg{zero};
      } else {
        Reg dst = info->from_ir_reg(loadconst->d1);
        push_back(make_unique<LoadImm>(dst, loadconst->value));
        func->constant_reg[dst] = loadconst->value;
      }
    } else if (auto loadarg = dynamic_cast<IR::LoadArg *>(cur)) {
      push_back(make_unique<Move>(info->from_ir_reg(loadarg->d1), func->arg_reg[loadarg->id]));
    } else if (auto unary = dynamic_cast<IR::UnaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(unary->d1),
          src = info->from_ir_reg(unary->s1);
      switch (unary->op.type) {
        case IR::UnaryOp::LNOT:
          push_back(make_unique<RegImmInst>(RegImmInst::Sltiu, dst, src, 1));
          break;
        case IR::UnaryOp::NEG:
          push_back(make_unique<RegRegInst>(RegRegInst::Sub, dst, Reg{zero}, src));
          break;
        case IR::UnaryOp::ID:
          push_back(make_unique<Move>(dst, src));
          break;
      }
    } else if (auto binary = dynamic_cast<IR::BinaryOpInstr *>(cur)) {
      Reg dst = info->from_ir_reg(binary->d1),
          s1 = info->from_ir_reg(binary->s1),
          s2 = info->from_ir_reg(binary->s2);
      if (binary->op.type == IR::BinaryOp::ADD ||
          binary->op.type == IR::BinaryOp::SUB ||
          binary->op.type == IR::BinaryOp::MUL ||
          binary->op.type == IR::BinaryOp::DIV ||
          binary->op.type == IR::BinaryOp::MOD) {
        push_back(make_unique<RegRegInst>(RegRegInst::from_ir_binary_op(binary->op.type), dst, s1, s2));
      } else {
        Compare c;
        switch (binary->op.type) {
          case IR::BinaryOp::LESS:
            c = Lt;
            break;
          case IR::BinaryOp::LEQ:
            c = Le;
            break;
          case IR::BinaryOp::EQ:
            c = Eq;
            break;
          case IR::BinaryOp::NEQ:
            c = Ne;
            break;
          default:
            unreachable();
        }
        push_back(make_unique<VirtualDefPoint>(dst));
        cmp_info[dst] = CmpInfo{ .type = c, .lhs = s1, .rhs = s2 };
      }
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
        push_back(make_unique<Jump>(jump_target));
      out_edge.push_back(jump_target);
      jump_target->in_edge.push_back(this);
    } else if (auto branch = dynamic_cast<IR::BranchInstr *>(cur)) {
      CmpInfo cond;
      Block *true_target = info->block_mapping[branch->target1],
            *false_target = info->block_mapping[branch->target0];
      if (cmp_info.find(info->from_ir_reg(branch->cond)) != cmp_info.end()) {
        cond = cmp_info[info->from_ir_reg(branch->cond)];
      } else {
        cond.type = Ne;
        cond.lhs = info->from_ir_reg(branch->cond);
        cond.rhs = Reg{zero};
      }
      if (false_target == next_block) {
        push_back(make_unique<Branch>(true_target, cond.lhs, cond.rhs, cond.type));
      } else if (true_target == next_block) {
        push_back(make_unique<Branch>(false_target, cond.lhs, cond.rhs, logical_not(cond.type)));
      } else {
        push_back(make_unique<Branch>(true_target, cond.lhs, cond.rhs, cond.type));
        push_back(make_unique<Jump>(false_target));
      }
      out_edge.push_back(false_target);
      out_edge.push_back(true_target);
      false_target->in_edge.push_back(this);
      true_target->in_edge.push_back(this);
    } else if (auto ret = dynamic_cast<IR::ReturnInstr *>(cur)) {
      if (ret->ignore_return_value) {
        push_back(make_unique<Return>(false));
      } else {
        push_back(make_unique<Move>(Reg{ARGUMENT_REGISTERS[0]}, info->from_ir_reg(ret->s1)));
        push_back(make_unique<Return>(true));
      }
    } else if (auto call = dynamic_cast<IR::CallInstr *>(cur)) {
      if (call->args.size() > ARGUMENT_REGISTER_COUNT) {
        int32_t sp_move = (static_cast<int32_t>(call->args.size()) - ARGUMENT_REGISTER_COUNT) * INT_SIZE;
        if (sp_move > IMM12_R || -sp_move < IMM12_L) {
          throw SyntaxError("too many arguments");
        }
        for (size_t i = 0; i < ARGUMENT_REGISTER_COUNT; ++i)
          push_back(make_unique<Move>(Reg{ARGUMENT_REGISTERS[i]}, info->from_ir_reg(call->args[i])));
        push_back(make_unique<MoveSP>(-sp_move));
        int32_t pos = 0;
        for (size_t i = ARGUMENT_REGISTER_COUNT; i < call->args.size(); ++i) {
          push_back(make_unique<Store>(info->from_ir_reg(call->args[i]), Reg{sp}, pos));
          pos += INT_SIZE;
        }
        push_back(make_unique<FuncCall>(call->f->name, static_cast<int>(call->args.size())));
        push_back(make_unique<MoveSP>(sp_move));
      } else {
        for (size_t i = 0; i < call->args.size(); ++i)
          push_back(make_unique<Move>(Reg{ARGUMENT_REGISTERS[i]}, info->from_ir_reg(call->args[i])));
        push_back(make_unique<FuncCall>(call->f->name, static_cast<int>(call->args.size())));
      }
      if (!call->ignore_return_value)
        push_back(make_unique<Move>(info->from_ir_reg(call->d1), Reg{ARGUMENT_REGISTERS[0]}));
    } else if (auto local_var_def = dynamic_cast<IR::LocalVarDef *>(cur)) {
      // do nothing
    } else if (auto array_index = dynamic_cast<IR::ArrayIndex *>(cur)) {
      Reg dst = info->from_ir_reg(array_index->d1),
          s1 = info->from_ir_reg(array_index->s1),
          s2 = info->from_ir_reg(array_index->s2),
          size = info->new_reg(),
          mid = info->new_reg();
      push_back(make_unique<LoadImm>(size, array_index->size));
      push_back(make_unique<RegRegInst>(RegRegInst::Mul, mid, s2, size));
      push_back(make_unique<RegRegInst>(RegRegInst::Add, dst, s1, mid));
      func->constant_reg[size] = array_index->size;
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
    if ((*prev_i)->as<Branch>() || (*prev_i)->as<Jump>()) {
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

Func::Func(Program *prog, std::string _name, IR::NormalFunc *ir_func)
    : name(_name), entry(nullptr), reg_n(0) {
  MappingInfo info;
  for (size_t i = 0; i < ir_func->scope.objects.size(); ++i) {
    IR::MemObject *cur = ir_func->scope.objects[i].get();
    if (cur->size == 0) continue;
    unique_ptr<StackObject> res = make_unique<StackObject>(cur->size);
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
          make_unique<Move>(cur_arg, Reg{ARGUMENT_REGISTERS[i]}));
    } else {
      unique_ptr<StackObject> t = make_unique<StackObject>(INT_SIZE);
      entry->push_back(make_unique<LoadStack>(t.get(), cur_arg, 0));
      caller_stack_object.push_back(std::move(t));
    }
    arg_reg.push_back(cur_arg);
  }
  Block *real_entry = info.block_mapping[ir_func->entry];
  if (blocks[1].get() != real_entry)
    entry->push_back(make_unique<Jump>(real_entry));
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
          cmp_info);  // maintain in_edge, out_edge,
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
              make_unique<Move>(mid, info.from_ir_reg(prev.first.id)));
          pending_moves.push_back({b, info.from_ir_reg(cur->d1.id), mid});
        }
  for (PendingMove &i : pending_moves)
    i.block->insert_before_jump(make_unique<Move>(i.to, i.from));
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
      for (Reg r : (*it)->def_reg()) 
        if (r.is_pseudo() || allocable(r.id)) {
          block->live_use.erase(r);
          block->def.insert(r);
        }
      for (Reg r : (*it)->use_reg())
        if (r.is_pseudo() || allocable(r.id)) {
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
      if (auto store_stk = (*i)->as<StoreStack>()) {
        int32_t total_offset = store_stk->base->position + store_stk->offset - sp_offset;
        if (!is_imm12(total_offset)) {
          Reg addr{reg_n++};
          block->insts.insert(i, make_unique<LoadStackAddr>(store_stk->base, addr, store_stk->offset));
          *i = make_unique<Store>(store_stk->src, addr, 0);
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
      if (auto load_stk = (*i)->as<LoadStack>()) {
        int32_t total_offset = load_stk->base->position + load_stk->offset - sp_offset;
        Reg dst = load_stk->dst;
        if (is_imm12(total_offset)) {
          *i = make_unique<Load>(dst, Reg{sp}, total_offset);
        } else {
          block->insts.insert(i, make_unique<LoadImm>(dst, total_offset));
          block->insts.insert(i, make_unique<RegRegInst>(RegRegInst::Add, dst, Reg{sp}, dst));
          *i = make_unique<Load>(dst, dst, 0);
        }
      } else if (auto store_stk = (*i)->as<StoreStack>()) {
        int32_t total_offset = store_stk->base->position + store_stk->offset - sp_offset;
        assert(is_imm12(total_offset));
        *i = make_unique<Store>(store_stk->src, Reg{sp}, total_offset);
      } else if (auto load_stk_addr = (*i)->as<LoadStackAddr>()) {
        int32_t total_offset = load_stk_addr->base->position + load_stk_addr->offset - sp_offset;
        Reg dst = load_stk_addr->dst;
        if (is_imm12(total_offset)) {
          *i = make_unique<RegImmInst>(RegImmInst::Addi, dst, Reg{sp}, total_offset);
        } else {
          block->insts.insert(i, make_unique<LoadImm>(dst, total_offset));
          *i = make_unique<RegRegInst>(RegRegInst::Add, dst, Reg{sp}, dst);
        }
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
      if (stack_size == 0 && save_regs.size() == 0) return;
      if (-stack_size - static_cast<int32_t>(save_regs.size() * INT_SIZE) >= IMM12_L) {
        out << "addi sp, sp, " << -stack_size - static_cast<int32_t>(save_regs.size() * INT_SIZE) << '\n';
        int32_t cur_pos = stack_size;
        for (Reg r : save_regs) {
          out << "sw " << r << ", " << cur_pos << "(sp)\n";
          cur_pos += 4;
        }
      } else {
        if (save_regs.size()) {
          out << "addi sp, sp, -" << save_regs.size() * INT_SIZE << '\n';
          int32_t cur_pos = 0;
          for (Reg r : save_regs) {
            out << "sw " << r << ", " << cur_pos << "(sp)\n";
            cur_pos += 4;
          }
        }
        if (stack_size > 0) {
          if (-stack_size >= IMM12_L) {
            out << "addi sp, sp, " << -stack_size << '\n';
          } else {
            out << "li t0, " << stack_size << '\n';
            out << "sub sp, sp, t0\n";
          }
        }
      }
    };
    ctx.epilogue = [save_regs, stack_size](ostream &out) {
      if (stack_size == 0 && save_regs.size() == 0) return;
      if (stack_size + static_cast<int32_t>(save_regs.size() * INT_SIZE) < IMM12_R) {
        int32_t cur_pos = stack_size;
        for (Reg r : save_regs) {
          out << "lw " << r << ", " << cur_pos << "(sp)\n";
          cur_pos += 4;
        }
        out << "addi sp, sp, " << stack_size + static_cast<int32_t>(save_regs.size() * INT_SIZE) << '\n';
      } else {
        if (stack_size > 0) {
          if (stack_size < IMM12_R) {
            out << "addi sp, sp, " << stack_size << '\n';
          } else {
            out << "li t0, " << stack_size << '\n';
            out << "add sp, sp, t0\n";
          }
        }
        if (save_regs.size()) {
          int32_t cur_pos = 0;
          for (Reg r : save_regs) {
            out << "lw " << r << ", " << cur_pos << "(sp)\n";
            cur_pos += 4;
          }
          out << "addi sp, sp, " << save_regs.size() * INT_SIZE << '\n';
        }
      }
    };
    int32_t cur_pos = stack_size + save_regs.size() * INT_SIZE;
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

Program::Program(IR::CompileUnit *ir): block_n(0) {
  for (auto &i : ir->funcs)
    funcs.push_back(make_unique<Func>(this, i.first, i.second.get()));
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
    out << ".data\n";
    for (auto &obj : global_objects)
      if (obj->init) {
        if (obj->is_int) {
          int32_t *init = reinterpret_cast<int32_t *>(obj->init);
          out << ".align 2\n";
          out << obj->name << ":\n";
          out << "    .4byte ";
          for (int i = 0; i < obj->size / 4; ++i) {
            if (i > 0) out << ", ";
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
    out << ".bss\n";
    for (auto &obj : global_objects)
      if (!obj->init) {
        if (obj->is_int)
          out << ".align 2\n";
        out << obj->name << ":\n";
        out << "    .space " << obj->size << '\n';
      }
  }
}


void Program::gen_asm(ostream &out) {
  gen_global_var_asm(out);
  out << ".globl main\n";
  out << ".text\n";
  for (auto &func : funcs) func->gen_asm(out);
}

}