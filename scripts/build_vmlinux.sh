#!/bin/bash
# Tao vmlinux tu BTF(BPF Type Format)
bpftool btf dump file /sys/kernel/btf/vmlinux format c > vmlinux.h
echo "vmlinux.h generated "