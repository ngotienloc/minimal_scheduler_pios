#include <linux/module.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/rcupdate.h>
#include <linux/kernel.h>
#include <linux/pid.h>   /* cần để dùng find_get_pid và pid_task */
#include "scx_bpf_helpers.h"

/* 
 * Wrapper BPF lấy CPU của PID
 * Trả về CPU id, hoặc -1 nếu task không tồn tại
 */
s32 scx_bpf_task_cpu_by_pid(s32 pid)
{
    struct task_struct *p;
    s32 cpu = -1;

    rcu_read_lock();
    p = pid_task(find_get_pid(pid), PIDTYPE_PID);
    if (p)
        cpu = task_cpu(p);
    rcu_read_unlock();

    return cpu;
}
EXPORT_SYMBOL_GPL(scx_bpf_task_cpu_by_pid);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SCX BPF Helper");
MODULE_DESCRIPTION("Wrapper for SCX/MLFQ BPF to get CPU by PID");
