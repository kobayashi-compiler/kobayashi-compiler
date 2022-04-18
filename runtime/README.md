# Runtime

Assembly code generated by this compiler depends on some external utilities for wrapping interaction with the environment, such as IO, timing, and managing threads. These utilities are provided here.

`sylib.c` is a C implementation of part of the runtime and it depends on libc. It's provided as the official runtime of Collegiate Student System Capability Challenge (Compiler Track) 2021, and is put here just for completeness. `sylib.h` illustrates how the functions in sylib can be used in SysY source code, but isn't really used as a header file, since there is no grammar for including headers in SysY.

Apart from sylib, we implemented some thread management utilities to support automatic parallelization, and it's under the `<arch>/` directory.

Both `sylib.c` and `<arch>/thread.S` should be linked with the generated code.