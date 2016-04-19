#include "fs.h"
#include "string.h"
#include "kmem_cache.h"
#include "stdio.h"

/*
 * FILES DESC
 */
static struct file_desc_t files_desc[MAX_DESC_COUNT];
static struct list_head free_desc;
static struct spinlock desc_lock;

/*
 * ROOT DIRECTORY
 */
static struct fs_node_t root;

static void file_resize(struct file_t* file, const uint64_t size) {
	void* new_data = kmem_alloc(size);
	memcpy(new_data, (void*) file->data, file->size);
	kmem_free((void*) file->data);
	file->data = (uint64_t) new_data;
	file->size = size;
}

static void file_clear(struct file_t* file) {
	kmem_free((void*) file->data);
	file->data = 0;
	file->size = 0;
}

static void files_desc_init() {
	spinlock_init(&desc_lock);

	list_init(&free_desc);
	for (size_t i = 0; i < MAX_DESC_COUNT; ++i) {
		list_init(&files_desc[i].node);	
		list_add_tail(&files_desc[i].node, &free_desc);
	}
}

static struct file_desc_t* get_file_desc() {
	struct file_desc_t* desc = 0;
	spin_lock(&desc_lock);
	if (!list_empty(&free_desc)) {
		desc = LIST_ENTRY(list_first(&free_desc), struct file_desc_t, node);
	}
	spin_unlock(&desc_lock);
	return desc;
}

static bool is_dir(struct fs_node_t* fs_node) {
	return fs_node->file.type == DIR;
}

static void file_init(struct file_t* file, enum file_type type) {
	spinlock_init(&file->lock);
	file->type = type;
	file->size = 0;
	file->data = 0;
}

static void fs_node_init(struct fs_node_t* fs_node, const char* name, enum file_type type) {	
	strcpy(fs_node->name, name);
	list_init(&fs_node->node);
	list_init(&fs_node->entries);
	file_init(&fs_node->file, type);
}

static struct fs_node_t* fs_node_find(struct fs_node_t* dir, const char* name) {
	struct list_head* node = list_first(&dir->entries);
	for (; node != &dir->entries; node = node->next) {
		struct fs_node_t* fs_node = LIST_ENTRY(node, struct fs_node_t, node);
		if (!strcmp(fs_node->name, name)) {
			return fs_node;
		}
	}
	return 0;
}

static struct fs_node_t* fs_node_get(struct fs_node_t* dir, const char* path, enum file_type type, bool create) {
	if (!strcmp(path, "/")) {
		return dir->file.type == type ? dir : 0;
	}

	char file_name[MAX_FILE_NAME_LENGTH] = {0};

	read_until(file_name, path + 1, '/');
	size_t file_name_len = strlen(file_name);

	bool end = (path[file_name_len + 1] == 0);

	spin_lock(&dir->file.lock);
	struct fs_node_t* next_dir = fs_node_find(dir, file_name);
	struct fs_node_t* fs_node = 0;

	if (end) {
		if (next_dir) {
			fs_node = (next_dir->file.type == type ? next_dir : 0);
		} else {
			if (create) {
				fs_node = (struct fs_node_t*) kmem_alloc(sizeof(struct fs_node_t));
				fs_node_init(fs_node, file_name, type);
				list_add_tail(&fs_node->node, &dir->entries);
			}
		}
	}
	spin_unlock(&dir->file.lock);

	if (end) {
		return fs_node;
	}
	
	return (next_dir && is_dir(next_dir)) ? fs_node_get(next_dir, path + file_name_len + 2, type, create) : 0;
}

void fs_init() {
	files_desc_init();
	fs_node_init(&root, "", DIR);
}


bool mkdir(const char* path) {
	struct fs_node_t* dir = fs_node_get(&root, path, DIR, false);
	if (dir) {
		printf("Directory already exists!\n");
		return false;
	}
	return fs_node_get(&root, path, DIR, true);
}

void readdir(const char* path) {
	struct fs_node_t* dir = fs_node_get(&root, path, DIR, false);
	if (!dir) {
		printf("Directory doesnt exist!\n");
		return;
	}

	spin_lock(&dir->file.lock);

	printf("%s: files list:\n", path);
	for (struct list_head* node = list_first(&dir->entries); node != &dir->entries; node = node->next) {
		struct fs_node_t* fs_node = LIST_ENTRY(node, struct fs_node_t, node);
		printf("%s (%s)\n", fs_node->name, fs_node->file.type == DIR ? "DIR" : "FILE");
	}

	spin_unlock(&dir->file.lock);
}

struct file_desc_t* open(const char* path, int mode)
{
	struct file_desc_t* desc = get_file_desc();
	if (!desc) {
		printf("No free file descriptors!\n");
		return 0;
	}

	struct fs_node_t* fs_node = fs_node_get(&root, path, FILE, true);
	if (!fs_node) {
		close(desc);
		printf("Unable to open file (%s)!\n", path);
		return 0;
	}

	desc->file 	= &fs_node->file;
	desc->pos	= (mode == APPEND ? desc->file->size : 0);

	if (mode == WRITE) {
		file_clear(desc->file);
	}

	return desc;
}

void close(struct file_desc_t* desc) {
	if (!desc) {
		return;
	}
	spin_lock(&desc_lock);
	list_add_tail(&desc->node, &free_desc);
	spin_unlock(&desc_lock);
}

uint64_t read(struct file_desc_t* desc, char* buff, uint64_t size) {
	spin_lock(&desc->file->lock);
	uint64_t count = 0;
	
	if (desc->pos < desc->file->size) {
		count = MIN_UINT64(size, desc->file->size - desc->pos);
	}

	memcpy(buff, (void*) (desc->file->data + desc->pos), size);
	desc->pos += count;

	spin_unlock(&desc->file->lock);
	return count;
}

uint64_t write(struct file_desc_t* desc, const char* buff, uint64_t size) {
	spin_lock(&desc->file->lock);	
	if (desc->pos + size > desc->file->size) {
		file_resize(desc->file, desc->pos + size);
	}
	memcpy((void*) (desc->file->data + desc->pos), buff, size);
	desc->pos += size;
	spin_unlock(&desc->file->lock);
	return size;
}
