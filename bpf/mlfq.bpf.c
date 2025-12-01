#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include "scx_common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

#define DSQ_HIGEST 0
#define DSQ_HIGH 1
#define DSQ_MED 2 
#define DSQ_LOW 3 
#define NUM_DSQ 4 

const volatile u64 SLICE_NS[NUM_DSQ] = {
    1 * 1000 * 1000,
    2 * 1000 * 1000,
    4 * 1000 * 1000,
    8 * 1000 * 1000
};

//Slice tracking
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, u32);           // pid
    __type(value, u64);         // remaining slice ns
} task_slice SEC(".maps");

// Current queue tracking 
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 1024);
    __type(key, u32);           // pid
    __type(value, u32);         // queue level
} task_queue SEC(".maps");

void BPF_STRUCT_OPS(mlfq_enable, struct task_struct *p, struct scx_enable_args *args)
{
    pbf_printk("Task %s enabled in MLFQ",p->comm); 
    u32 pid = p->pid; 
    u64 slice = SLICE_NS[DSQ_HIGHEST]

    bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
    u32 level = DSQ_HIGEST;
    bpf_map_update_elem(&task_queue, &pid, &level, PBF_ANY);  
}
