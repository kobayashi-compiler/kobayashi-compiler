#include "optimizer/pass.hpp"

void print_cfg(DomTree &S, NormalFunc *f) {
  dbg << "CFG:\n";
  dbg << "```mermaid\n";
  dbg << "graph TB\n";
  f->for_each([&](BB *w) { dbg << w->name << "(" << w->name << ")\n"; });
  f->for_each([&](BB *w) {
    for (BB *u : S.at(w).out) dbg << w->name << "-->" << u->name << "\n";
  });
  dbg << "```\n";
}

void print_dom_tree(DomTree &S, NormalFunc *f) {
  dbg << "dom tree\n";
  dbg << "```mermaid\n";
  dbg << "graph TB\n";
  f->for_each([&](BB *w) { dbg << w->name << "(" << w->name << ")\n"; });
  f->for_each([&](BB *w) {
    for (BB *u : S.at(w).dom_ch) dbg << w->name << "-->" << u->name << "\n";
  });
  dbg << "```\n";
}

void print_loop_tree(DomTree &S, NormalFunc *f) {
  dbg << "loop tree\n";
  dbg << "```mermaid\n";
  dbg << "graph TB\n";
  f->for_each([&](BB *w) {
    dbg << w->name;
    if (S[w].loop_rt)
      dbg << "[" << w->name << "]\n";
    else
      dbg << "(" << w->name << ")\n";
  });
  f->for_each([&](BB *w) {
    for (BB *u : S.at(w).loop_ch) dbg << w->name << "-->" << u->name << "\n";
  });
  dbg << "```\n";
}

DomTree build_dom_tree(NormalFunc *f) {
  SetDebugState _(dbg, 0);
  dbg << f->name << "\n\n";
  // build BB graph
  DomTree S;
  auto addedge = [&](BB *y, BB *x) {
    S[x].out.push_back(y);
    S[y].in.push_back(x);
  };
  f->for_each([&](BB *w) {
    S[w].self = w;
    Instr *x = w->back();
    Case(JumpInstr, y, x) { addedge(y->target, w); }
    else Case(BranchInstr, y, x) {
      addedge(y->target1, w);
      addedge(y->target0, w);
    }
    else Case(ReturnInstr, y, x) {
      ;
    }
    else assert(0);
  });

  // build domination tree
  int tag = 1;
  std::vector<BB *> idfn;
  std::function<void(BB *, BB *)> dfs;
  dfs = [&](BB *w, BB *fa) {
    auto &s = S[w];
    if (s.tag == tag) return;
    s.tag = tag;
    if (tag == 1) idfn.push_back(w);
    for (BB *u : s.out) dfs(u, w);
  };
  dfs(f->entry, NULL);
  for (BB *w : idfn) {
    ++tag;
    S[w].tag = tag;
    dfs(f->entry, NULL);
    for (BB *u : idfn) {
      if (S[u].tag != tag) S[u].dom_fa = w;
    }
  }
  for (BB *w : idfn) {
    BB *f = S[w].dom_fa;
    if (f) S[f].dom_ch.push_back(w);
  }
  ++tag;
  int tk = 0;
  std::vector<BB *> dom_idfn;
  dfs = [&](BB *w, BB *fa) {
    auto &s = S[w];
    if (s.tag == tag) return;
    s.tag = tag;
    s.dfn = ++tk;
    dom_idfn.push_back(w);
    for (BB *u : s.dom_ch) dfs(u, w);
    s.dfn2 = tk;
  };
  dfs(f->entry, NULL);  // calc dfs order
  ++tag;

  // calc DF
  f->for_each([&](BB *b) {
    auto &sb = S[b];
    for (BB *a : sb.in) {
      for (BB *x = a; x;) {
        auto &sx = S[x];
        if (sx.sdom(sb)) break;
        sx.DF.insert(b);
        x = sx.dom_fa;
      }
    }
  });

  dbg << "DF\n";
  dbg << "```mermaid\n";
  dbg << "graph TB\n";
  f->for_each([&](BB *w) {
    dbg << w->name;
    dbg << "(" << w->name << ")\n";
  });
  f->for_each([&](BB *w) {
    for (BB *u : S[w].DF) dbg << w->name << "-->" << u->name << "\n";
  });
  dbg << "```\n";

  // find loops
  dfs = [&](BB *w, BB *z) {
    if (w == z) return;
    auto &s = S[w];
    if (s.tag == tag) {
      dfs(s.loop_fa, z);
    } else {
      s.tag = tag;
      S[s.loop_fa = z].loop_ch.push_back(w);
      for (BB *u : s.in) dfs(u, z);
    }
  };
  ++tag;
  for (size_t i = idfn.size() - 1;; --i) {
    BB *w = idfn[i];
    auto &sw = S[w];
    assert(!sw.loop_fa);
    for (BB *u : S[w].in) {
      auto &su = S[u];
      if (sw.dom(su)) {
        // back edge
        dfs(u, w);
        sw.loop_rt = 1;
      }
    }
    if (sw.loop_rt) {
      for (BB *u : sw.in) {
        auto &su = S[u];
        if (!sw.dom(su)) {
          if (!su.dom(sw)) {
            SetDebugState _(dbg, 1);
            print_cfg(S, f);
          }
          assert(su.dom(sw));
          assert(!sw.loop_pre);
          sw.loop_pre = u;
        } else {
          assert(sw.loop_last == NULL);
          sw.loop_last = u;
        }
      }
      assert(sw.loop_pre);
      assert(sw.loop_last);

      loop_tree_for_each(S, w, [&](BB *u) {
        for (BB *u1 : S[u].out) {
          if (!in_loop(S, u1, w)) sw.loop_exit.insert(u1);
        }
      });
      dbg << "loop: " << w->name << "\n";
      dbg << "```\n";
      dbg << *w;
      for (BB *u : sw.loop_exit) dbg << "exit: " << u->name << "\n";
      dbg << "```\n";
    }
    if (!i) break;
  }

  for (BB *w : idfn) {
    auto &sw = S[w];
    if (!sw.loop_rt) continue;
    sw.loop_simple = 0;
    if (sw.loop_exit.size() != 1) continue;
    assert(sw.in.size() == 2);
    BB *exit = *sw.loop_exit.begin();
    int exit_cnt = 0;
    for (BB *u : S[exit].in) {
      if (in_loop(S, u, w)) ++exit_cnt, sw.loop_pre_exit = u;
    }
    if (exit_cnt != 1) {
      sw.loop_pre_exit = NULL;
      continue;
    }
    sw.loop_simple = 1;
  }
  for (BB *w : idfn) {
    for (BB *u = S[w].get_loop_rt(); u; u = S[u].loop_fa) ++S[w].loop_depth;
  }
  return S;
}

bool in_loop(DomTree &S, BB *w, BB *loop) {
  if (loop == NULL) return true;
  auto &sl = S.at(loop);
  assert(sl.loop_rt);
  while (w && w != loop) w = S.at(w).loop_fa;
  return w == loop;
}

void loop_tree_for_each(DomTree &S, BB *w, std::function<void(BB *)> F) {
  assert(S[w].loop_rt);
  std::queue<BB *> q;
  q.push(w);
  while (!q.empty()) {
    w = q.front();
    q.pop();
    F(w);
    for (BB *u : S[w].loop_ch) q.push(u);
  }
}

void dom_tree_dfs(DomTree &S, std::function<void(BB *)> F) {
  std::unordered_map<int, BB *> idfn;
  for (auto &kv : S) {
    idfn[kv.second.dfn] = kv.first;
  }
  for (int i = 1; i <= (int)idfn.size(); ++i) {
    F(idfn.at(i));
  }
}

void dom_tree_rdfs(DomTree &S, std::function<void(BB *)> F) {
  std::unordered_map<int, BB *> idfn;
  for (auto &kv : S) {
    idfn[kv.second.dfn] = kv.first;
  }
  for (int i = (int)idfn.size(); i; --i) {
    F(idfn.at(i));
  }
}
