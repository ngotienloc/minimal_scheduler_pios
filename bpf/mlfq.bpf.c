// bpf/mlfq_switch.bpf.c
#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "scx_common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* Queue levels */
#define DSQ_HIGHEST 0
#define DSQ_HIGH    1
#define DSQ_MED     2
#define DSQ_LOW     3
#define NUM_DSQ     4

/* Time slice per queue (ns) */
const volatile u64 SLICE_NS[NUM_DSQ] = {
    [DSQ_HIGHEST] = 1 * 1000 * 1000,  /* 1 ms */
    [DSQ_HIGH]    = 2 * 1000 * 1000,  /* 2 ms */
    [DSQ_MED]     = 4 * 1000 * 1000,  /* 4 ms */
    [DSQ_LOW]     = 8 * 1000 * 1000   /* 8 ms */
};

/* boost threshold */
const volatile u64 BOOST_NS = 50 * 1000 * 1000;  // 50ms

/* remaining slice per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);
    __type(value, u64);
} task_slice SEC(".maps");

/* queue level per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);
    __type(value, u32);
} task_queue SEC(".maps");

/* time when task started running */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);
    __type(value, u64);
} task_start_ns SEC(".maps");

/* last enqueue timestamp */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 4096);
    __type(key, u32);
    __type(value, u64);
} task_enq_ns SEC(".maps");


/* Clamp helper */
static __always_inline u32 clamp_level(u32 lvl)
{
    if (lvl >= NUM_DSQ)
        return NUM_DSQ - 1;
    return lvl;
}


/* ENABLE: initialize task -> highest queue */
void BPF_STRUCT_OPS(mlfq_enable, struct task_struct *p, struct scx_enable_args *args)
{
    if (!p)
        return;

    u32 pid = p->pid;
    u32 lvl = DSQ_HIGHEST;
    u64 slice = SLICE_NS[lvl];

    bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
    bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);

    bpf_printk("mlfq: enable pid=%d comm=%s", pid, p->comm);
}


/* ENQUEUE: put task in queue with remaining slice */
void BPF_STRUCT_OPS(mlfq_enqueue, struct task_struct *p, u64 enq_flags)
{
    if (!p)
        return;

    u32 pid = p->pid;
    u64 now = bpf_ktime_get_ns();
    bpf_map_update_elem(&task_enq_ns, &pid, &now, BPF_ANY);

    u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);
    u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);

    /* Not initialized? → put to top */
    if (!plevel || !pslice) {
        u32 lvl = DSQ_HIGHEST;
        u64 slice = SLICE_NS[lvl];
        bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
        bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
        scx_bpf_dsq_insert(p, lvl, slice, enq_flags);
        return;
    }

    scx_bpf_dsq_insert(p, *plevel, *pslice, enq_flags);
}


/* DISPATCH: try queues high → low + Boost aging */
void BPF_STRUCT_OPS(mlfq_dispatch, s32 cpu, struct task_struct *prev)
{
    u64 now = bpf_ktime_get_ns();
    u32 key;

    /* Aging: check only 1 PID */
    if (bpf_map_get_next_key(&task_enq_ns, NULL, &key) == 0) {
        u64 *t = bpf_map_lookup_elem(&task_enq_ns, &key);
        if (t && now - *t >= BOOST_NS) {
            u32 newlvl = DSQ_HIGHEST;
            u64 newslice = SLICE_NS[newlvl];

            bpf_map_update_elem(&task_queue, &key, &newlvl, BPF_ANY);
            bpf_map_update_elem(&task_slice, &key, &newslice, BPF_ANY);

            /* avoid boosting again immediately */
            bpf_map_update_elem(&task_enq_ns, &key, &now, BPF_ANY);

            bpf_printk("mlfq: BOOST pid=%d to lvl0", key);
        }
    }

    /* Pick from queues */
    for (int lvl = 0; lvl < NUM_DSQ; lvl++) {
        if (scx_bpf_dsq_move_to_local(lvl))
            return;
    }
}


/* SWITCH: accounting + demotion */
void BPF_STRUCT_OPS(mlfq_switch, struct task_struct *prev, struct task_struct *next)
{
    u64 now = bpf_ktime_get_ns();

    /* Handle prev */
    if (prev) {
        u32 pid = prev->pid;

        u64 *pstart = bpf_map_lookup_elem(&task_start_ns, &pid);
        u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);
        u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);

        if (pstart && pslice && plevel) {
            u64 elapsed = now - *pstart;

            if (elapsed >= *pslice)
                *pslice = 0;
            else
                *pslice -= elapsed;

            /* Demote if slice used up */
            if (*pslice == 0) {
                if (*plevel < NUM_DSQ - 1) {
                    u32 nl = *plevel + 1;
                    nl = clamp_level(nl);
                    u64 ns = SLICE_NS[nl];
                    bpf_map_update_elem(&task_queue, &pid, &nl, BPF_ANY);
                    bpf_map_update_elem(&task_slice, &pid, &ns, BPF_ANY);

                    bpf_printk("mlfq: demote pid=%d %d->%d", pid, *plevel, nl);
                } else {
                    /* lowest → refresh slice */
                    u64 ns = SLICE_NS[*plevel];
                    bpf_map_update_elem(&task_slice, &pid, &ns, BPF_ANY);
                }
            } else {
                bpf_map_update_elem(&task_slice, &pid, pslice, BPF_ANY);
            }
        }

        if (pstart)
            bpf_map_delete_elem(&task_start_ns, &pid);
    }


    /* Handle next */
    if (next) {
        u32 pid = next->pid;
        u64 start = now;

        /* task is running → clear enqueue timestamp */
        bpf_map_delete_elem(&task_enq_ns, &pid);

        bpf_map_update_elem(&task_start_ns, &pid, &start, BPF_ANY);

        u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);
        u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);

        if (!plevel || !pslice) {
            u32 lvl = DSQ_HIGHEST;
            u64 slice = SLICE_NS[lvl];
            bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
            bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
        }
    }
}


/* EXIT */
void BPF_STRUCT_OPS(mlfq_exit, struct scx_exit_info *ei)
{
    bpf_printk("mlfq: exit\n");
}

/* Register ops */
SEC(".struct_ops.link")
struct sched_ext_ops mlfq_ops = {
    .enable   = (void *)mlfq_enable,
    .enqueue  = (void *)mlfq_enqueue,
    .dispatch = (void *)mlfq_dispatch,
    .switch   = (void *)mlfq_switch,
    .exit     = (void *)mlfq_exit,
    .name     = "mlfq_4q_switch",
};
