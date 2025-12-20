#include <linux/module.h>
#include <linux/export-internal.h>
#include <linux/compiler.h>

MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.arch = MODULE_ARCH_INIT,
};

KSYMTAB_FUNC(scx_bpf_task_cpu_by_pid, "_gpl", "");

SYMBOL_CRC(scx_bpf_task_cpu_by_pid, 0xc0becfae, "_gpl");

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0x821f7d3e, "find_get_pid" },
	{ 0x38c2e650, "pid_task" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x474e54d2, "module_layout" },
};

MODULE_INFO(depends, "");


MODULE_INFO(srcversion, "0F30ED48946E7C58803090F");
