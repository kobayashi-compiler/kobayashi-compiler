#pragma once

#include <queue>
#include <set>
#include <utility>
#include <vector>

namespace RV32 {

struct RegAllocStat;
struct Func;

// no coalescing
class SimpleColoringAllocator {
  Func *func;
  std::vector<unsigned char> occur;  // in fact it's boolean array
  std::vector<std::set<int>> interfere_edge;
  std::queue<int> simplify_nodes;
  std::vector<std::pair<int, std::vector<int>>> simplify_history;
  std::set<int> remain_pesudo_nodes;

  void build_graph();  // build occur, interfere_edge
  void spill(const std::vector<int> &spill_nodes);
  void remove(int id);
  void simplify();
  void clear();
  int choose_spill();

 public:
  SimpleColoringAllocator(Func *_func);
  std::vector<int> run(RegAllocStat *stat);
};

}  // namespace RV32