#ifndef __ELF_C__
#define __ELF_C__

#include "elf.h"
#include "vfs.h"
#include "stdio.h"
#include "paging.h"
#include "string.h"
#include "memory.h"
#include "threads.h"
#include "kmem_cache.h"
#include "thread_regs.h"

static void read(struct fs_file* file, void* buffer, const int offset, const int size) {
    vfs_seek(file, offset, FSS_SET);
    vfs_read(file, buffer, size);
}

static void read_header(struct elf_phdr phdr, struct fs_file file, phys_t pml4_paddr, pte_t* pml4) {
    const uint64_t p_vaddr_start = (phdr.p_vaddr / PAGE_SIZE) * PAGE_SIZE;
    const uint64_t p_vaddr_end = ((phdr.p_vaddr + phdr.p_memsz + PAGE_SIZE - 2) / PAGE_SIZE) * PAGE_SIZE;

    const int rc = pt_populate_range(pml4, p_vaddr_start, p_vaddr_end);
    DBG_ASSERT(rc == 0 && "pt_populate_range");

    struct pt_iter iter;
    for_each_slot_in_range(pml4, p_vaddr_start, p_vaddr_end, iter) {
        const int level = iter.level;
        const int index = iter.idx[level];
        pte_t *pt = iter.pt[level];

        DBG_ASSERT(level == 0 && "level != 0");

        struct page* page = alloc_pages(0);
        pt[index] = page_paddr(page) | PTE_PT_FLAGS | PTE_PRESENT;
    }

    store_pml4(pml4_paddr);

    void* buffer = kmem_alloc(phdr.p_filesz);
    read(&file, buffer, phdr.p_offset, phdr.p_filesz);
    memcpy((void*) phdr.p_vaddr, buffer, phdr.p_filesz);
    kmem_free(buffer);
}

void prepare_stack(struct elf_hdr hdr, phys_t pml4_paddr, pte_t* pml4) {
    struct thread_regs* regs = thread_regs(current());
    struct page* stack = alloc_pages(0);
    
    DBG_ASSERT(stack != 0 && "Not enough memory");

    struct pt_iter iter;
    struct pt_iter* slot = pt_iter_set(&(iter), pml4, 0);

    const int level = slot->level;
    const int index = slot->idx[level];
    pte_t *pt = slot->pt[level];

    phys_t p_stack_addr = page_paddr(stack);
    pt[index] = p_stack_addr | PTE_PT_FLAGS | PTE_PRESENT;

    store_pml4(pml4_paddr);

    regs->cs = USER_CODE;
    regs->ss = USER_DATA;
    regs->rsp = (uint64_t) ((char*) (slot->addr) + PAGE_SIZE);
    regs->rip = hdr.e_entry;
}

static int read_elf(void* extra_arg) {
    (void) extra_arg;

    struct fs_file file;
    struct elf_hdr hdr;
    struct elf_phdr phdr;

    vfs_open("/initramfs/test", &file);
    read(&file, &hdr, 0, sizeof(hdr));

    phys_t pml4_paddr = load_pml4();
    pte_t* pml4 = va(pml4_paddr);

    for (int i = 0; i < hdr.e_phnum; ++i) {
        read(&file, &phdr, sizeof(hdr) + i * hdr.e_phentsize, sizeof(phdr));
        if (phdr.p_type == PT_LOAD) {
            read_header(phdr, file, pml4_paddr, pml4);
        }
    }
    
    vfs_release(&file);

    prepare_stack(hdr, pml4_paddr, pml4);

    return 0;
}

pid_t run_elf() {
    return create_kthread(&read_elf, 0);
}

#endif