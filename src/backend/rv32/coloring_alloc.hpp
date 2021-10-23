#pragma once

#include <functional>
#include <map>
#include <set>
#include <utility>
#include <vector>

namespace RV32 {

struct RegAllocStat;
struct Func;

class ColoringAllocator {
  struct CoalescePair {
    int u, v, w;
    CoalescePair(int _u, int _v, int _w) : u(_u), v(_v), w(_w) {}
  };
  Func *func;
  std::vector<unsigned char> occur;  // in fact it's boolean array
  std::vector<std::set<int>> interfere_edge;
  std::vector<std::map<int, int>> move_edge;
  std::vector<CoalescePair> freezed_move_edge;
  std::vector<std::pair<int, std::vector<int>>> simplify_history;
  std::set<int> remain_pesudo_nodes;
  std::vector<int> parent;
  std::vector<double> load_weight, store_weight;

  void build();
  void simplify();
  int coalesce();
  bool freeze();
  int spill();
  int get_root(int x);
  void add_spill_code(const std::vector<int> &spill_nodes);
  bool conservative_check(int u, int v);
  void merge(int u, int v);
  void for_each_node(std::function<void(int)> f);

 public:
  ColoringAllocator(Func *_func);
  std::vector<int> run(RegAllocStat *stat);
};

}  // namespace RV32