# Runtime

Assembly code generated by this compiler depends on some external utilities wrapping interaction with the environment, such as IO, timing, and managing threads. These utilities are provided here.

`sylib.c` is a C implementation of part of the runtime and it depends on libc. It's provided as the official runtime of Collegiate Student System Capability Challenge (Compiler Track) 2021, and is put here just for completeness. `sylib.h` illustrates how the functions in sylib can be used in SysY source code, but isn't really used as a header file, since there is no syntax for including headers in SysY.

Apart from sylib, we implemented some thread management utilities to support automatic parallelization, and it's under the `<arch>/` directory.

Both `sylib.c` and `<arch>/thread.S` should be linked with the generated code.

Chinese version:

# 运行时

这个编译器产生的汇编代码依赖于一些外部库提供的东西，也就是运行时。它们包装了与环境的交互，比如说 IO，测时，线程管理。

`sylib.c` 是运行时的一部分的 C 实现，依赖于 libc。它是全国大学生计算机系统能力大赛编译系统设计赛2021官方提供的运行时，放在这里只是出于完整性考虑。`sylib.h` 以头文件的风格解释了 sylib 提供的功能要怎样在 SysY 源码中使用。但它并不真正被用作一个头文件，毕竟 SysY中没有 include 语法。

除了 sylib，我们还实现了一些线程管理的工具以支持自动并行化，它放在 `<arch>/` 目录下。

`sylib.c` 和 `<arch>/thread.S` 都需要被链接进去。
