1. 
void BPF_STRUCT_OPS(mlfq_dispatch, s32 cpu, struct task_struct *prev){
     for(int lvl = 0; lvl < NUM_DSQ; lvl++){
        if(scx_bpf_dsq_move_to_local(lvl))
            return; 
     }
}
Nhiệm vụ: Chạy điều phối hàng đợi MLFQ trong môi trường eBPF sử dụng MARCO BPF_STRUCT_OPS.
Mục tiêu: Đảm bảo CPU rảnh rỗi luôn tìm được tác vụ có mức ưu tiên cao nhất để chạy. 
Cơ chế hoạt động: 
- Kích hoạt callback dispatch:
    Gọi mlgq_dispatch.
- Duyệt qua các hàng đợi ưu tiên:
    for(int lvl = 0; lvl < NUM_DSQ; lvl++).
- Di chuyển các tác vụ: 
    if (scx_bpf_dsq_move_to_local(lvl))
    return;
        Nó cố gắng lấy tác vụ đầu tiên từ Hàng đợi Điều phối tùy chỉnh có ID là lvl.
        Nó di chuyển tác vụ đó đến Hàng đợi Điều phối Cục bộ (SCX_DSQ_LOCAL) của CPU đang gọi (tức là CPU cpu).
        Hàm này trả về true nếu việc di chuyển thành công (tức là tìm thấy và di chuyển được ít nhất một tác vụ).
Giải thích chi tiết: 
+ Khi một tác vụ trong hàng đợi thứ lvl tồn tại, scx_bpf_dsq_move_to_local sẽ chuyển tác vụ 
đầu tiên trên queue mức đó vào hàng đợi cục bộ của CPU (Chưa chọn hàng đợi cục bộ của CPU).
+ Nếu không có tác vụ nào trong queue thì return thoát ra hàm. 

2. 
void BPF_STRUCT_OPS(mlfq_running, struct task_struct *p)
{
    u32 pid = p->pid;
    u64 *slice = bpf_map_lookup_elem(&task_slice, &pid);
    if (slice && *slice > 0) {
        *slice -= SLICE_NS[DSQ_HIGH]; // approximation
        if (*slice == 0) {
            u32 *level = bpf_map_lookup_elem(&task_queue, &pid);
            if (level && *level < DSQ_LOWEST) {
                (*level)++;
                u64 new_slice = SLICE_NS[*level];
                bpf_map_update_elem(&task_slice, &pid, &new_slice, BPF_ANY);
                bpf_map_update_elem(&task_queue, &pid, level, BPF_ANY);
            }
        }
    }
}
Nhiệm vụ: Giám sát việc sử dụng CPU time slice của một tác vụ đang chạy và hạ cấp 
nó xuống hàng đợi ưu tiên thấp hơn khi nó sử dụng hết thời gian được cấp. 
- Lấy id của tiến trình đang chạy
    u32 pid = p->pid
- Tìm kiếm BPF map "task_slice" lưu trữ thông tin thời gian sử dụng còn lại của mỗi tiến trình. 
    bpf_map_lookup_elem(&task_slice, &pid)
    