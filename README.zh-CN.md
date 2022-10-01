# Kobayashi Compiler

一个 SysY 语言的优化编译器，有 armv7 和 risc-v32 后端。（更精确地说，支持的 target 是 armv7ve 和 rv32im。）

SysY 是全国大学生计算机系统能力大赛编译系统设计赛2021的比赛语言。（[赛事主页](https://compiler.educg.net/2021CSCC)）

[这里](https://gitlab.eduxiji.net/nscscc/compiler2021/-/tree/master)有SysY的语言规范、runtime 和比赛测例。

## 统计

比赛中有两部分测例：功能测例和性能测例。正确通过功能测例即得分，而性能测例的分数由编译器生成的代码的运行时间决定。在比赛中，我们通过了所有功能测例。（决赛中的大部分队也都全部通过了。）我们的性能得分和总分都排名第一。

这里是比赛的性能测例上的测试结果。测试环境是一台 Raspberry Pi 4B。作为对比，也列出了 `gcc (Raspbian 8.3.0-6+rpi1) 8.3.0` 生成的代码的运行时间。

gcc 参数是 `-march=armv7ve -Ofast`。

不开启自动并行化的测试结果：[no parallelization](doc/no_parallelization.md)

开启自动并行化的测试结果（为此编译器加上 `--enable-loop-parallel` 参数，为 gcc 加上 `-ftree-parallelize-loops=4`）：[parallelization](doc/parallelization.md)

## 用法

0. Build

```bash
mkdir build
cd build
cmake ..
make -j$(nproc)
```

1. Run

```bash
./compiler <source_code>.sy -o <output_asm>.s
```

## 其他命令行参数

用 `--set-<key>=<value` 来指定键值对以改变它的一些行为。

| key          | value           | 默认值  | 备注                                                         |
| ------------ | --------------- | ------- | ------------------------------------------------------------ |
| `arch`       | `armv7`, `rv32` | `armv7` |                                                              |
| `num-thread` | 正整数          | 4       | 这是自动并行化时使用的线程数，仅在指定 ``--enable-loop-parallel` 时有作用。 |

其他 flag

| flag                                        | hint                                   |
| ------------------------------------------- | -------------------------------------- |
| `--enable-loop-parallel`                    | 开启自动并行化，目前对 `rv32` 不可用。 |
| `--exec`                                    | 模拟执行 IR，而非输出汇编代码。        |
| `--debug`, `--info`, `--warning`, `--error` | 不同的日志等级，默认为 warning。       |

此外，输出的代码要与 `runtime/` 目录下的 runtime 链接才能执行。

## 展示材料

```
slides/intro.pdf
slides/pre.pdf
```

## 关于我们

kobayashi 出自《小林家的龙女仆》，我们在比赛中的队名是“小林家的编译器”。