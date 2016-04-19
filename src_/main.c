#include "kmem_cache.h"
#include "interrupt.h"
#include "threads.h"
#include "memory.h"
#include "serial.h"
#include "paging.h"
#include "stdio.h"
#include "string.h"
#include "misc.h"
#include "time.h"
#include "fs.h"
#include "balloc.h"
#include "initramfs.h"

static void test_directories() {
	bool test_passed = true;

	test_passed &= (mkdir("fs/dir1") == false);
	test_passed &= (mkdir("fs/dir1/dir3") == false);
	test_passed &= mkdir("dir4");
	test_passed &= mkdir("dir4/dir6");
	
	printf("test_directories: %s.\n", test_passed ? "PASSED" : "FAILED");
}

static void test_files() {
	bool test_passed  = true;

	struct file_desc_t* file = open("fs/dir1/dir3/doc1.txt", WRITE);
	const char* text = "Hello, World!";
	write(file, text, strlen(text));
	close(file);
	
	file = open("fs/dir1/dir2/doc2.txt", READ);
	char buff[100];
	int count = read(file, buff, 100);
	test_passed  &= !strcmp(buff, text);
	test_passed  &= (count == 13);
	close(file);

	file = open("fs/dir1/dir3/doc1.txt", READ);
	test_passed  &= (file == 0);
	close(file);

	readdir("");
	readdir("fs/dir1");

	printf("test_files: %s.\n", test_passed  ? "PASSED" : "FAILED");
}

static int start_kernel(void *dummy)
{
	(void) dummy;
	test_directories();
	test_files();

	return 0;
}

//to run enter: qemu-system-x86_64 -kernel kernel -nographic -initrd init_fs

void main(void)
{
	setup_serial();
	setup_misc();
	setup_ints();
	setup_memory();
	setup_initramfs();

	setup_buddy();
	setup_paging();
	setup_alloc();
	setup_time();
	setup_threading();
	fs_init();
	read_initramfs();

	local_irq_enable();

	create_kthread(&start_kernel, 0);
	local_preempt_enable();
	idle();

	while (1);
}
