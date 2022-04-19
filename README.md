# Kobayashi Compiler

An optimizing compiler of SysY, a subset of C.

SysY is the language used by the Collegiate Student System Capability Challenge (Compiler Track) 2021. For its specification (in Chinese) and runtime, see [here](https://gitlab.eduxiji.net/nscscc/compiler2021/-/tree/master). Supported targets are armv7 and risc-v32. (To be accurate, armv7ve and rv32im.)

## Usage

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

### Additional Arguments

Use `--set-<key>=<value>` to specify key-value pairs. Here are the available options:

| key          | value            | default | hint                                                         |
| ------------ | ---------------- | ------- | ------------------------------------------------------------ |
| `arch`       | `armv7`, `rv32`  | `armv7` |                                                              |
| `num-thread` | positive integer | 4       | The number of threads in auto parallelization when `--enable-loop-parallel` is specified. |

Other flags:

| flag                     | hint                                                         |
| ------------------------ | ------------------------------------------------------------ |
| `--enable-loop-parallel` | Enable automatic parallelization. Currently it's not available for `rv32`. |
| `--exec`                 | Simulate the execution of IR instead of generating assembly code. |

Also, remember to link the output with the runtime under `runtime/`.

## Slides

```
slides/intro.pdf
slides/pre.pdf
```

