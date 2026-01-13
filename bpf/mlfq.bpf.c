// bpf/mlfq_final_production_v3.bpf.c
#include ".output/vmlinux.h"
#undef BPF_STRUCT_OPS
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
//#include <bpf/bpf_struct_ops.h>
#include <linux/sched/ext.h>
#include <bpf/bpf_core_read.h>
//#include "scx_common.bpf.h"
#include "common.h"
#include "common.bpf.h"

char LICENSE[] SEC("license") = "GPL";

/* --- CONFIG --- */
#define DSQ_HIGHEST 0
#define DSQ_HIGH    1
#define DSQ_MED     2
#define DSQ_LOW     3
#define NUM_DSQ     4


/* Time slices in nanoseconds */
const volatile u64 SLICE_NS[NUM_DSQ] = {
    [DSQ_HIGHEST] = 5ULL * 1000 * 1000,   /* 5ms */
    [DSQ_HIGH]    = 10ULL * 1000 * 1000,
    [DSQ_MED]     = 20ULL * 1000 * 1000,
    [DSQ_LOW]     = 40ULL * 1000 * 1000
};

const volatile u64 BOOST_NS = 500ULL * 1000 * 1000; /* 500ms */

/* Helper Sleepable*/
// Cho phép helper sleepable
/*static __u64 BPF_FUNC(scx_bpf_dsq_insert,
                      struct task_struct *p, u32 lvl, u64 slice, u64 flags)
__attribute__((section(".bpf_helpers"), used));
*/

/* --- MAPS --- */
static long (*bpf_map_get_next_key)(void *map, const void *key, void *next_key) = (void *) 5;
//extern int bpf_map_get_next_key(void *map, const void *key, void *next_key) __ksym;
/* Remaining time slice per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, s32);
    __type(value, u64);
} task_slice SEC(".maps");

/* Current queue level per PID */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, s32);
    __type(value, u32);
} task_queue SEC(".maps");

/* Start timestamp while running */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, s32);
    __type(value, u64);
} task_start_ns SEC(".maps");

/* Enqueue timestamp for aging check */
struct {
    __uint(type, BPF_MAP_TYPE_HASH);
    __uint(max_entries, 8192);
    __type(key, s32);
    __type(value, u64);
} task_enq_ns SEC(".maps");

/* User-space create DSQ */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, NUM_DSQ);
    __type(key, u32);
    __type(value, u32);
} dsq_trigger SEC(".maps");


/* CPU -> current PID map */
#define MAX_CPUS 1024
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, MAX_CPUS);
    __type(key, u32);
    __type(value, s32);
} cpu_curr_pid SEC(".maps");

/* Iterator cursor for aging. -1 means start from beginning */
struct {
    __uint(type, BPF_MAP_TYPE_ARRAY);
    __uint(max_entries, 1);
    __type(key, u32);
    __type(value, s32);
} aging_cursor SEC(".maps");


/* --- INIT --- */
//SEC("struct_ops/init_task")
int BPF_STRUCT_OPS_SLEEPABLE(mlfq_init) {
//s32 mlfq_init_task(struct task_struct *p, struct scx_init_task_args *args){
//static int mlfq_init(void)
//{
    /* Initialize DSQs */
    /*static bool dsq_created = false;	
    
    if (!dsq_created){
    	for (int i = 0; i < NUM_DSQ; i++) 
		scx_bpf_create_dsq(i, -1); 
	dsq_created = true;
    } */
    /* FIX: Initialize aging cursor to -1 (sentinel) */
    /* Map mặc định là 0, ta cần set -1 để báo hiệu "bắt đầu từ đầu" */
    #pragma unroll
    for (int i = 0; i < 4; i++) {
	int key = i;
	scx_bpf_create_dsq(i,-1);
        bpf_map_update_elem(&dsq_trigger, &key, &key, BPF_ANY);
    }
    /*bpf_map_update_elem(&dsq_trigger, &(u32){0}, &(u32){0}, BPF_ANY);
    bpf_map_update_elem(&dsq_trigger, &(u32){1}, &(u32){1}, BPF_ANY);
    bpf_map_update_elem(&dsq_trigger, &(u32){2}, &(u32){2}, BPF_ANY);
    bpf_map_update_elem(&dsq_trigger, &(u32){3}, &(u32){3}, BPF_ANY);
*/
    u32 idx = 0;
    s32 start_sentinel = -1;
    bpf_map_update_elem(&aging_cursor, &idx, &start_sentinel, BPF_ANY);

    bpf_printk("mlfq: init v3 OK\n");
    return 0;
}

/* --- EXIT TASK --- */
//void BPF_STRUCT_OPS(mlfq_exit_task, struct task_struct *p) {
void BPF_STRUCT_OPS(mlfq_exit_task,
                    const struct task_struct *p,
                    struct scx_exit_task_args *args){
    if(!p) return;
    s32 pid = BPF_CORE_READ(p, pid);
    if (pid == 0) return;

    bpf_map_delete_elem(&task_slice, &pid);
    bpf_map_delete_elem(&task_queue, &pid);
    bpf_map_delete_elem(&task_start_ns, &pid);
    bpf_map_delete_elem(&task_enq_ns, &pid);

    /* Safe CPU cleanup */
    s32 cpu_of_task = BPF_CORE_READ(p, thread_info.cpu); // scx_bpf_task_cpu((const struct task_struct *)p);
    if (cpu_of_task >= 0 && cpu_of_task < MAX_CPUS) {
        u32 cpu_idx = (u32)cpu_of_task;
        s32 *curr = bpf_map_lookup_elem(&cpu_curr_pid, &cpu_idx);
        if (curr && *curr == pid) {
            s32 zero = 0;
            bpf_map_update_elem(&cpu_curr_pid, &cpu_idx, &zero, BPF_ANY);
        }
    }
}

/* --- ENABLE --- */
void BPF_STRUCT_OPS(mlfq_enable, struct task_struct *p) {
    if (!p) return;
    s32 pid = BPF_CORE_READ(p, pid);
    if (pid == 0) return;

    u32 lvl = DSQ_HIGHEST;
    u64 slice = SLICE_NS[lvl];

    bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
    bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
    /* Clear stale data */
    bpf_map_delete_elem(&task_enq_ns, &pid);
    bpf_map_delete_elem(&task_start_ns, &pid);
}

/* --- ENQUEUE --- */
SEC("struct_ops/mlfq_enqueue")
void BPF_PROG(mlfq_enqueue, struct task_struct *p, u64 enq_flags)
//u64 BPF_STRUCT_OPS(mlfq_enqueue, struct task_struct *p, u64 enq_flags)
{
    //if (!p) return 0 ;
    //s32 pid = /*BPF_CORE_READ(p, pid);*/ p->pid;
    //if (pid == 0) return 0; /* Skip swapper explicitly */
    u64 flags = (u64)enq_flags;
    //s32 pid = bpf_get_current_pid_tgid() & 0xFFFFFFFF;
    s32 pid = BPF_CORE_READ(p, pid);
    if (pid == 0)  
        return  ;

    
    u64 now = bpf_ktime_get_ns();
    //bpf_map_update_elem(&task_enq_ns, &pid, &now, BPF_NOEXIST);
   // bpf_map_update_elem(&task_enq_ns, &pid, &now, BPF_ANY);
      bpf_map_update_elem(&task_enq_ns, &pid, &now, BPF_NOEXIST);
    
    u32 lvl = DSQ_HIGHEST;
    u64 slice = SLICE_NS[lvl];
    u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);
    u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);

    if (plevel && pslice) {
        lvl = *plevel;
        slice = *pslice;
    } else {
        bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
        bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
    }

    
    scx_bpf_dsq_insert(p, lvl,slice, enq_flags);
    int cpu = BPF_CORE_READ(p, thread_info.cpu);
    if (cpu >= 0 && cpu < MAX_CPUS) {
        u32 cpu_idx = (u32)cpu;
        s32 *curr_pid_ptr = bpf_map_lookup_elem(&cpu_curr_pid, &cpu_idx);
        
        if (curr_pid_ptr && *curr_pid_ptr > 0 && *curr_pid_ptr != pid) {
            s32 curr_pid = *curr_pid_ptr;
            u32 curr_lvl = DSQ_LOW; 
            u32 *cl = bpf_map_lookup_elem(&task_queue, &curr_pid);
            if (cl) curr_lvl = *cl;
            
            if (lvl < curr_lvl) {
                scx_bpf_kick_cpu(cpu, SCX_KICK_PREEMPT);
            }
        }
    }
    return  ;
}

/* --- DISPATCH --- */
void BPF_STRUCT_OPS(mlfq_dispatch, s32 cpu, struct task_struct *prev)
{
    u64 now = bpf_ktime_get_ns();
    u32 cursor_idx = 0;
    
    /* Lấy vị trí duyệt map lần trước */
    s32 *cursor_ptr = bpf_map_lookup_elem(&aging_cursor, &cursor_idx);
    /*s32 current_key = -1;
    if (cursor_ptr) current_key = *cursor_ptr;
*/
    s32 current_key = (cursor_ptr) ? *cursor_ptr : -1;
    s32 next_key;
    
    /* FIX: Logic duyệt map an toàn với sentinel -1 */
    #pragma unroll
    for (int i = 0; i < 32; i++) {
        long ret;
         if (current_key == -1) {
            /* Nếu key là -1, gọi với NULL để lấy phần tử ĐẦU TIÊN của map */
            ret = bpf_map_get_next_key(&task_enq_ns, NULL, &next_key);  
        } else {
            /* Nếu key khác -1, tìm key kế tiếp */
            /*ret = bpf_map_get_next_key(&task_enq_ns, 
					(current_key == -1) ? NULL : &current_key, &next_key);*/
	    ret = bpf_map_get_next_key(&task_enq_ns, &current_key, &next_key);
        }

        if (ret != 0) {
            /* Hết map, reset về -1 để lần sau duyệt lại từ đầu */
            current_key = -1;
            break; 
        }

        /* FIX: Bỏ qua pid 0 nếu vô tình lọt vào */
        if (next_key == 0) {
            current_key = next_key;
            continue;
        }

        /* Check aging */
        u64 *tenq = bpf_map_lookup_elem(&task_enq_ns, &next_key);
        if (tenq && (now - *tenq > BOOST_NS)) {
            /* Boost task */
            u32 newlvl = DSQ_HIGHEST;
            u64 newslice = SLICE_NS[DSQ_HIGHEST];
            bpf_map_update_elem(&task_queue, &next_key, &newlvl, BPF_ANY);
            bpf_map_update_elem(&task_slice, &next_key, &newslice, BPF_ANY);
            /* Reset waiting time to avoid double boost */
            bpf_map_update_elem(&task_enq_ns, &next_key, &now, BPF_ANY);
        }
        
        current_key = next_key;
	bpf_printk("MLFQ: Dispatching on CPU %d\n", cpu);
    }
    
    /* Lưu lại cursor cho lần dispatch sau */
    bpf_map_update_elem(&aging_cursor, &cursor_idx, &current_key, BPF_ANY);

    /* Dispatch tasks */
    for (int i = 0; i < NUM_DSQ; i++) {
        if (scx_bpf_dsq_move_to_local(i))
            return;
    }
}

/* --- STOPPING --- */
void BPF_STRUCT_OPS(mlfq_stopping, struct task_struct *p, bool runnable)
{
    if (!p) return;
    s32 pid = BPF_CORE_READ(p, pid);
    if (pid == 0) return;

    u64 now = bpf_ktime_get_ns();
    u64 *pstart = bpf_map_lookup_elem(&task_start_ns, &pid);
    u64 *pslice = bpf_map_lookup_elem(&task_slice, &pid);
    u32 *plevel = bpf_map_lookup_elem(&task_queue, &pid);

    if (pstart && pslice && plevel) {
        u64 elapsed = now - *pstart;
        u64 remaining = *pslice;

        if (elapsed >= remaining) remaining = 0;
        else remaining -= elapsed;

        if (remaining == 0) {
            u32 old_lvl = *plevel;
            u32 next_lvl = (old_lvl < NUM_DSQ - 1) ? old_lvl + 1 : old_lvl;
        	if (next_lvl >= NUM_DSQ) {
	    	    next_lvl = NUM_DSQ -1;
		}
		if (next_lvl < NUM_DSQ){
                    u64 reset_slice = SLICE_NS[next_lvl];
                    bpf_map_update_elem(&task_queue, &pid, &next_lvl, BPF_ANY);
                    bpf_map_update_elem(&task_slice, &pid, &reset_slice, BPF_ANY);
		}
        } else {
            bpf_map_update_elem(&task_slice, &pid, &remaining, BPF_ANY);
        }
    }

    bpf_map_delete_elem(&task_start_ns, &pid);

    int cpu = BPF_CORE_READ(p, thread_info.cpu);
    if (cpu >= 0 && cpu < MAX_CPUS) {
        u32 cpu_idx = (u32)cpu;
        s32 *curr = bpf_map_lookup_elem(&cpu_curr_pid, &cpu_idx);
        if (curr && *curr == pid) {
            s32 zero = 0;
            bpf_map_update_elem(&cpu_curr_pid, &cpu_idx, &zero, BPF_ANY);
        }
    }
}

/* --- RUNNING --- */
void BPF_STRUCT_OPS(mlfq_running, struct task_struct *p)
{
    if (!p) return;
    s32 pid = BPF_CORE_READ(p, pid);
    if (pid == 0) return;
    
    u32 cpu = bpf_get_smp_processor_id();
    if (cpu < MAX_CPUS) {
        bpf_map_update_elem(&cpu_curr_pid, &cpu, &pid, BPF_ANY);
    }

    u64 now = bpf_ktime_get_ns();
    /* Task chạy thì không còn "waiting" nữa -> xóa khỏi map aging check */
    bpf_map_delete_elem(&task_enq_ns, &pid);
    bpf_map_update_elem(&task_start_ns, &pid, &now, BPF_ANY);
    
    if (!bpf_map_lookup_elem(&task_queue, &pid)) {
        u32 lvl = DSQ_HIGHEST;
        u64 slice = SLICE_NS[lvl];
        bpf_map_update_elem(&task_queue, &pid, &lvl, BPF_ANY);
        bpf_map_update_elem(&task_slice, &pid, &slice, BPF_ANY);
    }
}

/* --- EXIT MODULE --- */
void BPF_STRUCT_OPS(mlfq_exit, struct scx_exit_info *ei) {
    bpf_printk("mlfq: unloaded\n");
}

/*SEC(".struct_ops")
struct sched_ext_ops mlfq_ops = {
    .enable     = (void *)mlfq_enable,
    .enqueue    = (void *)mlfq_enqueue,
    .dispatch   = (void *)mlfq_dispatch,
    .stopping   = (void *)mlfq_stopping,
    .running    = (void *)mlfq_running,
    .exit       = (void *)mlfq_exit,
    .exit_task  = (void *)mlfq_exit_task,
    //.init       = mlfq_init,
    //.init       = (void *)mlfq_init,
    .name       = "mlfq",
    .flags      = SCX_OPS_KEEP_BUILTIN_IDLE,	
}; */


SEC(".struct_ops")
struct sched_ext_ops mlfq_ops = {
    //.init_task  = (void *)mlfq_init_task,
    .init       = (void *)mlfq_init,
    .enable     = (void *)mlfq_enable,
    .enqueue    = (void *)mlfq_enqueue,
    .dispatch   = (void *)mlfq_dispatch,
    .stopping   = (void *)mlfq_stopping,
    .running    = (void *)mlfq_running,
    .exit       = (void *)mlfq_exit,
    .exit_task  = (void *)mlfq_exit_task,
    .name       = "mlfq",
    .flags      = SCX_OPS_KEEP_BUILTIN_IDLE | SCX_OPS_ENQ_LAST,
};
 
