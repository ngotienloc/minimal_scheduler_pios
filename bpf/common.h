#ifndef __MLFQ_COMMON_H__
#define __MLFQ_COMMON_H__

/* * Định nghĩa các hằng số cho bộ lập lịch MLFQ 
 */
#define MLFQ_LEVELS          3      // Số lượng mức ưu tiên (ví dụ: 0, 1, 2)
#define MLFQ_DEFAULT_SLICE   10000  // Thời gian chạy mặc định (microseconds)

/* * Cấu trúc dữ liệu dùng chung giữa BPF và User-space 
 * Cấu trúc này mô tả trạng thái của mỗi Task (tiến trình)
 */
struct task_ctx {
    unsigned int priority;      // Mức ưu tiên hiện tại
    unsigned long slice_ns;     // Thời gian còn lại trong lượt
    unsigned long vruntime;     // Thời gian ảo đã chạy
};

/* * Các thông số thống kê để User-space đọc và hiển thị
 */
struct mlfq_stats {
    unsigned long nr_enqueued;
    unsigned long nr_dispatched;
    unsigned long nr_expired;
};

#endif /* __MLFQ_COMMON_H__ */
