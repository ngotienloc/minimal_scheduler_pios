#!/bin/bash
# CPU-bound test
for i in {1..4}; do
    ./examples/cpu_bound &
done

# I/O-bound test
for i in {1..2}; do
    ./examples/io_bound &
done

wait
echo "Workload test finished"
