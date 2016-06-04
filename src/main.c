#include "kmem_cache.h"
#include "interrupt.h"
#include "initramfs.h"
#include "threads.h"
#include "memory.h"
#include "serial.h"
#include "paging.h"
#include "stdio.h"
#include "ramfs.h"
#include "misc.h"
#include "time.h"
#include "vfs.h"
#include "elf.h"

static void syscall(uint64_t syscall, uint64_t arg1, uint64_t arg2)
{
    __asm__ ("movq %0, %%rax\n\r"
             "movq %1, %%rbx\n\r"
             "movq %2, %%rcx\n\r"
             "int $48\n\r"
             :
             : "m"(syscall), "m"(arg1), "m"(arg2)
             : "rax", "rbx", "rcx", "memory");
}

static void test_syscalls()
{
    syscall(0, (uint64_t)"Test ", sizeof("Test ") - 1);
    syscall(0, (uint64_t)"message.\n", sizeof("message.\n") - 1);
}

static int start_kernel(void *dummy)
{
	(void) dummy;

	setup_ramfs();
	setup_initramfs();

    test_syscalls();
    wait(run_elf());

	return 0;
}

void main(void)
{
	setup_serial();
	setup_misc();
	setup_ints();
	setup_memory();
	setup_buddy();
	setup_paging();
	setup_alloc();
	setup_time();
	setup_threading();
	setup_vfs();

	create_kthread(&start_kernel, 0);
	local_preempt_enable();
	idle();

	while (1);
}