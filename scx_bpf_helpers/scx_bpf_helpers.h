#ifndef SCX_BPF_HELPERS_H
#define SCX_BPF_HELPERS_H

#include <linux/types.h>
#include <bpf/bpf_helpers.h>


s32 scx_bpf_task_cpu_by_pid(s32 pid);

SEC(".struct_ops")
static __always_inline u64 scx_bpf_dsq_insert(struct task_struct *p,
                                              u32 lvl,
                                              u64 slice,
                                              u64 flags)
{
    // Tại đây bạn gọi helper kernel thực tế hoặc làm logic trong BPF
    // Giữ nguyên kiểu struct_task *p
    return 0;
}

// Macro khai báo helper sleepable
#define BPF_HELPER_DSQ_INSERT \
    __attribute__((always_inline, noinline)) \
    static u64 scx_bpf_dsq_insert(struct task_struct *p, u32 lvl, u64 slice, u64 flags)

#endif /* SCX_BPF_HELPERS_H */
