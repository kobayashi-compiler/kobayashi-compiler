#include <fstream>

#include "common/common.hpp"

struct SyncDebugStream {
  std::ostream &os;
};

struct DebugStream {
  bool on = true;
  std::ofstream of;
  DebugStream() {}
  void init(std::string path) {
    if (global_config.log_level == Configuration::DEBUG) of.open(path);
  }
  SyncDebugStream sync(std::ostream &os) { return SyncDebugStream{os}; }
};

struct SetDebugState {
  DebugStream &s;
  bool on;
  SetDebugState(DebugStream &s, bool _on) : s(s) {
    on = s.on;
    s.on = _on;
  }
  ~SetDebugState() { s.on = on; }
};

extern DebugStream dbg;

template <class T>
DebugStream &operator<<(DebugStream &x, const T &y) {
  if (global_config.log_level != Configuration::DEBUG) return x;
  if (x.on) {
    // if(&y==NULL)x.of<<"(null)";
    // else x.of<<y;
    x.of << y;
    x.of.flush();
  }
  return x;
}

template <class T>
SyncDebugStream &operator<<(SyncDebugStream &x, const T &y) {
  if (global_config.log_level != Configuration::DEBUG) return x;
  dbg << y;
  x.os << y;
  return x;
}
