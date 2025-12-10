// main.c
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "mlfq.skel.h"

static volatile bool exiting = false;

static void sig_handler(int sig)
{
    printf("\nReceived signal, stopping MLFQ Scheduler...\n");
    exiting = true;
}

static int bump_memlock_rlimit(void)
{
    struct rlimit rlim_new = {
        .rlim_cur = RLIM_INFINITY,
        .rlim_max = RLIM_INFINITY,
    };

    if (setrlimit(RLIMIT_MEMLOCK, &rlim_new)) {
        fprintf(stderr, "Failed to increase RLIMIT_MEMLOCK limit!\n");
        return -1;
    }
    return 0;
}

static int libbpf_print_fn(enum libbpf_print_level level, const char *format, va_list args)
{
    return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
    struct mlfq_bpf *skel;
    int err;

    /* Setup logging để thấy mọi lỗi */
    libbpf_set_print(libbpf_print_fn);

    bump_memlock_rlimit();
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* Open */
    skel = mlfq_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    /* Load */
    printf("Loading BPF into kernel...\n");
    err = mlfq_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    /* Attach */
    err = mlfq_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
        goto cleanup;
    }
    
    /* Double check link */
    if (!skel->links.mlfq_ops) {
        fprintf(stderr, "Error: mlfq_ops link is NULL (Struct_ops not attached?)\n");
        goto cleanup;
    }

    printf("SUCCESS: MLFQ Scheduler is running on Raspberry Pi!\n");
    printf("Press Ctrl+C to stop.\n");

    /* Main loop */
    while (!exiting) {
        sleep(1);
    }

cleanup:
    mlfq_bpf__destroy(skel);
    printf("Scheduler Unloaded.\n");
    return -err;
}