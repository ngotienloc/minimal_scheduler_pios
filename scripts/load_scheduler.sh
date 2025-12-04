#!/bin/bash
set -euo pipefail

OBJ="$(cd "$(dirname "$0")/.." && pwd)/build/mlfq.bpf.o"
TARGET="/sys/fs/bpf/sched_ext/mlfq_4q"

if [ ! -f "$OBJ" ]; then
    echo "[ERROR] $OBJ not found. Run 'make' first."
    exit 1
fi

sudo mkdir -p /sys/fs/bpf/sched_ext
echo "[INFO] Registering struct_ops..."
sudo bpftool struct_ops register "$OBJ" "$TARGET"
echo "[OK] MLFQ loaded as $TARGET"
