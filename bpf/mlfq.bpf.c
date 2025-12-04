// mlfq.bpf.c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* 4-level MLFQ */
#define DSQ_HIGHEST 0
#define DSQ_HIGH    1
#define DSQ_MED     2
#define DSQ_LOW     3
#define NUM_DSQ     4

/* slice times per level in ns (volatile: can be modified at runtime) */
const volatile u64 SLICE_NS[NUM_DSQ] = {
    [DSQ_HIGHEST] = 1 * 1000 * 1000,  /* 1 ms */
    [DSQ_HIGH]    = 2 * 1000 * 1000,  /* 2 ms */
    [DSQ_MED]     = 4 * 1000 * 1000,  /* 4 ms */
    [DSQ_LOW]     = 8 * 1000 * 1000   /* 8 ms */
};

/* BPF maps */

/* remaining slice (ns) per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);   /* pid */
    __type(value, u64); /* remaining slice ns */
} task_slice SEC(".maps");

/* current queue level per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);   /* pid */
    __type(value, u32); /* queue level */
} task_queue SEC(".maps");

/* record start time (ns) when a task begins running */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);   /* pid */
    __type(value, u64); /* start time ns */
} task_start_ns SEC(".maps");

/* clamp helper */
static __always_inline u32 clamp_level(u32 lvl)
{
    if (lvl >= NUM_DSQ)
        return NUM_DSQ - 1;
    return lvl;
}

/* enable: initialize task -> top queue */
void BPF_STRUCT_OPS(mlfq_enable, struct task_struct *p,
                    struct scx_enable_args *args)
{
    bpf_printk("mlfq: enable pid=%d comm=%s", p->pid, p->comm);

    u32 pid = p->pid;
    u32 level = DSQ_HIGHEST;
    u64 slice = SLICE_NS[level];

    bpf_map_update_elem(&task_queue, &pid, &level, BPF_ANY);
    bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
}

/* enqueue: insert into DSQ according to current level */
void BPF_STRUCT_OPS(mlfq_enqueue, struct task_struct *p, u64 enq_flags)
{
    u32 pid = p->pid;

    u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);
    u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);

    if (!plevel || !pslice) {
        /* initialize if missing */
        u32 l = DSQ_HIGHEST;
        u64 s = SLICE_NS[l];
        bpf_map_update_elem(&task_queue, &pid, &l, BPF_ANY);
        bpf_map_update_elem(&task_slice, &pid, &s, BPF_ANY);
        /* use locals to insert immediately */
        scx_bpf_dsq_insert(p, l, s, enq_flags);
        return;
    }

    scx_bpf_dsq_insert(p, *plevel, *pslice, enq_flags);
}

/* dispatch: poll DSQ from high -> low and move to local */
void BPF_STRUCT_OPS(mlfq_dispatch, s32 cpu, struct task_struct *prev)
{
    for (int lvl = 0; lvl < NUM_DSQ; lvl++) {
        if (scx_bpf_dsq_move_to_local(lvl))
            return;
    }
}

/* running: record start timestamp */
void BPF_STRUCT_OPS(mlfq_running, struct task_struct *p)
{
    u32 pid = p->pid;
    u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&task_start_ns, &pid, &now, BPF_ANY);
}

/* stopping: compute elapsed, subtract slice, possibly demote */
void BPF_STRUCT_OPS(mlfq_stopping, struct task_struct *p, bool runnable)
{
    u32 pid = p->pid;
    u64 now = bpf_ktime_get_ns();

    u64 *pstart = bpf_map_lookup_elem(&task_start_ns, &pid);
    u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);
    u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);

    if (!pslice || !plevel) {
        if (pstart)
            bpf_map_delete_elem(&task_start_ns, &pid);
        return;
    }

    if (pstart && *pstart != 0 && now >= *pstart) {
        u64 elapsed = now - *pstart;

        /* subtract elapsed from remaining slice, saturate to 0 */
        if (elapsed >= *pslice) {
            *pslice = 0;
        } else {
            *pslice -= elapsed;
        }

        /* if slice exhausted -> demote one level (if possible) and reset slice */
        if (*pslice == 0) {
            if (*plevel < (NUM_DSQ - 1)) {
                u32 new_level = *plevel + 1;
                new_level = clamp_level(new_level);
                u64 new_slice = SLICE_NS[new_level];
                bpf_map_update_elem(&task_queue, &pid, &new_level, BPF_ANY);
                bpf_map_update_elem(&task_slice, &pid, &new_slice, BPF_ANY);
            } else {
                /* already lowest: refresh slice */
                u64 new_slice = SLICE_NS[*plevel];
                bpf_map_update_elem(&task_slice, &pid, &new_slice, BPF_ANY);
            }
        } else {
            /* write back updated slice (pslice points to map value) */
            bpf_map_update_elem(&task_slice, &pid, pslice, BPF_ANY);
        }
    }

    if (pstart)
        bpf_map_delete_elem(&task_start_ns, &pid);
}

/* optional exit callback for cleanup */
void BPF_STRUCT_OPS(mlfq_exit, struct scx_exit_info *ei)
{
    /* might cleanup maps here */
}

/* register ops */
SEC(".struct_ops.link")
struct sched_ext_ops mlfq_ops = {
    .enable   = (void *)mlfq_enable,
    .enqueue  = (void *)mlfq_enqueue,
    .dispatch = (void *)mlfq_dispatch,
    .running  = (void *)mlfq_running,
    .stopping = (void *)mlfq_stopping,
    .exit     = (void *)mlfq_exit,
    .name     = "mlfq_4q",
};
