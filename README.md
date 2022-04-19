# Kobayashi Compiler

An optimizing compiler of SysY, a subset of C. Supported targets are armv7 and risc-v32. (To be accurate, armv7ve and rv32im.)

SysY is the language used by the Collegiate Student System Capability Challenge (Compiler Track) 2021, an compiler contest in China. ([contest homepage](https://compiler.educg.net/2021CSCC))

For SysY specification (in Chinese), runtime, and testcases, see [here](https://gitlab.eduxiji.net/nscscc/compiler2021/-/tree/master).

[README in Chinese](README.zh-CN.md)

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

| flag                                        | hint                                                         |
| ------------------------------------------- | ------------------------------------------------------------ |
| `--enable-loop-parallel`                    | Enable automatic parallelization. Currently it's not available for `rv32`. |
| `--exec`                                    | Simulate the execution of IR instead of generating assembly code. |
| `--debug`, `--info`, `--warning`, `--error` | Different levels of logging. default to warning level.       |

Also, remember to link the output with the runtime under `runtime/`.

## Slides

```
slides/intro.pdf
slides/pre.pdf
```

## About Us

Kobayashi is a name from the anime *Miss Kobayashi's Dragon Maid*, and our team name in the contest is *Miss Kobayashi's Compiler*.
