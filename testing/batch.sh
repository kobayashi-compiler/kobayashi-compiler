#!/bin/bash

cd /home/pi/kobayashi-compiler/build || exit
cmake ..
make -j$(nproc)

cd /home/pi/kobayashi-compiler/testing || exit
python3 test.py 0
python3 test.py 1
python3 test.py 2
python3 test.py 3
