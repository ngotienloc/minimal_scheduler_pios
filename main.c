// main.c
#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/resource.h>
#include <bpf/libbpf.h>
#include "mlfq.skel.h" // File này sẽ được Makefile tự tạo ra

static volatile bool exiting = false;

// Xử lý khi bấm Ctrl+C
static void sig_handler(int sig)
{
    printf("\nStopping MLFQ Scheduler...\n");
    exiting = true;
}

// Tăng giới hạn bộ nhớ khóa (cần thiết cho BPF)
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
    // Chỉ in lỗi hoặc cảnh báo, bỏ qua debug log rác
    if (level > LIBBPF_INFO)
        return 0;
    return vfprintf(stderr, format, args);
}

int main(int argc, char **argv)
{
    struct mlfq_bpf *skel;
    int err;

    /* 1. Thiết lập môi trường */
    libbpf_set_print(libbpf_print_fn);
    bump_memlock_rlimit();
    signal(SIGINT, sig_handler);
    signal(SIGTERM, sig_handler);

    /* 2. Mở BPF Skeleton */
    skel = mlfq_bpf__open();
    if (!skel) {
        fprintf(stderr, "Failed to open BPF skeleton\n");
        return 1;
    }

    /* 3. Load vào Kernel (Verifier kiểm tra tại đây) */
    err = mlfq_bpf__load(skel);
    if (err) {
        fprintf(stderr, "Failed to load BPF skeleton: %d\n", err);
        goto cleanup;
    }

    /* 4. Attach (Kích hoạt Scheduler) */
    /* Lưu ý: struct_ops tự động attach khi gọi hàm này */
    err = mlfq_bpf__attach(skel);
    if (err) {
        fprintf(stderr, "Failed to attach BPF skeleton: %d\n", err);
        goto cleanup;
    }

    printf("MLFQ Scheduler LOADED on Raspberry Pi 4!\n");
    printf("Running... Press Ctrl+C to unload.\n");
    printf("   (Check logs: sudo cat /sys/kernel/debug/tracing/trace_pipe)\n");

    /* 5. Vòng lặp chính */
    while (!exiting) {
        // Giữ chương trình sống để scheduler hoạt động
        sleep(1);
    }

cleanup:
    /* 6. Dọn dẹp và gỡ bỏ khỏi Kernel */
    mlfq_bpf__destroy(skel);
    printf("Scheduler Unloaded. Back to default.\n");
    return -err;
}