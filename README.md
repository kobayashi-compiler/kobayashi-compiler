# Kobayashi Compiler

An optimizing compiler of SysY, a subset of C. Supported targets are armv7 and risc-v32. (Specifically, armv7ve and rv32im.)

SysY is the language used by the Collegiate Student System Capability Challenge (Compiler Track) 2021, an optimizing compiler challenge targeting ARM. ([contest homepage](https://compiler.educg.net/2021CSCC))

For SysY specification (in Chinese), runtime, and testcases, see [here](https://gitlab.eduxiji.net/nscscc/compiler2021/-/tree/master).

[README in Chinese](README.zh-CN.md)

## Statistics

Test cases of the contest consist of two parts: functional test cases and performance test cases. The functional test cases were scored by correctness. The performance test cases were scored based on the running time of the compiler output on designated input. In the contest, we passed all the functional test cases. Our performance score and total score both took the first place.

Here are some test results on the performance test cases, tested on a Raspberry Pi 4B. As a comparison, the performance of the code generated by gcc 8.3.0 is also listed. 

Arguments for gcc are `-march=armv7ve -Ofast`.

Test results when automatic parallelization is disabled: [no parallelization](doc/no_parallelization.md)

Test results when automatic parallelization is enabled (add `--enable-loop-parallel` to this compiler, `-ftree-parallelize-loops=4` to gcc): [parallelization](doc/parallelization.md)

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

Use `--set-<key>=<value>` to specify key-value pairs. Available options:

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
doc/intro.pdf
doc/pre.pdf
```

## About Us

Kobayashi is a name from the anime *Miss Kobayashi's Dragon Maid*, and our team name in the contest is *Miss Kobayashi's Compiler*.
