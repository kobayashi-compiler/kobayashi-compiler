#include "backend/armv7/coloring_alloc.hpp"

#include <memory>
#include <queue>

#include "backend/armv7/program.hpp"

using std::make_unique;
using std::map;
using std::queue;
using std::set;
using std::vector;

namespace ARMv7 {

ColoringAllocator::ColoringAllocator(Func *_func) : func(_func) {}

void ColoringAllocator::for_each_node(std::function<void(int)> f) {
  for (int i = 0; i < RegCount; ++i)
    if (occur[i]) f(i);
  for (int i : remain_pesudo_nodes) f(i);
}

void ColoringAllocator::build() {
  occur.resize(func->reg_n, 0);
  interfere_edge.resize(func->reg_n);
  move_edge.resize(func->reg_n);
  parent.resize(func->reg_n, -1);
  load_weight.resize(func->reg_n, 0);
  store_weight.resize(func->reg_n, 0);
  func->calc_live();
  vector<int> temp, new_nodes;
  for (auto &block : func->blocks) {
    double cur_block_prob = block->prob;
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
      if (MoveReg *mov = (*i)->as<MoveReg>())
        if (mov->src != mov->dst)
          if ((mov->src.is_pseudo() || allocable(mov->src.id)) &&
              (mov->dst.is_pseudo() || allocable(mov->dst.id))) {
            ++move_edge[mov->src.id][mov->dst.id];
            ++move_edge[mov->dst.id][mov->src.id];
          }
      new_nodes.clear();
      for (Reg r : (*i)->def_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          if (r.is_pseudo()) store_weight[r.id] += cur_block_prob;
          occur[r.id] = 1;
          live.erase(r);
        }
      for (Reg r : (*i)->use_reg())
        if (r.is_pseudo() || allocable(r.id)) {
          if (r.is_pseudo()) load_weight[r.id] += cur_block_prob;
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
  for (int i = 0; i < func->reg_n; ++i)
    if (occur[i]) {
      parent[i] = i;
      if (i >= RegCount) remain_pesudo_nodes.insert(i);
    }
}

void ColoringAllocator::simplify() {
  for_each_node([this](int x) {
    for (int y : this->interfere_edge[x]) {
      this->move_edge[x].erase(y);
    }
    if (x < RegCount)
      for (int y = 0; y < RegCount; ++y)
        if (this->occur[y] && y != x) {
          this->move_edge[x].erase(y);
        }
  });
  queue<int> simplify_q;
  for (int i : remain_pesudo_nodes) {
    if (move_edge[i].size() == 0 &&
        interfere_edge[i].size() < ALLOCABLE_REGISTER_COUNT)
      simplify_q.push(i);
  }
  while (simplify_q.size()) {
    int cur = simplify_q.front();
    simplify_q.pop();
    vector<int> neighbors;
    neighbors.reserve(interfere_edge[cur].size());
    for (int i : interfere_edge[cur]) {
      neighbors.push_back(i);
      interfere_edge[i].erase(cur);
      if (interfere_edge[i].size() == ALLOCABLE_REGISTER_COUNT - 1 &&
          i >= RegCount && move_edge[i].size() == 0)
        simplify_q.push(i);
    }
    simplify_history.emplace_back(cur, neighbors);
    interfere_edge[cur].clear();
    remain_pesudo_nodes.erase(cur);
  }
}

int ColoringAllocator::coalesce() {
  vector<CoalescePair> pairs;
  for_each_node([&](int x) {
    for (auto &i : this->move_edge[x])
      if (i.first < x) {
        pairs.emplace_back(x, i.first, i.second);
      }
  });
  std::sort(
      pairs.begin(), pairs.end(),
      [](const CoalescePair &l, const CoalescePair &r) { return l.w > r.w; });
  int ret = 0;
  for (auto &cur : pairs) {
    int u = get_root(cur.u), v = get_root(cur.v);
    if (u == v) {
      ret += cur.w;
      continue;
    }
    if (u < RegCount && v < RegCount) continue;
    if (interfere_edge[u].find(v) == interfere_edge[u].end() &&
        conservative_check(u, v)) {
      merge(u, v);
      ret += cur.w;
    }
  }
  return ret;
}

bool ColoringAllocator::freeze() {
  int selected = -1;
  for (int i : remain_pesudo_nodes)
    if (interfere_edge[i].size() < ALLOCABLE_REGISTER_COUNT) {
      if (selected == -1 ||
          interfere_edge[i].size() > interfere_edge[selected].size())
        selected = i;
    }
  if (selected == -1) return false;
  for (auto &i : move_edge[selected]) {
    freezed_move_edge.emplace_back(selected, i.first, i.second);
    move_edge[i.first].erase(selected);
  }
  move_edge[selected].clear();
  return true;
}

int ColoringAllocator::spill() {
  int spill_node = -1;
  double spill_heuristic = 0;
  const double eps = 1e-7;
  for (int i : remain_pesudo_nodes)
    if (func->spilling_reg.find(Reg{i}) == func->spilling_reg.end()) {
      double cur_spill_heuristic;
      if (func->constant_reg.find(Reg{i}) != func->constant_reg.end()) {
        cur_spill_heuristic = static_cast<double>(interfere_edge[i].size()) /
                              (load_weight[i] * 1 + eps);
      } else if (func->symbol_reg.find(Reg{i}) != func->symbol_reg.end()) {
        cur_spill_heuristic = static_cast<double>(interfere_edge[i].size()) /
                              (load_weight[i] * 2 + eps);
      } else {
        cur_spill_heuristic = static_cast<double>(interfere_edge[i].size()) /
                              (load_weight[i] * 5 + store_weight[i] * 5 + eps);
      }
      if (spill_node == -1 || cur_spill_heuristic > spill_heuristic) {
        spill_node = i;
        spill_heuristic = cur_spill_heuristic;
      }
    }
  assert(spill_node != -1);
  remain_pesudo_nodes.erase(spill_node);
  for (int i : interfere_edge[spill_node]) interfere_edge[i].erase(spill_node);
  for (auto &i : move_edge[spill_node]) move_edge[i.first].erase(spill_node);
  interfere_edge[spill_node].clear();
  move_edge[spill_node].clear();
  return spill_node;
}

int ColoringAllocator::get_root(int x) {
  return parent[x] == x ? x : parent[x] = get_root(parent[x]);
}

void ColoringAllocator::add_spill_code(const vector<int> &spill_nodes) {
  vector<StackObject *> spill_obj;
  set<int> constant_spilled;
  for (size_t i = 0; i < spill_nodes.size(); ++i)
    if (func->constant_reg.find(Reg{spill_nodes[i]}) !=
            func->constant_reg.end() ||
        func->symbol_reg.find(Reg{spill_nodes[i]}) != func->symbol_reg.end()) {
      constant_spilled.insert(spill_nodes[i]);
      spill_obj.push_back(nullptr);
    } else {
      StackObject *t = new StackObject();
      t->size = INT_SIZE;
      t->position = -1;
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
            insert(block->insts, i, load_imm(tmp, func->constant_reg[Reg{id}]));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        } else if (func->symbol_reg.find(Reg{id}) != func->symbol_reg.end()) {
          assert(!cur_def);
          if (cur_use) {
            Reg tmp{func->reg_n++};
            func->spilling_reg.insert(tmp);
            func->symbol_reg[tmp] = func->symbol_reg[Reg{id}];
            insert(block->insts, i,
                   load_symbol_addr(tmp, func->symbol_reg[Reg{id}]));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        } else {
          if (cur_def || cur_use) {
            InstCond cond = (*i)->cond;
            StackObject *cur_obj = spill_obj[j];
            Reg tmp{func->reg_n++};
            func->spilling_reg.insert(tmp);
            if (cur_use)
              block->insts.insert(i, make_unique<LoadStack>(tmp, 0, cur_obj));
            if (cur_def)
              block->insts.insert(std::next(i),
                                  make_unique<StoreStack>(tmp, 0, cur_obj));
            (*i)->replace_reg(Reg{id}, tmp);
          }
        }
      }
      ++i;
    }
}

bool ColoringAllocator::conservative_check(int u, int v) {
  int significant_neighbors = 0;
  for (int i : interfere_edge[u])
    if (i != v) {
      int deg = interfere_edge[i].size();
      if (interfere_edge[v].find(i) != interfere_edge[v].end()) {
        if (deg > ALLOCABLE_REGISTER_COUNT) ++significant_neighbors;
      } else if (deg >= ALLOCABLE_REGISTER_COUNT)
        ++significant_neighbors;
    }
  for (int i : interfere_edge[v])
    if (i != u && interfere_edge[u].find(i) == interfere_edge[u].end()) {
      if (interfere_edge[i].size() >= ALLOCABLE_REGISTER_COUNT)
        ++significant_neighbors;
    }
  return significant_neighbors < ALLOCABLE_REGISTER_COUNT;
}

void ColoringAllocator::merge(int u, int v) {
  assert(u >= RegCount || v >= RegCount);
  if (u < RegCount) std::swap(u, v);
  remain_pesudo_nodes.erase(u);
  parent[u] = v;
  for (int i : interfere_edge[u]) {
    interfere_edge[i].erase(u);
    interfere_edge[i].insert(v);
    interfere_edge[v].insert(i);
  }
  interfere_edge[u].clear();
  for (auto &i : move_edge[u]) {
    move_edge[i.first].erase(u);
    if (i.first != v) {
      move_edge[i.first][v] += i.second;
      move_edge[v][i.first] += i.second;
    }
  }
  move_edge[u].clear();
}

vector<int> ColoringAllocator::run(RegAllocStat *stat) {
  build();
  stat->succeed = true;
  stat->move_eliminated = 0;
  vector<int> spill_list;
  while (remain_pesudo_nodes.size()) {
    simplify();
    if (remain_pesudo_nodes.size() == 0) break;
    if (auto tmp = coalesce()) {
      stat->move_eliminated += tmp;
      continue;
    }
    if (freeze()) continue;
    stat->succeed = false;
    ++stat->spill_cnt;
    spill_list.push_back(spill());
  }
  if (!stat->succeed) {
    add_spill_code(spill_list);
    return {};
  }
  vector<int> ans;
  bool used[RegCount] = {};
  ans.resize(func->reg_n);
  std::fill(ans.begin(), ans.end(), -1);
  for (int i = 0; i < RegCount; ++i)
    if (occur[i]) {
      ans[i] = i;
      used[i] = true;
    }
  vector<map<int, int>> freezed;
  freezed.resize(func->reg_n);
  for (auto &i : freezed_move_edge) {
    int u = get_root(i.u), v = get_root(i.v);
    if (u != v) {
      freezed[u][v] += i.w;
      freezed[v][u] += i.w;
    }
  }
  for (size_t i = simplify_history.size() - 1; i < simplify_history.size();
       --i) {
    assert(ans[simplify_history[i].first] == -1);
    assert(parent[simplify_history[i].first] == simplify_history[i].first);
    bool flag[RegCount] = {};
    for (int neighbor : simplify_history[i].second)
      flag[ans[get_root(neighbor)]] = true;
    int score[RegCount] = {};
    for (int j = 0; j < RegCount; ++j)
      if (REGISTER_USAGE[j] == caller_save ||
          (REGISTER_USAGE[j] == callee_save && used[j])) {
        score[j] = 1;
      }
    for (auto &j : freezed[simplify_history[i].first]) {
      int v = get_root(j.first);
      if (ans[v] != -1) score[ans[v]] += j.second;
    }
    int cur_ans = -1;
    for (int j = 0; j < RegCount; ++j)
      if (allocable(j) && !flag[j])
        if (cur_ans == -1 || score[j] > score[cur_ans]) cur_ans = j;
    if (score[cur_ans] > 0) stat->move_eliminated += score[cur_ans] - 1;
    ans[simplify_history[i].first] = cur_ans;
    used[cur_ans] = true;
  }
  for (int i = RegCount; i < func->reg_n; ++i)
    if (occur[i] && parent[i] != i) ans[i] = ans[get_root(i)];
  stat->callee_save_used = 0;
  for (int i = 0; i < RegCount; ++i)
    if (used[i] && REGISTER_USAGE[i] == callee_save) ++stat->callee_save_used;
  return ans;
}

}  // namespace ARMv7