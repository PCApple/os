#ifndef __FS_H
#define __FS_H

#include "types.h"
#include "mem.h"
#include "print.h"
#include "scheduler.h"
#include "types.h"
#include "io.h"
#include "utils.h"

#define MAX_NUM_THREADS 16
#define TEST_THREAD_NUM 0 // for testing purposes in userspace, we only use one thread
#define MAX_FD_ENTRIES 16 // max number of open file descriptors per thread

#define FS_SIZE (2048*1024) // 2MB filesystem size
#define BLOCK_SIZE 256
#define INODE_SIZE 64
#define INODE_BLOCKS 256
#define ENTRIES_PER_DIR_BLOCK ((BLOCK_SIZE) / sizeof(dir_entry_t)) // 16 entries per directory block
#define BLOCK_BITMAP_BLOCKS 4
#define DATA_BLOCKS ((FS_SIZE / BLOCK_SIZE) - 1 - INODE_BLOCKS - BLOCK_BITMAP_BLOCKS) // number of data blocks - excluding superblock, inode blocks, and block bitmap blocks
#define BITMAP_SIZE (DATA_BLOCKS / 8) // in bytes
#define MAX_INODES ((BLOCK_SIZE * INODE_BLOCKS) / INODE_SIZE) // 1024 inodes
#define DIRECT_BLOCKS_PER_INDIRECT_BLOCK (BLOCK_SIZE / sizeof(void*)) // 64 pointers per indirect block

#define N_DIRECT_POINTERS 8
#define N_INDIRECT_POINTERS 1
#define N_DOUBLE_INDIRECT_POINTERS 1

#define MAX_FILENAME_LEN 13


typedef enum file_type {
    FILE_TYPE_UNUSED,
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY
}file_type_t;
typedef struct dir_entry {
    char filename[MAX_FILENAME_LEN+1]; // +1 for null terminator
    uint16_t inode_index;
}dir_entry_t;

typedef struct location {
    void* direct_pointers[N_DIRECT_POINTERS];
    void* indirect_pointer[N_INDIRECT_POINTERS];
    void* double_indirect_pointer[N_DOUBLE_INDIRECT_POINTERS];
}location_t;


typedef struct superblock {
    uint32_t n_free_blocks; // needs to be initialized at fs_init to DATA_BLOCKS
    uint32_t n_free_inodes; // needs to be initialized at fs_init to MAX_INODES
    void* inodes_start; // pointer to start of inode blocks
    void* block_bitmap_start; // pointer to start of block bitmap
    void* data_blocks_start; // pointer to start of data blocks
    uint32_t root_inode_index; // index of root inode (should be 0)
} superblock_t;

typedef struct block_bitmap {
    uint8_t bitmap[BITMAP_SIZE]; // 1 bit per data block, 0 is free, 1 is used
}block_bitmap_t;

typedef struct inode {
    file_type_t type;
    location_t location;
    uint32_t size;
    uint32_t idx; // not used, for debugging
    uint32_t num_accessed;
    uint32_t res3; // reserved, unsued so we can align to 64 bytes
    uint32_t res4;
}inode_t;

typedef struct fd_table_entry {
    uint8_t status_flags; // bit 0: present, bit 1: read, bit 2: write
    inode_t* inode;
    uint32_t file_offset;
}fd_table_entry_t;

typedef struct fd_table {
    fd_table_entry_t entries[MAX_FD_ENTRIES];
} fd_table_t;
// global pointer to the start of the filesystem in memory


int fs_init(void* fs_start, uint32_t fs_size);
int fs_create(const char* path);
int fs_mkdir(const char* path);
int fs_open(const char* path);
int fs_read(int fd, char* buf, uint32_t n_bytes);
int fs_write(int fd, char* buf, uint32_t n_bytes);
int fs_close(int fd);
int fs_lseek(int fd, uint32_t offset);
int fs_unlink(const char* path);
int fs_readdir(int fd, char* buf);
#endif