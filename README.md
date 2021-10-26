# Kobayashi Compiler

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
./main code.sy -o asm.s
```

The default target is armv7ve. For rv32im target, add `--set-arch=rv32`.

## Slides

```
slides/intro.pdf
slides/pre.pdf
```

