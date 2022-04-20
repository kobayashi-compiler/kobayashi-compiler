#!/usr/bin/env python3
# -*- coding: utf-8 -*-

# This script tests riscv code with qemu
# Please change the constant values according to your environment.
import os
import subprocess
import sys
import time
from glob import glob

from parse import parse

OK = '\033[92m'
FAIL = '\033[91m'
END = '\033[0m'

TIMEOUT = 100
ROUND = 1

scenes = ['function_test2020', 'function_test2021',
          'performance_test2021-public', 'performance_test2021-private']
scene = scenes[int(sys.argv[1])]

testcase = os.path.join('../../compiler2021/公开用例与运行时库', scene)
compiler = '../build/compiler'
runtime = '../runtime/sylib.c ../runtime/rv32/thread.S'
cc = 'riscv64-unknown-elf-gcc -march=rv32im -mabi=ilp32'
qemu = 'qemu-riscv32'

try:
    testcases = sorted([os.path.splitext(os.path.basename(file))[0]
                        for file in glob(os.path.join(testcase, '*.sy'))], key=lambda x: int(x.split('_')[0]))
except:
    testcases = sorted(os.path.splitext(os.path.basename(file))[
                       0] for file in glob(os.path.join(testcase, '*.sy')))

for base in testcases:
    fails = list()

    # names
    try:
        case = base.split('_')
        case[0] = str(int(case[0])).zfill(3)
        case = '_'.join(case)
    except:
        case = base
    print(f'{case}  ', end='', flush=True)

    sy_file = os.path.join(testcase, f'{base}.sy')
    in_file = os.path.join(testcase, f'{base}.in')
    ans_file = os.path.join(testcase, f'{base}.out')

    # compile
    os.system(
        ' '.join([compiler, sy_file, '-o', 'asm.s', '--set-arch=rv32'] + sys.argv[2:]))
    os.system(f'{cc} asm.s {runtime} -o main')
    os.system(f'touch {in_file}')

    # time
    avg = 0
    for i in range(ROUND):
        print(f'\b{i}', end='', flush=True)
        p = subprocess.Popen(f'{qemu} ./main', shell=True, stdin=open(in_file),
                             stdout=open('out.txt', 'w'), stderr=open('time.txt', 'w'))
        try:
            p.wait(TIMEOUT)
        except subprocess.TimeoutExpired:
            p.kill()
        ret = p.returncode
        with open('time.txt') as f:
            timing = list(filter(None, f.read().strip().split('\n')))
        if timing:
            h, m, s, us = map(int, parse(
                'TOTAL: {}H-{}M-{}S-{}us', timing[-1]))
            timing = ((h * 60 + m) * 60 + s) * 1000000 + us
            timing /= 1000000
        else:
            timing = 0.0
        avg += timing
    avg /= ROUND

    # judge
    with open('out.txt') as f:
        out = f.read().strip()
    with open(ans_file) as f:
        ans = f.read().strip().split('\n')
    ans_out, ans_ret = '\n'.join(ans[:-1]).strip(), int(ans[-1])
    if ans_out == out and ans_ret == ret:
        print(f'\r[{OK}OK{END}]      {round(avg, 4):7.3f} | {case}')
    else:
        print(f'\r[{FAIL}FAILED{END}]  {round(avg, 4):7.3f} | {case}')
        fails.append(case)
    os.remove('main')

if not fails:
    print(f'All {scene} passed!')
else:
    for fail in fails:
        print(f'{FAIL}{fail}{END} failed!')
