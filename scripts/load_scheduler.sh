#!/bin/bash
# Load MLFQ scheduler
sudo bpftool struct_ops register bpf/mlfq.bpf.o /sys/fs/bpf/sched_ext
echo "MLFQ scheduler loaded"
