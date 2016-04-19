#ifndef __FS_H__
#define __FS_H__

#include "list.h"
#include "locking.h"

#define MAX_FILE_NAME_LENGTH 	256
#define MAX_DESC_COUNT 			1024

#define MIN_UINT64(a, b) a < b ? a : b

enum file_type {
	FILE,
	DIR
};

enum file_mode { 
	READ	= 1 << 0,
	WRITE 	= 1 << 1,
	APPEND 	= 1 << 2
};

struct file_t {
	struct spinlock lock;
	enum file_type type;
	uint64_t size;
	uint64_t data;
};

struct file_desc_t {
	struct file_t* file;
	uint64_t pos;

	struct list_head node;
};

struct fs_node_t {
	char name[MAX_FILE_NAME_LENGTH];
	struct list_head node;	
	struct list_head entries;

	struct file_t file;
};

void fs_init();

struct file_desc_t* open(const char* path, int mode);

void close(struct file_desc_t* desc);

uint64_t read(struct file_desc_t* desc, char* buff, uint64_t size);

uint64_t write(struct file_desc_t* desc, const char* buff, uint64_t size);

bool mkdir(const char* path);

void readdir(const char* path);

#endif /* __FS_H__ */