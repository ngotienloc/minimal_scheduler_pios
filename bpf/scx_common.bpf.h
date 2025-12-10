/* bpf/scx_common.bpf.h */
#ifndef __SCX_COMMON_BPF_H
#define __SCX_COMMON_BPF_H

/* 1. Header chứa mọi định nghĩa của Kernel */
#include "vmlinux.h"

/* 2. Các thư viện chuẩn của Libbpf */
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h> /* Thêm cái này để dùng BPF_CORE_READ */

/* 3. Các cờ dự phòng (Fallback definitions) */
/* Đôi khi vmlinux.h không xuất ra các #define (chỉ xuất enum/struct), 
 * nên ta define lại cho chắc chắn */

#ifndef PF_IDLE
#define PF_IDLE		0x00000002
#endif

#ifndef PF_KTHREAD
#define PF_KTHREAD	0x00200000
#endif

#ifndef PF_EXITING
#define PF_EXITING	0x00000004
#endif

/* 4. Helper ép kiểu tiện lợi */
#define cast_to_task(p) ((struct task_struct *)(p))

#endif /* __SCX_COMMON_BPF_H */