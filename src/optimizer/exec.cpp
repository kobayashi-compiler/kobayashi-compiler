#include "optimizer/pass.hpp"

int exec(CompileUnit &c) {
  // std::cerr<<">>> exec"<<std::endl;
  // simulate IR execute result
  FILE *ifile = fopen("input.txt", "r");
  FILE *ofile = fopen("output.txt", "w");
  long long instr_cnt = 0, mem_r_cnt = 0, mem_w_cnt = 0, jump_cnt = 0,
            fork_cnt = 0, par_instr_cnt = 0;
  int sp = c.scope.size, mem_limit = sp + (8 << 20);
  char *mem = new char[mem_limit];
  auto wMem = [&](int addr, int v) {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ((int *)mem)[addr / 4] = v;
    ++mem_w_cnt;
  };
  auto rMem = [&](int addr) -> int {
    assert(0 <= addr && addr < mem_limit && !(addr % 4));
    ++mem_r_cnt;
    return ((int *)mem)[addr / 4];
  };
  c.scope.for_each([&](MemObject *x) {
    assert(x->global);
    if (x->initial_value) {
      memcpy(mem + x->offset, x->initial_value, x->size);
    } else {
      memset(mem + x->offset, 0, x->size);
    }
  });
  auto dbg_pr = [&] {
    printf("Instr: %lld\n", instr_cnt);
    printf("Load:  %lld\n", mem_r_cnt);
    printf("Store: %lld\n", mem_w_cnt);
    printf("Jump:  %lld\n", jump_cnt);
    printf("Fork:  %lld\n", fork_cnt);
    printf("Parallel:  %g\n", par_instr_cnt * 1. / instr_cnt);
    /*c.scope.for_each([&](MemObject *x){
            assert(x->global);
            printf("%s: ",x->name.data());
            int *data=(int*)(mem+x->offset),size=x->size;
            for(int i=0;i<size/4;++i)printf("%d,",data[i]);
            printf("\n");
    });*/
  };
  bool in_fork = 0;
  int cur_thread = -1;
  bool eol = 1;
  NormalFunc *fork_func = NULL;
  BB *fork_bb = NULL;
  std::list<std::unique_ptr<Instr>>::iterator fork_instr;

  std::function<int(NormalFunc *, std::vector<int>)> run;
  run = [&](NormalFunc *func, std::vector<int> args) -> int {
    BB *last_bb = NULL;
    BB *cur = func->entry;
    int sz = func->scope.size;
    std::unordered_map<int, int> regs;
    auto wReg = [&](Reg x, int v) {
      assert(func->thread_local_regs.count(x) == in_fork);
      assert(1 <= x.id && x.id <= func->max_reg_id);
      regs[x.id] = v;
    };
    auto rReg = [&](Reg x) -> int {
      assert(1 <= x.id && x.id <= func->max_reg_id);
      return regs[x.id];
    };
    for (int i = 0; i < (int)args.size(); ++i) {
      wReg(i + 1, args[i]);
    }
    int _ret = 0;
    bool last_in_fork = in_fork;
    while (cur) {
      // printf("BB: %s\n",cur->name.data());
      for (auto it = cur->instrs.begin(); it != cur->instrs.end(); ++it) {
        Instr *x0 = it->get();
        ++instr_cnt;
        if (in_fork) ++par_instr_cnt;
        Case(PhiInstr, x, x0) {
          for (auto &kv : x->uses)
            if (last_bb == kv.second) wReg(x->d1, rReg(kv.first));
        }
        else Case(LoadAddr, x, x0) {
          wReg(x->d1, (x->offset->global ? 0 : sp) + x->offset->offset);
        }
        else Case(LoadConst, x, x0) {
          wReg(x->d1, x->value);
        }
        else Case(LoadArg, x, x0) {
          wReg(x->d1, args.at(x->id));
        }
        else Case(UnaryOpInstr, x, x0) {
          int s1 = rReg(x->s1), d1 = x->compute(s1);
          wReg(x->d1, d1);
        }
        else Case(BinaryOpInstr, x, x0) {
          int s1 = rReg(x->s1), s2 = rReg(x->s2), d1 = x->compute(s1, s2);
          wReg(x->d1, d1);
        }
        else Case(ArrayIndex, x, x0) {
          int s1 = rReg(x->s1), s2 = rReg(x->s2), d1 = s1 + s2 * x->size,
              max = x->limit;
          wReg(x->d1, d1);
        }
        else Case(LocalVarDef, x, x0) {
          --instr_cnt;
          if (in_fork) --par_instr_cnt;
        }
        else Case(LoadInstr, x, x0) {
          wReg(x->d1, rMem(rReg(x->addr)));
        }
        else Case(StoreInstr, x, x0) {
          wMem(rReg(x->addr), rReg(x->s1));
        }
        else Case(MemDef, x, x0) {
        }
        else Case(MemUse, x, x0) {
        }
        else Case(MemEffect, x, x0) {
        }
        else Case(MemRead, x, x0) {
          wReg(x->d1, rMem(rReg(x->addr)));
        }
        else Case(MemWrite, x, x0) {
          wMem(rReg(x->addr), rReg(x->s1));
        }
        else Case(JumpInstr, x, x0) {
          last_bb = cur;
          cur = x->target;
          if (last_bb->id + 1 != cur->id) ++jump_cnt;
          break;
        }
        else Case(BranchInstr, x, x0) {
          last_bb = cur;
          cur = (rReg(x->cond) ? x->target1 : x->target0);
          if (last_bb->id + 1 != cur->id) ++jump_cnt;
          break;
        }
        else Case(ReturnInstr, x, x0) {
          _ret = rReg(x->s1);
          last_bb = cur;
          cur = NULL;
          jump_cnt += 2;
          break;
        }
        else Case(CallInstr, x, x0) {
          int ret = 0;
          std::vector<int> args;
          for (Reg p : x->args) args.push_back(rReg(p));
          Case(NormalFunc, f, x->f) {
            sp += sz;
            ret = run(f, args);
            sp -= sz;
            if (!f->ignore_return_value) wReg(x->d1, ret);
          }
          else {
            if (x->f->name == "getint") {
              assert(args.size() == 0);
              fscanf(ifile, "%d", &ret);
            } else if (x->f->name == "getch") {
              assert(args.size() == 0);
              ret = fgetc(ifile);
            } else if (x->f->name == "getarray") {
              assert(args.size() == 1);
              fscanf(ifile, "%d", &ret);
              for (int i = 0, x; i < ret; ++i) {
                fscanf(ifile, "%d", &x);
                wMem(args[0] + i * 4, x);
              }
            } else if (x->f->name == "putint") {
              assert(args.size() == 1);
              fprintf(ofile, "%d", args[0]);
              fflush(ofile);
              eol = 0;
            } else if (x->f->name == "putch") {
              assert(args.size() == 1);
              fputc(args[0], ofile);
              fflush(ofile);
              eol = (args[0] == 10);
            } else if (x->f->name == "putarray") {
              assert(args.size() == 2);
              int n = args[0], a = args[1];
              fprintf(ofile, "%d:", n);
              for (int i = 0; i < n; ++i)
                fprintf(ofile, " %d", rMem(a + i * 4));
              fputc(10, ofile);
              fflush(ofile);
              eol = 1;
            } else if (x->f->name == "putf") {
              /*char *c=mem+args[0];
              char buf[1024];
              if(args.size()==1)sprintf(buf,c);
              else if(args.size()==2)sprintf(buf,c,args[1]);
              else if(args.size()==3)sprintf(buf,c,args[1],args[2]);
              else if(args.size()==4)sprintf(buf,c,args[1],args[2],args[3]);
              else
              if(args.size()==5)sprintf(buf,c,args[1],args[2],args[3],args[4]);
              else assert(0);
              fflush(ofile);
              int len=strlen(buf);
              fputs(buf,ofile);
              if(len)eol=(buf[len-1]==10);*/
              assert(0);
            } else if (x->f->name == ".fork") {
              assert(args.size() == 1);
              assert(args[0] >= 1);
              assert(!in_fork);
              fork_func = f;
              fork_bb = cur;
              fork_instr = it;
              in_fork = 1;
              ++fork_cnt;
              ret = args[0] - 1;
              // std::cerr<<">>> fork"<<std::endl;
            } else if (x->f->name == ".join") {
              assert(args.size() == 2);
              assert(args[0] >= 0);
              assert(args[0] < args[1]);
              assert(in_fork);
              assert(fork_func == f);
              if (args[0]) {
                ret = args[0] - 1;
                cur = fork_bb;
                it = fork_instr;
                Case(CallInstr, call, it->get()) { x = call; }
                else assert(0);
              } else {
                fork_func = NULL;
                fork_bb = NULL;
                ret = 0;
                in_fork = 0;
              }
              // std::cerr<<">>> join"<<std::endl;
            }
          }
          if (!x->f->ignore_return_value && !x->ignore_return_value)
            wReg(x->d1, ret);
        }
        else assert(0);
      }
    }
    assert(last_in_fork == in_fork);
    return _ret;
  };
  int ret = run(c.funcs["main"].get(), std::vector<int>());
  dbg_pr();
  delete[] mem;
  if (!eol) fputc(10, ofile);
  fprintf(ofile, "%u\n", ret & 255);
  printf("return %d\n", ret);
  if (ifile) fclose(ifile);
  if (ofile) fclose(ofile);
  return ret;
}
