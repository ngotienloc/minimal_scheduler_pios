#!/bin/bash
set -euo pipefail

OUT_FILE="${1:-vmlinux.h}"

if ! command -v bpftool >/dev/null 2>&1; then
    echo "[ERROR] bpftool not found. Install package linux-tools or libbpf tools"
    exit 1
fi

if [ ! -f /sys/kernel/btf/vmlinux ]; then
    echo "[ERROR] /sys/kernel/btf/vmlinux not found. Kernel needs BTF (CONFIG_DEBUG_INFO_BTF)."
    exit 1
fi

echo "[INFO] Generating $OUT_FILE ..."
bpftool btf dump file /sys/kernel/btf/vmlinux format c > "$OUT_FILE"
echo "[OK] vmlinux.h generated -> $OUT_FILE"
