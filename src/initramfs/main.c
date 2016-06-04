#include <stdint.h>

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

void main(void) {
    syscall(0, (uint64_t)"Test elf message.\n", sizeof("Test elf message.\n") - 1);
    while(1);
}