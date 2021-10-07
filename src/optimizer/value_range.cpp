#include "optimizer/pass.hpp"

struct SideEffect {
  ;
};

struct ValueRange {
  MemObject *ptr_base;
  int l, r;
};

void value_range_analysis(NormalFunc *f) {}
