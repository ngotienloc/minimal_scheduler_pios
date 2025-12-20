#ifndef __SCX_COMMON_BPF_H__
#define __SCX_COMMON_BPF_H__

/* 1. Các hằng số DSQ cơ bản */
#ifndef SCX_DSQ_GLOBAL
#define SCX_DSQ_GLOBAL 0
#endif

#ifndef SCX_DSQ_LOCAL
#define SCX_DSQ_LOCAL  0xffffffffffffffffULL
#endif

/* 2. Khai báo các hàm kfuncs còn thiếu trong mã của bạn */
extern s32 scx_bpf_create_dsq(u64 dsq_id, s32 node) __ksym __weak;
extern s32 scx_bpf_task_cpu(struct task_struct *p) __ksym __weak;
extern void scx_bpf_dispatch(struct task_struct *p, u64 dsq_id, u64 slice, u64 enq_flags) __ksym __weak;
extern bool scx_bpf_consume(u64 dsq_id) __ksym __weak;
extern void scx_bpf_kick_cpu(s32 cpu, u64 flags) __ksym __weak;

// Bổ sung 2 hàm đang báo lỗi "undeclared"
extern void scx_bpf_dsq_insert(struct task_struct *p, u64 dsq_id, u64 slice, u64 enq_flags) __ksym __weak;
extern bool scx_bpf_dsq_move_to_local(u64 dsq_id) __ksym __weak;

/* 3. Macro BPF_STRUCT_OPS (đã xử lý xung đột ở file .c) */
#ifndef BPF_STRUCT_OPS
#define BPF_STRUCT_OPS(name, args...) \
    SEC("struct_ops/" #name)          \
    name(args)
#endif

/* 4. Loại bỏ enum scx_enq_flags vì vmlinux.h đã có */
/* Nếu sau này bị báo thiếu flag nào đó, ta sẽ bổ sung lẻ sau */

#endif /* __SCX_COMMON_BPF_H__ */
