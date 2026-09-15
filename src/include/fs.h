#ifndef __FS_H
#define __FS_H

#include <stdint.h>

/* =========================================================
 * Filesystem constants
 * ========================================================= */

#define BLOCK_SIZE 512

#define MAGIC_NUMBER 0x67676767

#define MAX_FILENAME_LEN 13

#define N_DIRECT_POINTERS 8
#define N_INDIRECT_POINTERS 1
#define N_DOUBLE_INDIRECT_POINTERS 1

#define MAX_FD_ENTRIES 16


/* =========================================================
 * Filesystem types
 * ========================================================= */

typedef enum file_type {
    FILE_TYPE_UNUSED = 0,
    FILE_TYPE_FILE,
    FILE_TYPE_DIRECTORY
} file_type_t;


/* =========================================================
 * Directory entry
 * ========================================================= */

typedef struct dir_entry {
    char filename[MAX_FILENAME_LEN + 1];
    uint16_t inode_index;
} dir_entry_t;

#define ENTRIES_PER_DIR_BLOCK \
    (BLOCK_SIZE / sizeof(dir_entry_t))


/* =========================================================
 * Inode block mapping
 * ========================================================= */

typedef struct location {
    uint32_t direct_pointers[N_DIRECT_POINTERS];
    uint32_t indirect_pointer;
    uint32_t double_indirect_pointer;
} location_t;


/* =========================================================
 * Inode
 * ========================================================= */

typedef struct inode {
    file_type_t type;

    location_t location;

    uint32_t size;

    /* reserved / future metadata */
    uint32_t reserved[4];

} inode_t;

#define INODE_SIZE sizeof(inode_t)

#define INODES_PER_BLOCK \
    (BLOCK_SIZE / sizeof(inode_t))

#define POINTERS_PER_BLOCK \
    (BLOCK_SIZE / sizeof(uint32_t))


/* =========================================================
 * Superblock
 * ========================================================= */

typedef struct superblock {

    uint32_t magic_number;

    /* Disk geometry */
    uint32_t total_blocks;

    /* Inode table */
    uint32_t inode_start;
    uint32_t inode_blocks;
    uint32_t inode_count;
    uint32_t free_inodes;

    /* Block bitmap */
    uint32_t bitmap_start;
    uint32_t bitmap_blocks;

    /* Data region */
    uint32_t data_start;
    uint32_t data_blocks;
    uint32_t free_blocks;

    /* Root directory */
    uint32_t root_inode;

    /* Pad superblock to one filesystem block */
    uint8_t padding[
        BLOCK_SIZE - (12 * sizeof(uint32_t))
    ];

} superblock_t;


/* =========================================================
 * File descriptors
 * ========================================================= */

typedef struct fd_table_entry {

    uint8_t status_flags;

    uint32_t inode_num;

    uint32_t file_offset;

} fd_table_entry_t;


typedef struct fd_table {

    fd_table_entry_t entries[MAX_FD_ENTRIES];

} fd_table_t;


/* =========================================================
 * Filesystem lifecycle
 * ========================================================= */

int fs_init(int drive_num);

int fs_format(int drive_num);

int fs_flush(void);


/* =========================================================
 * File operations
 * ========================================================= */

int fs_create(const char *path);

int fs_open(const char *path);

int fs_close(int fd);

int fs_read(
    int fd,
    char *buf,
    uint32_t n_bytes
);

int fs_write(
    int fd,
    const char *buf,
    uint32_t n_bytes
);

int fs_lseek(
    int fd,
    uint32_t offset
);


/* =========================================================
 * Directory operations
 * ========================================================= */

int fs_mkdir(const char *path);

int fs_readdir(
    int fd,
    char *buf
);


/* =========================================================
 * Removal
 * ========================================================= */

int fs_unlink(const char *path);


#endif