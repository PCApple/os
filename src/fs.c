#include "include/fs.h"
#include "include/strings.h"
#include "include/block_cache.h"
#include "include/disk.h"

static uint8_t initialized = 0;

// Helper functions for filesystem operations
int bitmap_set(uint32_t block_num, uint32_t block_bitmap_start, int set)
{
    if (set < 0 || set > 1) {
        return -1;
    }
    uint32_t bits_per_block = BLOCK_SIZE * 8;

    uint32_t bitmap_block =
        block_num / bits_per_block;

    uint32_t bit =
        block_num % bits_per_block;

    cache_block_t *block =
        cache_get(block_bitmap_start + bitmap_block);

    if (!block) {
        return -1;
    }

    uint8_t *bitmap = (uint8_t *)block->data;

    if (set) {
        bitmap[bit / 8] |= (1 << (bit % 8));
    } else {
        bitmap[bit / 8] &= ~(1 << (bit % 8));
    }

    cache_mark_dirty(block);
    cache_release(block);

    return 0;
}
dir_entry_t* fs_lookup_dir_entry(const char *path) {
    return NULL; // placeholder implementation

}
inode_t *fs_lookup_parent(const char *path) {
    inode_t *parent_inode = NULL;
    cache_block_t *sb_block = cache_get(0);
    if (!sb_block) {
        return NULL;
    }
    superblock_t *sb = (superblock_t *)sb_block->data;
    uint32_t inode_start = sb->inode_start;
    uint32_t root_inode = sb->root_inode;
    cache_release(sb_block);
    uint32_t path_len = strlen(path);
    if (path_len == 0) {
        return NULL; // empty path
    }
    if (path[path_len - 1] == '/') {
        return NULL; // path ends with a slash, invalid for file creation
    }
    uint32_t path_idx = 1;
    cache_block_t *current_inode_block = cache_get(inode_start);
    if (!current_inode_block) {
        return NULL;
    }
    inode_t *inodes = (inode_t *)(current_inode_block->data);
    inode_t* current_inode = &inodes[root_inode];
    char name[MAX_FILENAME_LEN+1];
    while (path_idx < path_len) {
        if (current_inode->type != FILE_TYPE_DIRECTORY) {
            cache_release(current_inode_block);
            return NULL;
        }
        uint32_t name_idx = 0;
        while (path_idx < path_len && path[path_idx] != '/') {
            name[name_idx++] = path[path_idx++];
        }
        name[name_idx] = '\0'; // null-terminate the filename
        dir_entry_t *entry = fs_lookup_dir_entry(name);
        if (!entry) {
            cache_release(current_inode_block);
            return NULL;
        }
        current_inode = &inodes[entry->inode_index];
        parent_inode = current_inode;
        cache_release(current_inode_block);
        current_inode_block = cache_get(inode_start + entry->inode_index / INODES_PER_BLOCK);
        if (!current_inode_block) {
            return NULL;
        }
        inodes = (inode_t *)(current_inode_block->data);
    }
    cache_release(current_inode_block);
    if (!parent_inode) {
        return NULL;
    }
    // return the parent inode of the last component in the path
    if (current_inode == parent_inode) {
        return NULL; // the last component is the same as the parent, meaning no parent exists
    }
    return parent_inode;
}
//Filesystem lifecycle


 /* Initialize the filesystem.
 * @param drive_num The number of the drive to initialize. should be 1 by default.
 * @return 0 on success, -1 on failure.
 */
    int fs_init(int drive_num) {
        if (initialized) {
            return 0; // already initialized
        }
        if (drive_num > 3) {
            return -1; // invalid drive number
        }
        if (drive_num < 1) {
            return -1; // invalid drive number
        }

        cache_init(drive_num);
        cache_block_t *superblock = cache_get(0); // load the superblock into the cache
        if (!superblock) {
            
            return -1;
        }
        superblock_t *sb = (superblock_t *)superblock->data;
        if (sb->magic_number != MAGIC_NUMBER) { // if superblock is not valid, format the filesystem
            cache_release(superblock);
            int err = fs_format(drive_num);
            if (err != DISK_OK) {
                return err;
            }
            superblock = cache_get(0); // reload the superblock after formatting
            if (!superblock) {
                return -1;
            }
            sb = (superblock_t *)superblock->data;
        }
        cache_release(superblock); // this should keep the superblock in cache.
        initialized = 1;    
        return 0;
    }

int fs_format(int drive_num) {

    if (drive_num < 1 || drive_num > 3) {
        return -1; // invalid drive number
    }
    disk_info_t info;
    if (disk_get_info(drive_num, &info) != DISK_OK) {
        return -1;
    }
    uint64_t total_bytes = ((uint64_t)info.sector_count * info.sector_size); // sector size should be 512 bytes, so the next line shouldn't matter, but just to be safe
    uint64_t total_blocks = total_bytes / BLOCK_SIZE;
    uint32_t inode_count = total_blocks / BLOCKS_PER_INODE;
    uint32_t block_bitmap_blocks = DIV_ROUND_UP(total_blocks, BLOCK_SIZE * 8);
    uint32_t inode_bitmap_blocks = DIV_ROUND_UP(inode_count, BLOCK_SIZE * 8);
    uint32_t inode_table_blocks = DIV_ROUND_UP(inode_count, INODES_PER_BLOCK);
    if (total_blocks == 0 || inode_count == 0) {
        return -1;
    }
    if (total_blocks > UINT32_MAX) {
        return -1;
    }


    // Initialize the superblock
    superblock_t sb = {0};
    sb.magic_number = MAGIC_NUMBER;
    sb.total_blocks = total_blocks;
    sb.inode_start = block_bitmap_blocks + inode_bitmap_blocks + 1;
    sb.inode_blocks = inode_table_blocks;
    sb.inode_count = inode_count;
    sb.free_inodes = inode_count-1;
    sb.block_bitmap_start = 1;
    sb.inode_bitmap_start = 1 + block_bitmap_blocks;
    sb.block_bitmap_blocks = block_bitmap_blocks;
    sb.inode_bitmap_blocks = inode_bitmap_blocks;
    sb.data_start = sb.inode_start + sb.inode_blocks;
    if (sb.data_start >= total_blocks) {
        return -1;
    }
    sb.data_blocks = (uint32_t)total_blocks - sb.data_start;
    sb.free_blocks = sb.data_blocks-1;
    sb.root_inode = 0; // root inode is the first inode in the inode table
    
    
    // zero out block bitmaps and inode bitmaps and inode table
    for (uint32_t i = 0; i < block_bitmap_blocks + inode_bitmap_blocks; i++) {
        cache_block_t *block = cache_get(sb.block_bitmap_start + i);
        if (block) {
            memset(block->data, 0, BLOCK_SIZE);
            cache_mark_dirty(block);
            cache_release(block);
        }
        else {
            // Failed to get the block from cache, this could indicate an error
            return -1;
        }
    }
    // now mark the reserved blocks in the block bitmap as used
    for (uint32_t i = 0; i <= sb.data_start; i++) {
        if (bitmap_set(i, sb.block_bitmap_start, 1) != 0) {
            return -1;
        }
    }
    // now mark the non existant blocks in the padding as used as well
    uint32_t padding_blocks = block_bitmap_blocks * BLOCK_SIZE*8 - total_blocks;
    for (uint32_t i = total_blocks; i < total_blocks + padding_blocks; i++) {
        if (bitmap_set(i, sb.block_bitmap_start, 1) != 0) {
            return -1;
        }
    }
    // do the same for the inode bitmap, marking all non-existent inodes as used
    uint32_t padding_inodes = inode_bitmap_blocks * BLOCK_SIZE*8 - inode_count;
    for (uint32_t i = inode_count; i < inode_count + padding_inodes; i++) {
        if (bitmap_set(i, sb.inode_bitmap_start, 1) != 0) {
            return -1;
        }
    }
    // now setup the root inode
    if (bitmap_set(0, sb.inode_bitmap_start, 1) != 0) {
        return -1;
    }
    // now setup the root inode structure

    // clear out inodes first
    for (uint32_t i = 0; i < sb.inode_blocks; i++) {
        cache_block_t *inode_block = cache_get(sb.inode_start + i);
        if (inode_block) {
            memset(inode_block->data, 0, BLOCK_SIZE);
            cache_mark_dirty(inode_block);
            cache_release(inode_block);
        } else {
            return -1;
        }
    }
    cache_block_t *root_inode_block = cache_get(sb.inode_start);
    if (!root_inode_block) {
        return -1;
    }
    // setup the root inode structure
    inode_t *root_inode = (inode_t *)root_inode_block->data;
    root_inode->type = FILE_TYPE_DIRECTORY;
    root_inode->location.direct_pointers[0] = sb.data_start;
    root_inode->size = 2 * sizeof(dir_entry_t); // for "." and ".." entries
    cache_mark_dirty(root_inode_block);
    cache_release(root_inode_block);
    // now to add the "." and ".." entries to the root directory
    cache_block_t *root_dir_block = cache_get(sb.data_start);
    if (!root_dir_block) {
        return -1;
    }
    dir_entry_t *root_dir = (dir_entry_t *)root_dir_block->data;
    memset(root_dir, 0, BLOCK_SIZE);
    // add "." entry
    root_dir[0].filename[0] = '.';
    root_dir[0].filename[1] = '\0';
    root_dir[0].inode_index = 0; // root inode itself
    // add ".." entry
    root_dir[1].filename[0] = '.';
    root_dir[1].filename[1] = '.';
    root_dir[1].filename[2] = '\0';
    root_dir[1].inode_index = 0; // root inode itself
    cache_mark_dirty(root_dir_block);
    cache_release(root_dir_block);
    cache_flush_all();
    // Write the superblock to the disk
    cache_block_t *superblock = cache_get(0);
    if (!superblock) {
        return -1;
    }
    memset(superblock->data, 0, BLOCK_SIZE);
    memcpy(superblock->data, &sb, sizeof(superblock_t));
    cache_mark_dirty(superblock);
    cache_release(superblock);
    cache_flush_all(); // want to flush superblock last
    return 0;
}

int fs_flush(void) {
    cache_flush_all();
    return 0;
}


/* =========================================================
 * File operations
 * ========================================================= */

int fs_create(const char *path) {
    if (!path || path[0] != '/') {
        return -1; // invalid path
    }
    uint32_t path_len = strlen(path);
    if (path_len == 0) {
        return -1; // empty path
    }
    if (path[path_len - 1] == '/') {
        return -1; // path ends with a slash, invalid for file creation
    }
    inode_t *parent_inode = fs_lookup_parent(path);
}

int fs_open(const char *path) {
    return 0;

}

int fs_close(int fd) {
    return 0;

}

int fs_read(
    int fd,
    char *buf,
    uint32_t n_bytes
) {
    return 0;

}

int fs_write(
    int fd,
    const char *buf,
    uint32_t n_bytes
) {
    return 0;

}

int fs_lseek(
    int fd,
    uint32_t offset
) {
    return 0;

}


/* =========================================================
 * Directory operations
 * ========================================================= */

int fs_mkdir(const char *path) {
    return 0;

}

int fs_readdir(
    int fd,
    char *buf
) {
    return 0;
}


/* =========================================================
 * Removal
 * ========================================================= */

int fs_unlink(const char *path) {
    return 0;
}