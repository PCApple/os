#include "include/fs.h"
#include "include/strings.h"
#include "include/block_cache.h"
#include "include/disk.h"

/* One cache-backed filesystem is active at a time; repeated mounts are ignored. */
static uint8_t initialized = 0;

/* Set or clear one allocation bit relative to the supplied bitmap start.
 * A set bit means reserved/allocated; return -1 on invalid input or cache failure.
 * Dirty data is written later by the cache's flush or eviction path. */
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
int bitmap_read(uint32_t block_num, uint32_t block_bitmap_start)
{
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

    int value = (bitmap[bit / 8] & (1 << (bit % 8))) != 0;

    cache_release(block);

    return value;
}
/* Search one directory for an exact, case-sensitive filename.
 * Return the inode index (including valid index 0), or -1 for invalid arguments,
 * a missing entry, or a cache read failure. Zero block pointers are skipped.
 * Release all blocks acquired here and return a copied index, not cached data. */
int fs_lookup_dir_entry_inode_index(inode_t* dir_inode, const char* filename) {
    if (dir_inode == NULL || filename == NULL) {
        return -1;
    }
    if (filename[0] == '\0') {
        return -1; // empty filename
    }
    if (dir_inode->type != FILE_TYPE_DIRECTORY) {
        return -1; // not a directory
    }
    /* Direct pointers address blocks containing directory entries. */
    for(int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (dir_inode->location.direct_pointers[i] == 0) {
            continue; // no data blocks
        }
        cache_block_t *block = cache_get(dir_inode->location.direct_pointers[i]);
        if (!block) {
            return -1;
        }
        dir_entry_t* entries = (dir_entry_t*)block->data;
        for (int j = 0; j < ENTRIES_PER_DIR_BLOCK; j++) {
            if (strcmp(entries[j].filename, filename) == 0) {
                /* Copy before release: the cache owns the entry's storage. */
                int inode_index = entries[j].inode_index;
                cache_release(block);
                return inode_index;
            }
        }
        cache_release(block);
    }
    /* An indirect block contains data-block numbers, not directory entries. */
    for (int i = 0; i < N_INDIRECT_POINTERS; i++)
    {
        if (dir_inode->location.indirect_pointer[i] == 0) {
            continue; // no indirect blocks
        }
        cache_block_t *indirect_block = cache_get((uint32_t)dir_inode->location.indirect_pointer[i]);
        if (!indirect_block) {
            return -1;
        }
        uint32_t* indirect_block_data = (uint32_t*)indirect_block->data;
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (indirect_block_data[j] == 0) {
                continue; // no data blocks
            }
            cache_block_t *data_block = cache_get(indirect_block_data[j]);
            if (!data_block) {
                cache_release(indirect_block);
                return -1;
            }
            dir_entry_t* entries = (dir_entry_t*)data_block->data;
            for (int k = 0; k < ENTRIES_PER_DIR_BLOCK; k++) {
                if (strcmp(entries[k].filename, filename) == 0) {
                    int inode_index = entries[k].inode_index;
                    cache_release(data_block);
                    cache_release(indirect_block);
                    return inode_index;
                }
            }
            cache_release(data_block);
        }
        cache_release(indirect_block);
    }
    /* Double indirection: pointer block -> pointer block -> directory data.
     * Failure and success paths must release every acquired level. */
    for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
        if (dir_inode->location.double_indirect_pointer[i] == 0) {
            continue; // no double indirect blocks
        }
        cache_block_t *double_indirect_block = cache_get((uint32_t)dir_inode->location.double_indirect_pointer[i]);
        if (!double_indirect_block) {
            return -1;
        }
        uint32_t* double_indirect_block_data = (uint32_t*)double_indirect_block->data;
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (double_indirect_block_data[j] == 0) {
                continue; // no indirect blocks
            }
            cache_block_t *indirect_block = cache_get(double_indirect_block_data[j]);
            if (!indirect_block) {
                cache_release(double_indirect_block);
                return -1;
            }
            uint32_t* indirect_block_data = (uint32_t*)indirect_block->data;
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                if (indirect_block_data[k] == 0) {
                    continue; // no data blocks
                }
                cache_block_t *data_block = cache_get(indirect_block_data[k]);
                if (!data_block) {
                    cache_release(indirect_block);
                    cache_release(double_indirect_block);
                    return -1;
                }
                dir_entry_t* entries = (dir_entry_t*)data_block->data;
                for (int l = 0; l < ENTRIES_PER_DIR_BLOCK; l++)
                {
                    if (strcmp(entries[l].filename, filename) == 0) {
                        int inode_index = entries[l].inode_index;
                        cache_release(data_block);
                        cache_release(indirect_block);
                        cache_release(double_indirect_block);
                        return inode_index;
                    }
                }
                cache_release(data_block);
            }
            cache_release(indirect_block);
        }
        cache_release(double_indirect_block);
    }
    return -1; // not found
}
/* Resolve the containing directory of an absolute path, e.g. /a/b -> /a.
 * All components must be nonempty and fit MAX_FILENAME_LEN; the final name is
 * validated but need not exist. Root-only and trailing-slash paths are rejected.
 * On success, return 0 and transfer the current block acquisition to the caller:
 * use parent_handle->inode, then cache_release(parent_handle->block).
 * On failure, return -1, release acquired blocks, and leave the output untouched.
 * The cache currently does not pin acquired blocks against eviction. 
 * WARNING: The caller is responsible for managing the lifecycle of the returned block and inode.
 */
int fs_lookup_parent(const char *path, inode_handle_t *parent_handle) {
    if (!path || !parent_handle || path[0] != '/') {
        return -1;
    }
    cache_block_t *sb_block = cache_get(0);
    if (!sb_block) {
        return -1;
    }
    superblock_t *sb = (superblock_t *)sb_block->data;
    /* Keep scalar metadata locally before releasing the superblock. */
    uint32_t inode_start = sb->inode_start;
    uint32_t root_inode = sb->root_inode;
    uint32_t inode_count = sb->inode_count;
    cache_release(sb_block);
    uint32_t path_len = strlen(path);
    if (path_len == 0) {
        return -1; // empty path
    }
    
    if (path[path_len - 1] == '/') {
        return -1; // path ends with a slash, invalid for file creation
    }
    uint32_t path_idx = 1;
    /* Divide to choose the inode-table block; take the remainder for its slot. */
    cache_block_t *current_inode_block = cache_get(inode_start+root_inode / INODES_PER_BLOCK);
    if (!current_inode_block) {
        return -1;
    }
    inode_t *inodes = (inode_t *)(current_inode_block->data);
    inode_t* current_inode = &inodes[root_inode % INODES_PER_BLOCK];
    char name[MAX_FILENAME_LEN+1];
    while (path_idx < path_len) {
        if (current_inode->type != FILE_TYPE_DIRECTORY) {
            cache_release(current_inode_block);
            return -1;
        }
        uint32_t name_idx = 0;
        /* Leave room for the terminator, including when validating the leaf. */
        while (path_idx < path_len && path[path_idx] != '/') {
            if (name_idx >= MAX_FILENAME_LEN) {     
                cache_release(current_inode_block);
                return -1; // filename too long
            }
            name[name_idx++] = path[path_idx++];
        }
        name[name_idx] = '\0'; // null-terminate the filename
        if (name_idx == 0) {
            cache_release(current_inode_block);
            return -1;
        }
        /* At the leaf, current_inode is already the containing directory. */
        if (path_idx == path_len) {
            /* Keep this acquisition for the caller rather than releasing it. */
            parent_handle->block = current_inode_block;
            parent_handle->inode = current_inode;
            return 0;
        }
        path_idx++; // skip the slash
        
        int inode_index = fs_lookup_dir_entry_inode_index(current_inode, name);
        /* Reject missing entries and indices outside the inode table. */
        if (inode_index < 0 || (uint32_t)inode_index >= inode_count) {
            cache_release(current_inode_block);
            return -1;
        }
        /* The copied index survives releasing the old directory's inode block. */
        cache_release(current_inode_block);
        current_inode_block = cache_get(inode_start + inode_index / INODES_PER_BLOCK);
        if (!current_inode_block) {
            return -1;
        }
        inodes = (inode_t *)(current_inode_block->data);
        current_inode = &inodes[inode_index % INODES_PER_BLOCK];
    }
    cache_release(current_inode_block);
    return -1;
}

/**
 * Allocate a new inode.
 * @return The index of the allocated inode, or -1 on failure.
 */
int fs_alloc_inode(){
    cache_block_t *sb_block = cache_get(0);
    if (!sb_block) {
        return -1;
    }
    superblock_t *sb = (superblock_t *)sb_block->data;
    uint32_t inode_count = sb->inode_count;
    uint32_t free_inodes = sb->free_inodes;
    uint32_t inode_bitmap_start = sb->inode_bitmap_start;
    cache_release(sb_block);
    if (free_inodes == 0) {
        return -1; // no free inodes
    }
    // Scan the inode bitmap for a free inode
    for (uint32_t i = 0; i < inode_count; i++) {
        int bit_value = bitmap_read(i, inode_bitmap_start);
        if (bit_value < 0) {
            return -1; // error reading bitmap
        }
        if (bit_value == 0) {
            // Found a free inode, mark it as allocated
            if (bitmap_set(i, inode_bitmap_start, 1) != 0) {
                return -1; // error setting bitmap
            }
            // Update the superblock's free inode count
            sb_block = cache_get(0);
            if (!sb_block) {
                return -1;
            }
            sb = (superblock_t *)sb_block->data;
            sb->free_inodes--;
            cache_mark_dirty(sb_block);
            cache_release(sb_block);
            return i; // return the index of the allocated inode
        }        
    }
    return -1;
}
/**
 * Get a handle to an inode.
 * @param inode_index The index of the inode to get.
 * @param handle A pointer to the handle to fill in.
 * @return 0 on success, -1 on failure.
 * WARNING: The caller is responsible for managing the lifecycle of the returned block and inode.
 */
int fs_get_inode(int inode_index, inode_handle_t *handle) {
    cache_block_t *sb_block = cache_get(0);
    if (!sb_block) {
        return -1;
    }
    superblock_t *sb = (superblock_t *)sb_block->data;
    uint32_t inode_start = sb->inode_start;
    uint32_t inode_count = sb->inode_count;
    cache_release(sb_block);
    if (inode_index < 0 || (uint32_t)inode_index >= inode_count) {
        return -1; // invalid inode index
    }
    cache_block_t *inode_block = cache_get(inode_start + inode_index / INODES_PER_BLOCK);
    if (!inode_block) {
        return -1;
    }
    inode_t *inodes = (inode_t *)(inode_block->data);
    if (!handle) {
        cache_release(inode_block);
        return -1; // memory allocation failure
    }
    handle->block = inode_block;
    handle->inode = &inodes[inode_index % INODES_PER_BLOCK];
    return 0;
}
/* Reserve the first free data block and zero its contents before use.
 * Scan each bitmap block once, skipping bytes whose blocks are all allocated.
 * Update the allocation bitmap and superblock's free_blocks count in cache;
 * dirty blocks are written later by cache flush or eviction.
 * Return the allocated block number, or -1 on exhaustion or cache failure. */
static int fs_alloc_block(void) {
    cache_block_t *block = cache_get(0);
    if (!block) return -1;
    superblock_t sb = *(superblock_t *)block->data;
    cache_release(block);
    if (!sb.free_blocks) return -1;

    const uint32_t bits = BLOCK_SIZE * 8;
    for (uint32_t base = sb.data_start / bits; base < sb.block_bitmap_blocks; base++) {
        block = cache_get(sb.block_bitmap_start + base);
        if (!block) return -1;
        uint8_t *bitmap = block->data;
        uint32_t first = base == sb.data_start / bits ? sb.data_start % bits : 0;
        for (uint32_t bit = first; bit < bits && base * bits + bit < sb.total_blocks; bit++) {
            if (bitmap[bit / 8] == 0xff) {
                bit |= 7;
                continue;
            }
            uint8_t mask = 1u << (bit % 8);
            if (bitmap[bit / 8] & mask) continue;
            uint32_t number = base * bits + bit;
            bitmap[bit / 8] |= mask;
            cache_mark_dirty(block);
            cache_release(block);
            block = cache_get(number);
            if (!block) {
                bitmap_set(number, sb.block_bitmap_start, 0);
                return -1;
            }
            memset(block->data, 0, BLOCK_SIZE);
            cache_mark_dirty(block);
            cache_release(block);
            block = cache_get(0);
            if (!block) {
                bitmap_set(number, sb.block_bitmap_start, 0);
                return -1;
            }
            ((superblock_t *)block->data)->free_blocks--;
            cache_mark_dirty(block);
            cache_release(block);
            return number;
        }
        cache_release(block);
    }
    return -1;
}

/* Release an allocated data block by clearing its bitmap bit and incrementing
 * free_blocks. Reject metadata, the root directory's first block, out-of-range
 * numbers, and blocks that are already free. Contents are not erased here;
 * allocation zeroes them before reuse. The caller must remove any references.
 * Return 0 on success, or -1 for an invalid block or cache failure. */
static int fs_dealloc_block(uint32_t number) {
    cache_block_t *block = cache_get(0);
    if (!block) return -1;
    superblock_t sb = *(superblock_t *)block->data;
    cache_release(block);
    if (number <= sb.data_start || number >= sb.total_blocks ||
        bitmap_read(number, sb.block_bitmap_start) != 1) return -1;
    if (bitmap_set(number, sb.block_bitmap_start, 0) != 0) return -1;
    block = cache_get(0);
    if (!block) return -1;
    ((superblock_t *)block->data)->free_blocks++;
    cache_mark_dirty(block);
    cache_release(block);
    return 0;
}

/* Insert an entry into the first empty filename slot in a directory subtree.
 * depth selects directory data (0), indirect pointers (1), or double indirect
 * pointers (2). Allocate missing blocks and update *number when it was zero.
 * Copy pointer blocks before recursion because cache acquisitions do not pin
 * their contents against eviction; reacquire them to publish new children.
 * Append new block numbers to allocated, advancing *count, so the caller can
 * free an unpublished branch on failure. Starting with count == 0, at most
 * depth + 1 slots are needed. The caller updates the directory inode and size.
 * Return 0 after insertion, 1 when the subtree is full, or -1 on failure. */
static int fs_insert_entry(uint32_t *number, unsigned depth,
                           const dir_entry_t *entry, uint32_t allocated[3],
                           unsigned *count) {
    if (!*number) {
        int fresh = fs_alloc_block();
        if (fresh < 0) return -1;
        allocated[(*count)++] = fresh;
        *number = fresh;
    }
    cache_block_t *block = cache_get(*number);
    if (!block) return -1;
    if (!depth) {
        dir_entry_t *entries = (dir_entry_t *)block->data;
        for (unsigned i = 0; i < ENTRIES_PER_DIR_BLOCK; i++) {
            if (entries[i].filename[0]) continue;
            entries[i] = *entry;
            cache_mark_dirty(block);
            cache_release(block);
            return 0;
        }
        cache_release(block);
        return 1;
    }
    uint32_t pointers[POINTERS_PER_BLOCK];
    memcpy(pointers, block->data, BLOCK_SIZE);
    cache_release(block);
    for (unsigned i = 0; i < POINTERS_PER_BLOCK; i++) {
        uint32_t child = pointers[i];
        int result = fs_insert_entry(&child, depth - 1, entry, allocated, count);
        if (result == 1) continue;
        if (result < 0) return -1;
        if (child != pointers[i]) {
            block = cache_get(*number);
            if (!block) return -1;
            ((uint32_t *)block->data)[i] = child;
            cache_mark_dirty(block);
            cache_release(block);
        }
        return 0;
    }
    return 1;
}
//Filesystem lifecycle


 /* Mount once, formatting the selected drive if its superblock magic is absent.
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

/* Build an empty filesystem on the cache's currently selected drive.
 * Layout: superblock | block bitmap | inode bitmap | inode table | data.
 * The root directory consumes inode 0 and the first data block. This overwrites
 * filesystem metadata; errors can leave a partially formatted filesystem.
 * The caller must initialize the cache for drive_num before calling here. */
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


    /* Round each metadata region up to whole blocks; zero initialization also
     * clears reserved superblock bytes before writing the on-disk structure. */
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
    
    
    /* Begin with every bitmap bit clear, then reserve metadata and root data. */
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
    /* The inclusive endpoint reserves data_start for the root directory. */
    for (uint32_t i = 0; i <= sb.data_start; i++) {
        if (bitmap_set(i, sb.block_bitmap_start, 1) != 0) {
            return -1;
        }
    }
    /* Mark bitmap padding as occupied so allocation cannot select nonexistent
     * blocks beyond the device's actual capacity. */
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
    /* Both root's self entry and parent entry refer back to root inode 0. */
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
    /* Flush the layout before publishing the superblock and its magic number.
     * This is ordered initialization, not an atomic format transaction. */
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

/* Request writeback of dirty cache blocks; the cache API reports no status. */
int fs_flush(void) {
    cache_flush_all();
    return 0;
}


/* =========================================================
 * File operations
 * ========================================================= */



/* Create an empty file and publish its entry in the containing directory. */
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
    // find final name component and validate it
    char name[MAX_FILENAME_LEN + 1];
    uint32_t name_idx = 0;
    for (uint32_t i = path_len; i > 0; i--) {
        if (path[i - 1] == '/') {
            break;
        }
        if (name_idx >= MAX_FILENAME_LEN) {
            return -1; // filename too long
        }
        name[name_idx++] = path[i - 1];
    }
    name[name_idx] = '\0';
    // reverse the name to get the correct order
    for (uint32_t i = 0; i < name_idx / 2; i++) {
        char temp = name[i];
        name[i] = name[name_idx - 1 - i];
        name[name_idx - 1 - i] = temp;
    }
    if (name_idx == 0) {
        return -1; // empty filename
    }
    // resolve the parent directory

    inode_handle_t parent_handle;
    if (fs_lookup_parent(path, &parent_handle) != 0) {
        return -1;
    }
    uint32_t parent_block_number = parent_handle.block->block_num;
    uint32_t parent_offset = (uint8_t *)parent_handle.inode - parent_handle.block->data;
    inode_t parent = *parent_handle.inode;
    cache_release(parent_handle.block);
    int name_inode_index = fs_lookup_dir_entry_inode_index(&parent, name);
    if (name_inode_index >= 0) {
        return -1; // file already exists
    }

    int new_inode_index = fs_alloc_inode();
    if (new_inode_index < 0) {
        return -1;
    }
    if (new_inode_index > UINT16_MAX) goto free_inode;

    inode_handle_t new_inode_handle;
    if (fs_get_inode(new_inode_index, &new_inode_handle) != 0) {
        goto free_inode;
    }
    memset(new_inode_handle.inode, 0, sizeof(inode_t));
    new_inode_handle.inode->type = FILE_TYPE_FILE;
    cache_mark_dirty(new_inode_handle.block);
    cache_release(new_inode_handle.block);

    dir_entry_t new_entry;
    memset(&new_entry, 0, sizeof(new_entry));
    memcpy(new_entry.filename, name, name_idx + 1);
    new_entry.inode_index = new_inode_index;

    uint32_t allocated[3];
    unsigned count = 0;
    int result = 1;
    for (unsigned depth = 0; depth <= 2 && result == 1; depth++) {
        uint32_t *pointers = depth == 0 ? parent.location.direct_pointers :
                             depth == 1 ? parent.location.indirect_pointer :
                                          parent.location.double_indirect_pointer;
        unsigned length = depth == 0 ? N_DIRECT_POINTERS :
                          depth == 1 ? N_INDIRECT_POINTERS : N_DOUBLE_INDIRECT_POINTERS;
        for (unsigned i = 0; i < length && result == 1; i++) {
            result = fs_insert_entry(&pointers[i], depth, &new_entry, allocated, &count);
        }
    }
    if (result == 0) {
        cache_block_t *block = cache_get(parent_block_number);
        if (block) {
            inode_t *inode = (inode_t *)(block->data + parent_offset);
            inode->location = parent.location;
            inode->size += sizeof(dir_entry_t);
            cache_mark_dirty(block);
            cache_release(block);
            return 0;
        }
    }
    while (count) fs_dealloc_block(allocated[--count]);

free_inode:
    {
        cache_block_t *block = cache_get(0);
        if (!block) return -1;
        uint32_t bitmap_start = ((superblock_t *)block->data)->inode_bitmap_start;
        cache_release(block);
        if (bitmap_set(new_inode_index, bitmap_start, 0) == 0) {
            block = cache_get(0);
            if (block) {
                ((superblock_t *)block->data)->free_inodes++;
                cache_mark_dirty(block);
                cache_release(block);
            }
        }
    }
    return -1;
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
