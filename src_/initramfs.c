#include "initramfs.h"
#include "stdio.h"
#include "string.h"
#include "memory.h"
#include "multiboot.h"
#include "fs.h"

static char* align4(char *addr) {
    return ((uint64_t) addr & 3) == 0 ? addr : addr + (4 - ((uint64_t) addr & 3));    
}

static uint32_t read_int(char *str) {
    uint32_t result = 0;
    for (int i = 0; i < 8; i++) {
        result <<= 4;
        if (str[i] >= '0' && str[i] <= '9')
            result += str[i] - '0';
        else if (str[i] >= 'A' && str[i] <= 'F')
            result += str[i] - 'A' + 10;
        else if (str[i] >= 'a' && str[i] <= 'f')
            result += str[i] - 'a' + 10;
        else
            printf("Incorrect integer in cpio header: %s\n", str);
    }
    return result;
}

static char *start, *end;

void setup_initramfs() {
	extern const uint32_t mboot_info;
 	multiboot_info_t *multiboot_info = (multiboot_info_t *) (uintptr_t) mboot_info;
 	
 	if (!(multiboot_info->flags & (1<<3))) {
        puts("No modules from multiboot!");
        return;
 	}

 	bool found = 0;
    
    multiboot_module_t *module = (multiboot_module_t*) (uintptr_t) multiboot_info->mods_addr;
 	for (uint32_t i = 0; i < multiboot_info->mods_count; i++, module++) {
 		if (module->mod_end - module->mod_start >= sizeof(cpio_header_t) && 
                !memcmp((void*) (uintptr_t) module->mod_start, CPIO_HEADER_MAGIC, 6)){
            found = 1;
 			break;
 		}
 	}

 	if(!found) {
        puts("Can't find cpio module");
        return;
 	}

    start = (char*) (uintptr_t) module->mod_start;
    end = (char*) (uintptr_t) module->mod_end;
 
	printf("reserve memory range: %llu-%llu for initramfs\n",
			(unsigned long long) start,
			(unsigned long long) end- 1);
    balloc_add_region((phys_t) start, end - start);
    balloc_reserve_region((phys_t) start, end - start);
}

void read_initramfs() {
    start = va((phys_t) start);
    end = va((phys_t) end);
    while (start < end) {
        start = align4(start);
        DBG_ASSERT(!memcmp(start, CPIO_HEADER_MAGIC, 6)); 
        cpio_header_t *header = (cpio_header_t*) start;
        uint32_t name_len = read_int(header->namesize);
        uint32_t file_len = read_int(header->filesize);
        start += sizeof(cpio_header_t);
        if (!memcmp(start, END_OF_ARCHIVE, strlen(END_OF_ARCHIVE))) {
            return;
        }
        
        char file_name[MAX_FILE_NAME_LENGTH];
        file_name[0] = '/';
        for (uint32_t i = 0; i < name_len; i++, start++) {
            file_name[i + 1] = *start;
        } 

        file_name[name_len] = 0;
        struct file_desc_t* file = NULL;
        if (S_ISDIR(read_int(header->mode))) {
            printf("make dir: %s\n", file_name);
            mkdir(file_name);
        } else {
            printf("open: %s\n", file_name);
            file = open(file_name, WRITE);
        }

        puts("");
        start = align4(start);
        if (file) {
            DBG_ASSERT(write(file, start, file_len) == file_len);
        }
        start += file_len;
    }
}