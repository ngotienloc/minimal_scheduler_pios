#!/bin/bash
set -euo pipefail

TARGET="/sys/fs/bpf/sched_ext/mlfq_4q"

echo "[INFO] Unregistering struct_ops..."
sudo bpftool struct_ops unregister "$TARGET" || true
sudo rm -f "$TARGET" || true
echo "[OK] MLFQ unloaded"
