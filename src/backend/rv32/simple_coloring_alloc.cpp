#include "backend/rv32/simple_coloring_alloc.hpp"

#include <cassert>
#include <memory>
#include <queue>
#include <set>
#include <utility>
#include <vector>

#include "backend/rv32/archinfo.hpp"
#include "common/common.hpp"
#include "backend/rv32/inst.hpp"
#include "backend/rv32/program.hpp"

using std::make_unique;
using std::pair;
using std::set;
using std::vector;

namespace RV32 {

SimpleColoringAllocator::SimpleColoringAllocator(Func *_func) : func(_func) {}

void SimpleColoringAllocator::spill(const vector<int> &spill_nodes) {
  vector<StackObject *> spill_obj;
  set<int> constant_spilled;
  for (size_t i = 0; i < spill_nodes.size(); ++i)
    if (func->constant_reg.find(Reg{spill_nodes[i]}) !=
            func->constant_reg.end() ||
        func->symbol_reg.find(Reg{spill_nodes[i]}) != func->symbol_reg.end()) {
      constant_spilled.insert(spill_nodes[i]);
      spill_obj.push_back(nullptr);
    } else {
      StackObject *t = new StackObject(INT_SIZE);
      func->stack_objects.emplace_back(t);
      spill_obj.push_back(t);
    }
  for (auto &block : func->blocks)
    for (auto i = block->insts.begin(); i != block->insts.end();) {
      bool need_continue = false;
      for (Reg r : (*i)->def_reg())
        if (constant_spilled.find(r.id) != constant_spilled.end()) {
          auto nxt = std::next(i);
          block->insts.erase(i);
          i = nxt;
          need_continue = true;
          break;
        }
      if (need_continue) continue;
      for (size_t j = 0; j < spill_nodes.size(); ++j) {
        int id = spill_nodes[j];
        bool cur_def = (*i)->def(Reg{id}), cur_use = (*i)->use(Reg{id});
        if (func->constant_reg.find(Reg{id}) != func->constant_reg.end()) {
          assert(!cur_def);
          if (cur_use) {
            Reg tmp{func->reg_n++};
            func->spilling_reg.insert(tmp);
            func->constant_reg[tmp] = func->constant_reg[Reg{id}];
            block->insts.insert(i, make_unique<LoadImm>(tmp, func->constant_reg[Reg{id}]));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        } else if (func->symbol_reg.find(Reg{id}) != func->symbol_reg.end()) {
          assert(!cur_def);
          if (cur_use) {
            Reg tmp{func->reg_n++};
            func->spilling_reg.insert(tmp);
            func->symbol_reg[tmp] = func->symbol_reg[Reg{id}];
            block->insts.insert(i, make_unique<LoadLabelAddr>(tmp, func->symbol_reg[Reg{id}]));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        } else {
          if (cur_def || cur_use) {
            StackObject *cur_obj = spill_obj[j];
            Reg tmp{func->reg_n++};
            func->spilling_reg.insert(tmp);
            if (cur_use)
              block->insts.insert(i, make_unique<LoadStack>(cur_obj, tmp, 0));
            if (cur_def)
              block->insts.insert(std::next(i),
                                  make_unique<StoreStack>(cur_obj, tmp, 0));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        }
      }
      ++i;
    }
}

void SimpleColoringAllocator::build_graph() {
  occur.resize(func->reg_n);
  interfere_edge.resize(func->reg_n);
  std::fill(occur.begin(), occur.end(), 0);
  std::fill(interfere_edge.begin(), interfere_edge.end(), set<int>{});
  func->calc_live();
  vector<int> temp, new_nodes;
  for (auto &block : func->blocks) {
    set<Reg> live = block->live_out;
    temp.clear();
    for (Reg r : live)
      if (r.is_pseudo() || allocable(r.id)) temp.push_back(r.id);
    if (block->insts.size() > 0)
      for (Reg r : (*block->insts.rbegin())->def_reg())
        if (r.is_pseudo() || allocable(r.id)) temp.push_back(r.id);
    for (size_t idx1 = 0; idx1 < temp.size(); ++idx1)
      for (size_t idx0 = 0; idx0 < idx1; ++idx0)
        if (temp[idx0] != temp[idx1]) {
          interfere_edge[temp[idx0]].insert(temp[idx1]);
          interfere_edge[temp[idx1]].insert(temp[idx0]);
        }
    for (auto i = block->insts.rbegin(); i != block->insts.rend(); ++i) {
      new_nodes.clear();
      for (Reg r : (*i)->def_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          occur[r.id] = 1;
          live.erase(r);
        }
      for (Reg r : (*i)->use_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          occur[r.id] = 1;
          if (live.find(r) == live.end()) {
            for (Reg o : live) {
              interfere_edge[r.id].insert(o.id);
              interfere_edge[o.id].insert(r.id);
            }
            live.insert(r);
          }
          new_nodes.push_back(r.id);
        }
      if (std::next(i) != block->insts.rend())
        for (Reg r : (*std::next(i))->def_reg())
          if (r.is_pseudo() || allocable(r.id)) {
            new_nodes.push_back(r.id);
            if (live.find(r) == live.end())
              for (Reg o : live) {
                interfere_edge[r.id].insert(o.id);
                interfere_edge[o.id].insert(r.id);
              }
          }
      for (size_t idx1 = 0; idx1 < new_nodes.size(); ++idx1)
        for (size_t idx0 = 0; idx0 < idx1; ++idx0)
          if (new_nodes[idx0] != new_nodes[idx1]) {
            interfere_edge[new_nodes[idx0]].insert(new_nodes[idx1]);
            interfere_edge[new_nodes[idx1]].insert(new_nodes[idx0]);
          }
    }
  }
}

void SimpleColoringAllocator::remove(int id) {
  assert(id >= RegCount);
  for (int i : interfere_edge[id]) {
    interfere_edge[i].erase(id);
    if (interfere_edge[i].size() == ALLOCABLE_REGISTER_COUNT - 1 &&
        i >= RegCount)
      simplify_nodes.push(i);
  }
  interfere_edge[id].clear();
  remain_pesudo_nodes.erase(id);
}

void SimpleColoringAllocator::simplify() {
  while (!simplify_nodes.empty()) {
    int cur = simplify_nodes.front();
    simplify_nodes.pop();
    vector<int> neighbors;
    neighbors.reserve(interfere_edge[cur].size());
    for (int i : interfere_edge[cur]) {
      neighbors.push_back(i);
    }
    remove(cur);
    simplify_history.emplace_back(cur, neighbors);
  }
}

void SimpleColoringAllocator::clear() {
  occur.clear();
  interfere_edge.clear();
  simplify_nodes = std::queue<int>{};
  simplify_history.clear();
  remain_pesudo_nodes.clear();
}

int SimpleColoringAllocator::choose_spill() {
  int spill_node = -1;
  for (int i : remain_pesudo_nodes)
    if (func->spilling_reg.find(Reg{i}) == func->spilling_reg.end())
      if (func->constant_reg.find(Reg{i}) != func->constant_reg.end() ||
          func->symbol_reg.find(Reg{i}) != func->symbol_reg.end())
        if (spill_node == -1 ||
            interfere_edge[i].size() > interfere_edge[spill_node].size())
          spill_node = i;
  if (spill_node == -1) {
    for (int i : remain_pesudo_nodes)
      if (func->spilling_reg.find(Reg{i}) == func->spilling_reg.end())
        if (spill_node == -1 ||
            interfere_edge[i].size() > interfere_edge[spill_node].size())
          spill_node = i;
  }
  assert(spill_node != -1);
  return spill_node;
}

vector<int> SimpleColoringAllocator::run(RegAllocStat *stat) {
  build_graph();
  for (int i = RegCount; i < func->reg_n; ++i)
    if (occur[i]) {
      remain_pesudo_nodes.insert(i);
      if (interfere_edge[i].size() < ALLOCABLE_REGISTER_COUNT)
        simplify_nodes.push(i);
    }
  simplify();
  if (remain_pesudo_nodes.size()) {
    vector<int> spill_nodes;
    while (remain_pesudo_nodes.size()) {
      int cur = choose_spill();
      spill_nodes.push_back(cur);
      remove(cur);
      simplify();
    }
    stat->spill_cnt += static_cast<int>(spill_nodes.size());
    spill(spill_nodes);
    stat->succeed = false;
    return {};
  }
  stat->succeed = true;
  stat->move_eliminated = 0;
  stat->callee_save_used = 0;
  vector<int> ans;
  bool used[RegCount] = {};
  ans.resize(func->reg_n);
  std::fill(ans.begin(), ans.end(), -1);
  for (int i = 0; i < RegCount; ++i)
    if (occur[i]) {
      ans[i] = i;
      used[i] = true;
    }
  for (size_t i = simplify_history.size() - 1; i < simplify_history.size();
       --i) {
    assert(ans[simplify_history[i].first] == -1);
    bool flag[RegCount] = {};
    for (int neighbor : simplify_history[i].second) flag[ans[neighbor]] = true;
    for (int j = 0; j < RegCount; ++j)
      if ((REGISTER_USAGE[j] == caller_save ||
           (REGISTER_USAGE[j] == callee_save && used[j])) &&
          !flag[j]) {
        ans[simplify_history[i].first] = j;
        break;
      }
    if (ans[simplify_history[i].first] == -1)
      for (int j = 0; j < RegCount; ++j)
        if (allocable(j) && !flag[j]) {
          ans[simplify_history[i].first] = j;
          break;
        }
    used[ans[simplify_history[i].first]] = true;
  }
  for (int i = 0; i < RegCount; ++i)
    if (used[i] && REGISTER_USAGE[i] == callee_save) ++stat->callee_save_used;
  return ans;
}

}  // namespace ARMv7