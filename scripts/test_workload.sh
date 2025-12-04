#!/bin/bash
set -euo pipefail

# build example
gcc -O2 examples/cpu_bound.c -o examples/cpu_bound

# spawn several CPU-bound tasks
./examples/cpu_bound &
./examples/cpu_bound &
./examples/cpu_bound &

echo "[INFO] started cpu_bound x3"
wait
