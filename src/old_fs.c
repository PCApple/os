#include "include/fs.h"
#include "include/lru_cache.h"


static fd_table_t fd_table[MAX_NUM_THREADS]; // global file descriptor table
// cache_t* inode_cache;
// cache_t* data_block_cache;
// cache_t* superblock_cache;
// cache_t* bitmap_cache; // all the caches, might be a lot



// -------------------------------- internal helper functions --------------------------------//
// this will first look for the block in the cache, if not found will read from disk and store in cache
// @param block_idx index of the block to read
// @param buffer pointer to store the read block, this should not be allocated beforehand
// @return 0 on success, -1 on failure

int fs_get_and_lock_block(int block_idx, void* buffer){
    if (block_idx < 0) {
        return -1;
    }
    else if (block_idx == 0) { // super block
        buffer = lru_cache_get_and_lock(superblock_cache, block_idx);
        if (buffer == NULL) {
            return -1;
        }
        return 0;
    }
    else if (block_idx <= INODE_BLOCKS) { // reading inode blocks
        buffer = lru_cache_get_and_lock(inode_cache, block_idx);
        if (buffer == NULL) {
            return -1;
        }
        return 0;            
        
    } else if (block_idx <= INODE_BLOCKS + BITMAP_BLOCKS) { // bitmap block
        buffer = lru_cache_get_and_lock(bitmap_cache, block_idx);
        if (buffer == NULL) {
            return -1;
        }
        return 0;     
    } else { // data block
        buffer = lru_cache_get_and_lock(data_block_cache, block_idx);
        if (buffer == NULL) {
            return -1;
        }
        return 0;
    }
}
// frees the block from cache and decrements refcnt, this is the way to mark a block as "dirty" and ready to be written back to disk
// @param block_idx index of the block to free
// @param is_modified 1 if the block has been modified and needs to be written back to disk, 0 otherwise
// @return 0 on success, -1 on failure
int fs_release_block(int block_idx, int is_modified) {
    if (block_idx < 0) {
        return -1;
    }
    else if (block_idx == 0) { // super block
        return lru_cache_update_and_release(superblock_cache, block_idx, is_modified);
    }
    else if (block_idx <= INODE_BLOCKS) { // inode block
        return lru_cache_update_and_release(inode_cache, block_idx, is_modified);
    } else if (block_idx <= INODE_BLOCKS + BITMAP_BLOCKS) { // bitmap block
        return lru_cache_update_and_release(bitmap_cache, block_idx, is_modified);        
    } else { // data block
        return lru_cache_update_and_release(data_block_cache, block_idx, is_modified);
    }
}
// flushes all memory changes to disk
// @return 0 on success, else fail
int fs_flush() {
    //flush superblock
    if (lru_cache_flush(superblock_cache) != 0) {
        return -1;
    }
    //flush inode cache
    if (lru_cache_flush(inode_cache) != 0) {
        return -1;
    }
    //flush bitmap cache
    if (lru_cache_flush(bitmap_cache) != 0) {
        return -1;
    }
    //flush data block cache
    if (lru_cache_flush(data_block_cache) != 0) {
        return -1;
    }
    return 0;
}
// reads the inode at inode_idx into inode_result, WILL LEAVE THE BLOCK LOCKED, you need to call fs_release_block after you are done with the inode
// @param inode_idx index of the inode to read
// @param inode_result pointer to store the read inode
// @param superblock_ptr pointer to the superblock, can be left NULL if superblock is not already read, but it is currently used then you will need to provide it
// @return returns block index on success, 0 on failure
int fs_get_and_lock_inode(int inode_idx, inode_t* inode_result, void* superblock_ptr) {
    superblock_t* sb;
    if (superblock_ptr == NULL) {
        if (fs_get_and_lock_block(0, sb)) {
            return 0;
        }
    } else {
        sb = (superblock_t*)superblock_ptr;
    }
    if (inode_idx < 0 || inode_idx >= MAX_INODES) {
        if (superblock_ptr == NULL) {
            fs_release_block(0, 0);
        }
        return 0;
    }
    inode_t* inode_block;
    int block_idx = sb->inode_index + (inode_idx / INODES_PER_BLOCK);
    if (fs_get_and_lock_block(block_idx, inode_block)) {
        if (superblock_ptr == NULL) {
            fs_release_block(0, 0);
        }
        return 0;
    }
    int inode_offset = inode_idx % INODES_PER_BLOCK;
    *inode_result = inode_block[inode_offset];
    return block_idx;
}
/*
    returns the index of a free data block and marks it as used in the block bitmap and decrements n_free_blocks in the superblock
    @param superblock_ptr pointer to the superblock, can leave NULL if superblock is not already read, but it is currently used then you will need to provide it
*/

uint32_t fs_allocate_block(void* superblock_ptr){
    superblock_t* sb;
    if (superblock_ptr == NULL) {
        if (fs_get_and_lock_block(0, sb)) {
            return 0;
        }
    } else {
        sb = (superblock_t*)superblock_ptr;
    }
    if (sb->n_free_blocks == 0) {
        if (superblock_ptr == NULL) {
            fs_release_block(0, 0);
        }
        return 0; // no free blocks
    }
    char* bb;
    
    uint32_t total_data_blocks_checked = 0; 
    for (int i = 0; i < BITMAP_BLOCKS; i++) {
        int bb_block_idx = 1 + INODE_BLOCKS + i;

        if(fs_get_and_lock_block(bb_block_idx, bb)){
            if (superblock_ptr == NULL) {
                fs_release_block(0, 0);
            }
            return 0;
        }
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (total_data_blocks_checked > DATA_BLOCKS) {
                fs_release_block(bb_block_idx, 0);
                if (superblock_ptr == NULL) {
                    fs_release_block(0, 0);
                }
                return 0; // no free blocks
            }
            total_data_blocks_checked++;
            for (int k = 0; k < 8; k++) {
                if ((bb[j] & (1 << k)) == 0){
                    bb[j] |= (1 << k); // mark it as used
                    uint32_t block_num = i * BLOCK_SIZE * 8 + j * 8 + k;
                    fs_release_block(bb_block_idx, 1);
                    sb->n_free_blocks--;
                    if (superblock_ptr == NULL) {
                        fs_release_block(0, 1);
                    }
                    int block_idx = 1 + INODE_BLOCKS + BITMAP_BLOCKS + block_num; // calculate block index
                    return block_idx;                 
                }
            }
        }
        fs_release_block(bb_block_idx, 0);  
    }
    fs_release_block(0, 0);
    return 0; // no free block found
}
// deallocates the block at the index passed in, does not clear mem
// @param block_idx the block index of the block to free
// @param superblock_ptr pointer to the superblock, can leave NULL if superblock is not already read, but it is currently used then you will need to provide it
// 0 on success, else fail
int fs_deallocate_block(int block_idx, void* superblock_ptr) {
    superblock_t* sb;
    if (superblock_ptr == NULL) {
        if (fs_get_and_lock_block(0, sb)) {
            return -1;
        }
    } else {
        sb = (superblock_t*)superblock_ptr;
    }
    char* bb;
    
    uint32_t block_num = block_idx - sb->data_blocks_index;
    uint32_t block_part_num = block_num / (DATA_BLOCKS / BITMAP_BLOCKS);
    if(fs_get_and_lock_block(1 + INODE_BLOCKS + block_part_num, bb)) {
        if (superblock_ptr == NULL) {
            fs_release_block(0, 0);
        }
        return -1;
    }
    uint32_t byte_idx = (block_num - block_part_num * (DATA_BLOCKS / BITMAP_BLOCKS)) / 8;
    uint8_t bit_idx = block_num % 8;
    uint8_t bits = bb[byte_idx];
    if (bits & (1 << bit_idx)) {
        bb[byte_idx] &= ~(1 << bit_idx); // mark it as free
        sb->n_free_blocks++;
        fs_release_block(sb->bb_index + block_part_num, 1);
        if (superblock_ptr == NULL) {
            fs_release_block(0, 1);
        }
        return 0;
    } else {
        fs_release_block(sb->bb_index + block_part_num, 0);
        if (superblock_ptr == NULL) {
            fs_release_block(0, 0);
        }
        return -1; // block was already free
    }
}
// finds the directory entry in a directory
// @param dir_inode the inode of the directory to be checked
// @param the filename of the file/directory to look for
// @param space to store the directory entry
// @return 0 on success, else fail
int fs_find_dir_entry(inode_t* dir_inode, const char* filename, dir_entry_t* result_entry) {
    if (dir_inode == NULL || filename == NULL) {
        return -1;
    }
    if (dir_inode->type != FILE_TYPE_DIRECTORY) {
        return -1; // not a directory
    }
    // check direct pointers
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (dir_inode->location.direct_pointers[i] == 0) {
            return -1; // no data blocks
        }
        dir_entry_t* entries;
        if (fs_get_and_lock_block(dir_inode->location.direct_pointers[i], (void*)entries)) {
            return -1;
        }
        uint32_t entry_idx = 0;
        while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
            if (strcmp(entries[entry_idx].filename, filename) == 0) {
                strcpy(result_entry->filename, entries[entry_idx].filename);
                result_entry->inode_index = entries[entry_idx].inode_index;
                fs_release_block(dir_inode->location.direct_pointers[i], 0);
                return 0;
            }
            entry_idx++;
        }
        fs_release_block(dir_inode->location.direct_pointers[i], 0);
    }
    // check indirect pointers
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        if (dir_inode->location.indirect_pointer[i] == NULL) {
            return -1; // no indirect blocks
        }
        uint32_t* indirect_block;
        if (fs_get_and_lock_block((uint32_t)dir_inode->location.indirect_pointer[i], (void*)indirect_block)) {
            return -1;
        }
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (indirect_block[j] == 0) {
                return -1; // no data blocks
            }
            dir_entry_t* entries;
            if (fs_get_and_lock_block((uint32_t)indirect_block[j], (void*)entries)) {
                return -1;
            }
            uint32_t entry_idx = 0;
            while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                if (strcmp(entries[entry_idx].filename, filename) == 0) {
                    strcpy(result_entry->filename, entries[entry_idx].filename);
                    result_entry->inode_index = entries[entry_idx].inode_index;
                    fs_release_block(indirect_block[j], 0); //dir block
                    fs_release_block(dir_inode->location.indirect_pointer[i], 0); // indirect block
                    return 0;
                }
                entry_idx++;
            }
            fs_release_block(indirect_block[j], 0); // dir block
        }
        fs_release_block(dir_inode->location.indirect_pointer[i], 0); // indirect block
    }
    // check double indirect pointers
    for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
        if (dir_inode->location.double_indirect_pointer[i] == NULL) {
            return -1; // no double indirect blocks
        }
        void* double_indirect_block_buffer;
        if (fs_get_and_lock_block((uint32_t)dir_inode->location.double_indirect_pointer[i], double_indirect_block_buffer)) {
            return -1;
        }
        uint32_t* double_indirect_block = (uint32_t*)double_indirect_block_buffer;
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (double_indirect_block[j] == 0) {
                fs_release_block(dir_inode->location.double_indirect_pointer[i], 0);
                return -1; // no indirect blocks
            }
            uint32_t* indirect_block;
            if (fs_get_and_lock_block((uint32_t)double_indirect_block[j], (void*)indirect_block)) {
                fs_release_block((uint32_t)dir_inode->location.double_indirect_pointer[i], 0);
                return -1;
            }
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                if (indirect_block[k] == 0) {
                    return -1; // no data blocks
                }
                dir_entry_t* entries;
                if (fs_get_and_lock_block((uint32_t)indirect_block[k], (void*)entries)) {
                    fs_release_block((uint32_t)double_indirect_block[j], 0);
                    fs_release_block(dir_inode->location.double_indirect_pointer[i], 0);
                    return -1;
                }
                uint32_t entry_idx = 0;
                while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                    if (strcmp(entries[entry_idx].filename, filename) == 0) {
                        strcpy(result_entry->filename, entries[entry_idx].filename);
                        result_entry->inode_index = entries[entry_idx].inode_index;
                        fs_release_block((uint32_t)indirect_block[k], 0); // dir block
                        fs_release_block((uint32_t)double_indirect_block[j], 0); // indirect block
                        fs_release_block(dir_inode->location.double_indirect_pointer[i], 0); // double indirect block
                        return 0;
                    }
                    entry_idx++;
                }
                fs_release_block((uint32_t)indirect_block[k], 0); // dir block
            }
            fs_release_block((uint32_t)double_indirect_block[j], 0); // indirect block
        }
        fs_release_block(dir_inode->location.double_indirect_pointer[i], 0); // double indirect block
    }
    return -1; // not found
}
// will get parent directory and leave it locked, you need to call fs_release_block after you are done with the inode, NEEDS SB
// @param path full path to the file/directory
// @return pointer to the parent directory inode on success, NULL on failure
inode_t* get_parent_dir_inode(const char* path){
    superblock_t* sb;
    if (fs_get_and_lock_block(0, sb) != 0){
        return NULL;
    }
    uint32_t len = strlen(path);
    uint32_t path_idx = 1;
    inode_t* curr_dir;
    uint32_t curr_inode_block_idx = 1 + (sb->inode_index / INODES_PER_BLOCK);
    if (fs_get_and_lock_inode(sb->inode_index, curr_dir, sb) == 0){
        fs_release_block(0, 0);
        return NULL;
    }
    uint32_t par_inode_block_idx = NULL;
    inode_t* parent_dir = NULL;
    while (path_idx < strlen(path)) {
        if (curr_dir->type != FILE_TYPE_DIRECTORY) {
            if (parent_dir != NULL) {
                fs_release_block(par_inode_block_idx, 0);
            }
            fs_release_block(curr_inode_block_idx, 0);
            fs_release_block(0, 0);
            return NULL; // not a directory
        }
        char name[MAX_FILENAME_LEN + 1]; // name for the next file/directory to be read
        uint32_t name_idx = 0;
        while (path[path_idx] != '/' && path[path_idx] != '\0' && name_idx < MAX_FILENAME_LEN) {
            name[name_idx++] = path[path_idx++];
        }
        if (path_idx == len || (path[path_idx] == '/' && path_idx + 1 == len)) { // this is either the file(part 1) or the last directory(part 2)
            return curr_dir; // reached the target file/directory (this is the parent)
        }
        name[name_idx] = '\0';
        // search for name in curr_dir
        dir_entry_t* dir_entry;
        if (fs_find_dir_entry(curr_dir, name, dir_entry) != 0) {
            if (parent_dir != NULL) {
                fs_release_block(par_inode_block_idx, 0);
            }
            fs_release_block(curr_inode_block_idx, 0);
            fs_release_block(0, 0);
            return NULL; // entry not found
        }
        if (parent_dir != NULL) {
            fs_release_block(par_inode_block_idx, 0);
        }
        parent_dir = curr_dir;
        par_inode_block_idx = curr_inode_block_idx;
        curr_inode_block_idx = 1 + (dir_entry->inode_index / INODES_PER_BLOCK);
        if (fs_get_and_lock_inode(dir_entry->inode_index, curr_dir, sb) != 0) {
            if (parent_dir != NULL) {
                fs_release_block(par_inode_block_idx, 0);
            }
            fs_release_block(0, 0);
            return NULL;
        }
        if (path[path_idx] == '/') {
            path_idx++; // skip the slash
        }
    }
    fs_release_block(curr_inode_block_idx, 0);
    fs_release_block(par_inode_block_idx, 0);
    fs_release_block(0, 0);
    return NULL; // should not reach here probably
}
// extracts the filename from the full path, i dont think this does directory names but im too stupid to eyeball it
// @param path full path to the file/directory
// @param filename pointer to store the extracted filename
void read_filename(const char* path, char* filename) {
    uint32_t last_slash = 0;
    uint8_t isdir = 0;
    uint32_t stop_idx = strlen(path);
    if (path[strlen(path)-1] == '/') {
        isdir = 1;
        stop_idx--; // ignore trailing slash
    }
    for (int i = 0; i < stop_idx; i++) {
        if (path[i] == '/') {
            last_slash = i;
        }
    }
    filename[MAX_FILENAME_LEN] = '\0'; // ensure null termination
    int cnt = 0;
    for (int i = last_slash + 1; i < stop_idx && cnt < MAX_FILENAME_LEN+1; i++) {
        filename[cnt++] = path[i];
    }
    filename[cnt] = '\0';
}
// expands the file to be the required size, expects superblock to be free
// @param inode the inode of the file that will be increased
// @param new_size the new size of the file
// @return 0 on success, -1 on failure
int expand_file_size(inode_t* inode, uint32_t new_size) {
        if (new_size <= inode->size) {
            return 0; // no need to expand
        }
        if (new_size < 1) {
            printk("ERROR: File size is 0\n");
            return -1;
        }
        superblock_t* sb;
        if (fs_get_and_lock_block(0, (void*)sb) != 0) {
            printk("Error, superblock cannot be opened\n");
            return -1;
        }
        uint32_t n_free_blocks = sb->n_free_blocks;
        fs_release_block(0, 0);
        uint32_t remaining_blocks = ceiling(new_size, BLOCK_SIZE);
        
        // really gross calc to see if there is enough space
        uint32_t used_blocks = ceiling(inode->size, BLOCK_SIZE);
        uint32_t required_blocks_to_allocate = 0;
        uint32_t direct_ptr_cnt = N_DIRECT_POINTERS;
        uint32_t indirect_ptr_cnt = N_INDIRECT_POINTERS;
        uint32_t double_indirect_ptr_cnt = N_DOUBLE_INDIRECT_POINTERS;
        uint32_t dir_per_indir_cnt = 0;
        uint32_t indir_per_dindir_cnt = 0;
        for(int i = 0; i < remaining_blocks; i++){
            int cnt_to_remove = 0;
            if (direct_ptr_cnt > 0) {
                cnt_to_remove++;
            } else if (indirect_ptr_cnt > 0){
                if (direct_ptr_cnt % DIRECT_BLOCKS_PER_INDIRECT_BLOCK == 0){
                    indirect_ptr_cnt--;
                    cnt_to_remove++;
                }
                cnt_to_remove++;
                dir_per_indir_cnt = (dir_per_indir_cnt+1)%DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            } else if (double_indirect_ptr_cnt > 0){
                if (indir_per_dindir_cnt%DIRECT_BLOCKS_PER_INDIRECT_BLOCK == 0){
                    double_indirect_ptr_cnt--;
                    cnt_to_remove++;
                }
                if (dir_per_indir_cnt % DIRECT_BLOCKS_PER_INDIRECT_BLOCK == 0){
                    indir_per_dindir_cnt = (indir_per_dindir_cnt+ 1)%DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
                    cnt_to_remove++;                                        
                }
                dir_per_indir_cnt =(dir_per_indir_cnt+1)%DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
                cnt_to_remove++;
            } else {
                printk("Error: size exceeds capacity of inode\n");
            }
            if (used_blocks >= cnt_to_remove) {
                used_blocks -= cnt_to_remove;
            } else if (used_blocks == 0) {
                required_blocks_to_allocate += cnt_to_remove;
            } else {
                uint32_t diff = cnt_to_remove - used_blocks;
                used_blocks = 0;
                required_blocks_to_allocate += diff;
            }
        }
        // check if there is enough space
        if (required_blocks_to_allocate > sb->n_free_blocks) {
            printk("Error: file size will exceed the number of remaining freeblocks, needed blocks: %d, remaining free blocks: %d\n", remaining_blocks, sb->n_free_blocks);
            return -1;
        }
        for (int i = 0; i < N_DIRECT_POINTERS; i++) {
            if (inode->location.direct_pointers[i] == NULL) {
                uint32_t new_block_idx = fs_allocate_block();
                inode->location.direct_pointers[i] = new_block_idx;
            }
            remaining_blocks--;
            if (remaining_blocks == 0) {
                inode->size = new_size; 
                return 0;
            }
        }
        if (remaining_blocks > 0) { // doing one level of indirection
            // handle indirect pointers
            for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
                if (inode->location.indirect_pointer[i] == NULL) {
                    uint32_t indirect_block_idx = fs_allocate_block();
                    inode->location.indirect_pointer[i] = indirect_block_idx;
                }
                uint32_t* indirect_block;
                if (fs_get_and_lock_block((uint32_t)inode->location.indirect_pointer[i], (void*)indirect_block) != 0) {
                    printk("ERROR: Failed to read indirect block %d\n", inode->location.indirect_pointer[i]);
                    return -1;
                }
                for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                    if (indirect_block[j] == 0) {
                        uint32_t new_block_idx = fs_allocate_block();
                        indirect_block[j] = new_block_idx;
                    }
                    remaining_blocks--;
                    if (remaining_blocks == 0) {
                        inode->size = new_size; 
                        fs_release_block(inode->location.indirect_pointer[i], 1);   
                        return 0;
                    }
                }   
                fs_release_block(inode->location.indirect_pointer[i], 1);             
            }
        }
        if (remaining_blocks > 0) { // doing double indirection
            for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
                if (inode->location.double_indirect_pointer[i] == NULL) {
                    uint32_t double_indirect_block_idx = fs_allocate_block();
                    inode->location.double_indirect_pointer[i] = double_indirect_block_idx;
                }
                uint32_t* double_indirect_block;
                if (fs_get_and_lock_block(inode->location.double_indirect_pointer[i], double_indirect_block) != 0){
                    printk("Error: failed to read double indirect block %d\n", inode->location.double_indirect_pointer[i]);
                }
                for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                    if (double_indirect_block[j] == NULL) {
                        double_indirect_block[j] = fs_allocate_block();
                    }
                    uint32_t* indirect_block;
                    if (fs_get_and_lock_block(double_indirect_block[j], indirect_block)){
                        fs_release_block(inode->location.double_indirect_pointer[i], 1);
                        printk("Error: Failed to read indirect %d from double block %d\n", double_indirect_block[j], inode->location.double_indirect_pointer[i]);
                        return -1;
                    }
                    for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                        if (indirect_block[k] == NULL) {
                            indirect_block[k] = fs_allocate_block();
                        }
                        remaining_blocks--;
                        if (remaining_blocks == 0) {
                            fs_release_block(inode->location.double_indirect_pointer[i], 1);
                            fs_release_block(double_indirect_block[j],1);
                            inode->size = new_size; 
                            return 0;
                        }
                    }
                    fs_release_block(double_indirect_block[j],1);
                }
                fs_release_block(inode->location.double_indirect_pointer[i], 1);
            }
        }
        if (remaining_blocks > 0) {
            printk("ERROR: File size exceeds maximum limit\n");
            return -1;
        }  
    return 0;
}
int create_or_mkdir(const char* path) {
    superblock_t* sb;
    if (path == NULL || path[0] != '/') {
        printk("ERROR: Invalid path\n");
        return -1;
    }
    uint8_t is_dir = 0;
    if (path[strlen(path)-1] == '/') {
        is_dir = 1;
    }
    inode_t* parent_dir = get_parent_dir_inode(path);
    if (parent_dir == NULL) {
        printk("ERROR: Parent directory does not exist\n");
        return -1;
    }
    // check if file/dir already exists
    char name[MAX_FILENAME_LEN + 1];  
    read_filename(path, name);
    dir_entry_t* dir_entry;
    if (fs_find_dir_entry(parent_dir, name, dir_entry)){
        printk("Error: cannot find dir entry in parent");
        return -1;
    }
    if (dir_entry != NULL) {
        printk("ERROR: File/Directory already exists\n");
        return -2;
    }
    
    if (fs_get_and_lock_block(0, sb)){
        printk("ERROR: cannot grab superblock\n");
        return -1;
    }
    // allocate new inode
    uint32_t new_inode_idx = -1; // block idx
    inode_t* new_inode;
    for (int i = 0; i < MAX_INODES; i++)
    {
        if(fs_get_and_lock_inode(i, new_inode, sb)){
            continue; // inode is used, skip
        }
        if (new_inode->type == FILE_TYPE_UNUSED) {
            new_inode_idx = sb->inode_index + i;
            break;
        }
        fs_release_block(sb->inode_index + i, 0);
    }
    if (new_inode_idx == -1) {
        printk("ERROR: No free inodes available\n");
        return -1;
    }
    new_inode->idx = new_inode_idx;
    new_inode->type = is_dir ? FILE_TYPE_DIRECTORY : FILE_TYPE_FILE;
    new_inode->size = 0;
    for (int j = 0; j < N_DIRECT_POINTERS; j++) {
        new_inode->location.direct_pointers[j] = 0;
    }
    for (int j = 0; j < N_INDIRECT_POINTERS; j++)
    {
        new_inode->location.indirect_pointer[j] = 0;
    }
    for (int j = 0; j < N_DOUBLE_INDIRECT_POINTERS; j++)
    {
        new_inode->location.double_indirect_pointer[j] = 0;
    }
    sb->n_free_inodes--;
    fs_release_block(0, 1);
    if (is_dir) { // need to setup directory block
        new_inode->location.direct_pointers[0] = fs_allocate_block();
        if (new_inode->location.direct_pointers[0] == NULL) {
            printk("ERROR: Failed to allocate block for new directory\n");
            new_inode->idx = 0;
            new_inode->type = FILE_TYPE_UNUSED;
            new_inode->size = 0;
            fs_release_block(new_inode_idx, 1);
            if (fs_get_and_lock_block(0, sb)){
                printk("MAJOR ERROR: super block is corrupted, need an additional free block\n");
                return -1;
            }
            sb->n_free_inodes++;
            fs_release_block(0, 1);
            return -1;
        }
        new_inode->size += sizeof(dir_entry_t);
        dir_entry_t* new_dir = (dir_entry_t*)new_inode->location.direct_pointers[0];
        new_dir[0].filename[0] = '.';
        new_dir[0].inode_index = new_inode_idx;
        new_dir[0].filename[1] = '\0'; // end of entries
    }
    //------------------------ find a blank entry in parent directory-------------------------//
    for (int i  = 0; i < N_DIRECT_POINTERS; i++) {
        if (parent_dir->location.direct_pointers[i] == NULL) {
            parent_dir->location.direct_pointers[i] = fs_allocate_block(); // allocate new block for directory entries
            if (parent_dir->location.direct_pointers[i] == NULL) {
                printk("ERROR: Failed to allocate block for parent directory entries\n");
                new_inode->idx = 0;
                new_inode->type = FILE_TYPE_UNUSED;
                new_inode->size = 0;
                fs_deallocate_block(new_inode->location.direct_pointers[0]);
                new_inode->location.direct_pointers[0] = 0;
                fs_release_block(new_inode_idx, 1);
                if (fs_get_and_lock_block(0, sb)){
                    printk("MAJOR ERROR: super block is corrupted, need an additional free block\n");
                    return -1;
                }
                sb->n_free_inodes++;
                fs_release_block(0, 1);
                return -1;
            }
        }
        dir_entry_t* entries;
        if (fs_get_and_lock_block(parent_dir->location.direct_pointers[i], entries)){
            printk("Error: block not accessible");

            return -1;
        }
        for (int j = 0; j < ENTRIES_PER_DIR_BLOCK; j++) {
            if (entries[j].filename[0] == '\0') {
                // found a free entry
                entries[j].inode_index = new_inode_idx;
                strncpy(entries[j].filename, name, MAX_FILENAME_LEN);
                entries[j].filename[MAX_FILENAME_LEN] = '\0'; // ensure null termination
                parent_dir->num_accessed++; // easier to keep track of files in opened directories
                parent_dir->size += sizeof(dir_entry_t);
                return 0;
            }
        }
    }
    // check indirect pointers
    for (int i = 0; i < N_INDIRECT_POINTERS; i++)
    {
        if (parent_dir->location.indirect_pointer[i] == NULL) {
            parent_dir->location.indirect_pointer[i] = fs_allocate_block();
            if (parent_dir->location.indirect_pointer[i] == NULL) {
                printk("ERROR: Failed to allocate indirect block for parent directory entries\n");
                return -1;
            }
        }
        void** direct_blocks = (void**)parent_dir->location.indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (direct_blocks[j] == NULL) {
                direct_blocks[j] = fs_allocate_block();
                if (direct_blocks[j] == NULL) {
                    printk("ERROR: Failed to allocate block for parent directory entries\n");
                    return -1;
                }
            }
            dir_entry_t* entries = (dir_entry_t*)direct_blocks[j];
            for (int k = 0; k < ENTRIES_PER_DIR_BLOCK; k++) {
                if (entries[k].filename[0] == '\0') {
                    // found a free entry
                    entries[k].inode_index = new_inode_idx;
                    strncpy(entries[k].filename, name, MAX_FILENAME_LEN);
                    entries[k].filename[MAX_FILENAME_LEN] = '\0'; // ensure null termination
                    parent_dir->num_accessed++; // easier to keep track of files in opened directories
                    parent_dir->size += sizeof(dir_entry_t);
                    return 0;
                }
            }
        }
    }
    // check double indirect pointers
    for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
        if (parent_dir->location.double_indirect_pointer[i] == NULL) {
            parent_dir->location.double_indirect_pointer[i] = fs_allocate_block();
            if (parent_dir->location.double_indirect_pointer[i] == NULL) {
                printk("ERROR: Failed to allocate double indirect block for parent directory entries\n");
                return -1;
            }
        }
        void*** indirect_blocks = (void***)parent_dir->location.double_indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (indirect_blocks[j] == NULL) {
                indirect_blocks[j] = fs_allocate_block();
                if (indirect_blocks[j] == NULL) {
                    printk("ERROR: Failed to allocate indirect block for parent directory entries\n");
                    return -1;
                }
            }
            void** direct_blocks = (void**)indirect_blocks[j];
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                if (direct_blocks[k] == NULL) {
                    direct_blocks[k] = fs_allocate_block();
                    if (direct_blocks[k] == NULL) {
                        printk("ERROR: Failed to allocate block for parent directory entries\n");
                        return -1;
                    }
                }
                dir_entry_t* entries = (dir_entry_t*)direct_blocks[k];
                for (int l = 0; l < ENTRIES_PER_DIR_BLOCK; l++) {
                    if (entries[l].filename[0] == '\0') {
                        // found a free entry
                        entries[l].inode_index = new_inode_idx;
                        strncpy(entries[l].filename, name, MAX_FILENAME_LEN);
                        entries[l].filename[MAX_FILENAME_LEN] = '\0'; // ensure null termination
                        parent_dir->num_accessed++; // easier to keep track of files in opened directories
                        parent_dir->size += sizeof(dir_entry_t);
                        return 0;
                    }
                }
            }
        }
    }
    return -1; // parent directory is full
}

// -------------------------------- exposed functions --------------------------------//
int fs_init(int drive_num) {

    // first, init the caches:
    superblock_cache = lru_cache_init(1, (uint32_t)drive_num);
    inode_cache = lru_cache_init(MAX_INODES, (uint32_t)drive_num);
    bitmap_cache = lru_cache_init(BITMAP_BLOCKS, (uint32_t)drive_num);
    data_block_cache = lru_cache_init(DATA_BLOCKS, (uint32_t)drive_num);

    // __asm__ volatile ("cli");
    // fs_ptr = fs_start;
    // if (fs_start == NULL || fs_size != FS_SIZE) {
    //     __asm__ volatile ("sti");
    //     return -1;
    // }
    // int err = ide_read_sectors(DRIVE_NUM, 0, 1, DS, (uint32_t)fs_start);
    // if (err != 0){
    //     printk("ERROR: Failed to read filesystem from disk code: %d\n", err);
    //     return -1;
    // }
    // superblock_t* sb = (superblock_t*)fs_start;
    // if (sb->magic_number == MAGIC_NUMBER) {
    //     printk("Filesystem found, reading in existing filesystem\n");
    //     int num_sectors_to_read = FS_SIZE / 512;
    //     err = ide_read_sectors(DRIVE_NUM, 0, num_sectors_to_read, DS, (uint32_t)fs_start);
    //     if (err != 0){
    //         printk("ERROR: Failed to read filesystem from disk code: %d\n", err);
    //         return -1;
    //     }
    //     __asm__ volatile ("sti");
    //     return 0;
    // }
    // printk("Initializing new filesystem\n");
    // sb->n_free_blocks = DATA_BLOCKS;
    // sb->n_free_inodes = MAX_INODES;
    // // Initialize inodes
    // inode_t* inodes = (inode_t*)((uint8_t*)fs_start + BLOCK_SIZE); // inodes start after superblock
    // for (uint32_t i = 0; i < MAX_INODES; i++)
    // {
    //     inodes[i].type = FILE_TYPE_UNUSED; // default type
    //     inodes[i].size = 0;
    //     for (int j = 0; j < N_DIRECT_POINTERS; j++) {
    //         inodes[i].location.direct_pointers[j] = 0;
    //     }
    //     for (int j = 0; j < N_INDIRECT_POINTERS; j++) {
    //         inodes[i].location.indirect_pointer[j] = 0;
    //     }
    //     for (int j = 0; j < N_DOUBLE_INDIRECT_POINTERS; j++) {
    //         inodes[i].location.double_indirect_pointer[j] = 0;
    //     }
    // }
    // // Initialize block bitmap
    // block_bitmap_t* bb = (block_bitmap_t*)((uint8_t*)inodes + INODE_BLOCKS * BLOCK_SIZE);
    // memset(bb->bitmap, 0, sizeof(bb->bitmap));
    // // setup root directory
    // inodes[0].type = FILE_TYPE_DIRECTORY;
    // inodes[0].idx = 0;
    // inodes[0].size = 0;
    // inodes[0].location.direct_pointers[0] = fs_allocate_block();
    // if (inodes[0].location.direct_pointers[0] == NULL) {
    //     __asm__ volatile ("sti");
    //     return -1; // failed to allocate block for root directory
    // }
    // inodes[0].size += sizeof(dir_entry_t);
    // sb->n_free_inodes--;
    // dir_entry_t* root_dir = (dir_entry_t*)inodes[0].location.direct_pointers[0];
    // root_dir[0].filename[0] = '.';
    // root_dir[0].inode_index = 0;
    // root_dir[0].filename[1] = '\0'; // end of entries
    // sb->root_inode_index = 0;
    // sb->inodes_start = (void*)inodes;
    // sb->block_bitmap_start = (void*)bb;
    // sb->data_blocks_start = (void*)(sb->block_bitmap_start + BLOCK_BITMAP_BLOCKS * BLOCK_SIZE);
    // // __asm__ volatile ("sti");
    // return 0;

}

/*
int fs_create(const char* path) {
    if (path == NULL) {
        return -1;
    }
    if (path[strlen(path)-1] == '/') {
        printk("ERROR: Path ends with '/', use fs_mkdir for directories\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int ret = create_or_mkdir(path);
    __asm__ volatile ("sti");
    return ret;
}
int fs_mkdir(const char* path) {
    if (path == NULL) {
        return -1;
    }
    if (path[strlen(path)-1] != '/') {
        printk("ERROR: Path does not end with '/', use fs_create for files\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int ret = create_or_mkdir(path);
    __asm__ volatile ("sti");
    return ret;
}
int fs_open(const char* path) {
    char name[MAX_FILENAME_LEN + 1];
    uint8_t is_root = 0;
    
    if (path == NULL || path[0] != '/') {
        printk("ERROR: Invalid path\n");
        return -1;
    }
        
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    superblock_t* sb = (superblock_t*)fs_ptr;
    inode_t* parent_dir = get_parent_dir_inode(path);
    if (parent_dir == NULL) {
        if (strcmp(path, "/") == 0) {
            is_root = 1;
        } else {
            printk("ERROR: Parent directory does not exist\n");
            return -1;
        }
    }
    if (is_root) { // im just gonna hardcode this 
        name[0] = '.';
        name[1] = '\0';
        int fd = -1;
        for (int i = 0; i < MAX_FD_ENTRIES; i++)
        {
            if ((fd_table[tid].entries[i].status_flags & 0x1) == 0) { // not present
                fd = i;
                break;
            }
        }
        if (fd == -1) {
            printk("ERROR: No free file descriptor available\n");
            return -1;
        }
        fd_table[tid].entries[fd].status_flags = 0x7; // mark as present, read, write for now
        fd_table[tid].entries[fd].inode = &((inode_t*)sb->inodes_start)[sb->root_inode_index];
        fd_table[tid].entries[fd].file_offset = 0;
        ((inode_t*)sb->inodes_start)[sb->root_inode_index].num_accessed++;
        return fd;
    }
    // getting the inode of the file
    read_filename(path, name);
    dir_entry_t* dir_entry = fs_find_dir_entry(parent_dir, name);
    if (dir_entry == NULL) {
        printk("ERROR: File does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* file_inode = &((inode_t*)sb->inodes_start)[dir_entry->inode_index];
    if (file_inode == NULL) {
        printk("ERROR: File does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    // find free fd table entry
    int fd = -1;
    for (int i = 0; i < MAX_FD_ENTRIES; i++)
    {
        if ((fd_table[tid].entries[i].status_flags & 0x1) == 0) { // not present
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        printk("ERROR: No free file descriptor available\n");
        __asm__ volatile ("sti");
        return -1;
    }
    fd_table[tid].entries[fd].status_flags = 0x7; // mark as present, read, write for now
    fd_table[tid].entries[fd].inode = file_inode;
    fd_table[tid].entries[fd].file_offset = 0;
    file_inode->num_accessed++;
    __asm__ volatile ("sti");
    return fd;
}

int fs_close(int fd) {
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        printk("ERROR: Invalid file descriptor\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        printk("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    inode->num_accessed--;
    fd_entry->status_flags = 0; // mark as not present
    fd_entry->inode = NULL;
    fd_entry->file_offset = 0;
    __asm__ volatile ("sti");
    return 0;
}

int fs_read_helper(int fd, char* buf, uint32_t n_bytes, int should_lock) {
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        printk("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        printk("ERROR: Invalid buffer\n");
        return -1;
    }
    if (n_bytes == 0) {
        return 0; // nothing to read
    }
    if (should_lock) {
        __asm__ volatile ("cli");
    }
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        printk("ERROR: File descriptor not open\n");
        if (should_lock) {
            __asm__ volatile ("sti");
        }
        return -1;
    }
    if ((fd_entry->status_flags & 0x2) == 0) {
        printk("ERROR: File descriptor not opened for reading\n");
        if (should_lock) {
            __asm__ volatile ("sti");
        }
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type == FILE_TYPE_UNUSED) {
        printk("ERROR: Not a File or DIR type\n");
        if (should_lock) {
            __asm__ volatile ("sti");
        }
        return -1;
    }
    if (fd_entry->file_offset + n_bytes > inode->size) {
        n_bytes = inode->size - fd_entry->file_offset; // adjust n_bytes to read only up to file size
    }
    uint32_t bytes_read = 0;
    uint32_t current_offset = fd_entry->file_offset;
    while (bytes_read < n_bytes) {
        uint32_t block_idx = current_offset / BLOCK_SIZE;
        uint32_t block_offset = current_offset % BLOCK_SIZE;
        char* block_addr = NULL;
        if (block_idx < N_DIRECT_POINTERS) {
            block_addr = (char*)inode->location.direct_pointers[block_idx];
        } else if (block_idx < N_DIRECT_POINTERS + DIRECT_BLOCKS_PER_INDIRECT_BLOCK) {
            uint32_t indirect_idx = (block_idx - N_DIRECT_POINTERS)/DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            void** indirect_block = (void**)inode->location.indirect_pointer[indirect_idx];
            uint32_t direct_block_idx = (block_idx - N_DIRECT_POINTERS) % DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            block_addr = (char*)indirect_block[direct_block_idx];
        } else {
            uint32_t from_beginning_of_double_indirect = block_idx - N_DIRECT_POINTERS - DIRECT_BLOCKS_PER_INDIRECT_BLOCK; // shifts index to make it easier
            uint32_t double_indirect_idx = from_beginning_of_double_indirect / (DIRECT_BLOCKS_PER_INDIRECT_BLOCK * DIRECT_BLOCKS_PER_INDIRECT_BLOCK);
            void*** double_indirect_block = (void***)inode->location.double_indirect_pointer[double_indirect_idx];
            uint32_t indirect_block_idx = (from_beginning_of_double_indirect % (DIRECT_BLOCKS_PER_INDIRECT_BLOCK*DIRECT_BLOCKS_PER_INDIRECT_BLOCK))/ DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            void** indirect_block = (void**)double_indirect_block[indirect_block_idx];
            uint32_t direct_block_idx = from_beginning_of_double_indirect % DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            block_addr = (char*)indirect_block[direct_block_idx];
        }
        if (block_addr == NULL) {
            printk("ERROR: Data block not allocated\n");
            if (should_lock) {
                __asm__ volatile ("sti");
            }
            return -1;
        }
        buf[bytes_read] = *(block_addr + block_offset);
        bytes_read++;
        current_offset++;
        fd_entry->file_offset = current_offset;
    }
    if (should_lock) {
        __asm__ volatile ("sti");
    }
    return bytes_read;
}
int fs_read( int fd, char* buf, uint32_t n_bytes) {
    return fs_read_helper( fd, buf, n_bytes, 1);
}
int fs_write(int fd, char* buf, uint32_t n_bytes) {
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        printk("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        printk("ERROR: Invalid buffer\n");
        return -1;
    }
    if (n_bytes == 0) {
        printk("whyyyyyyy\n");
        return 0; // nothing to write
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        printk("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if ((fd_entry->status_flags & 0x4) == 0) {
        printk("ERROR: File descriptor not opened for writing\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type != FILE_TYPE_FILE) {
        printk("ERROR: Not a File type\n");
        __asm__ volatile ("sti");
        return -1;
    }
    uint32_t offset_after_write = fd_entry->file_offset + n_bytes;
    if (offset_after_write > inode->size) { // need to allocate more blocks
        if (expand_file_size(inode, offset_after_write) != 0) {
            __asm__ volatile ("sti");
            return -1; // failed to expand file size
        }
    }
    uint32_t bytes_written = 0;
    uint32_t current_offset = fd_entry->file_offset;
    while (bytes_written < n_bytes) {
        uint32_t block_idx = current_offset / BLOCK_SIZE;
        uint32_t block_offset = current_offset % BLOCK_SIZE;
        char* block_addr = NULL;
        if (block_idx < N_DIRECT_POINTERS) {
            block_addr = (char*)inode->location.direct_pointers[block_idx];
        } else if (block_idx < N_DIRECT_POINTERS + DIRECT_BLOCKS_PER_INDIRECT_BLOCK) {
            uint32_t indirect_idx = (block_idx - N_DIRECT_POINTERS)/DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            void** indirect_block = (void**)inode->location.indirect_pointer[indirect_idx];
            uint32_t direct_block_idx = (block_idx - N_DIRECT_POINTERS) % DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            block_addr = (char*)indirect_block[direct_block_idx];
        } else {
            uint32_t from_beginning_of_double_indirect = block_idx - N_DIRECT_POINTERS - DIRECT_BLOCKS_PER_INDIRECT_BLOCK; // shifts index to make it easier
            uint32_t double_indirect_idx = from_beginning_of_double_indirect / (DIRECT_BLOCKS_PER_INDIRECT_BLOCK * DIRECT_BLOCKS_PER_INDIRECT_BLOCK);
            void*** double_indirect_block = (void***)inode->location.double_indirect_pointer[double_indirect_idx];
            uint32_t indirect_block_idx = (from_beginning_of_double_indirect % (DIRECT_BLOCKS_PER_INDIRECT_BLOCK*DIRECT_BLOCKS_PER_INDIRECT_BLOCK))/ DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            void** indirect_block = (void**)double_indirect_block[indirect_block_idx];
            uint32_t direct_block_idx = from_beginning_of_double_indirect % DIRECT_BLOCKS_PER_INDIRECT_BLOCK;
            block_addr = (char*)indirect_block[direct_block_idx];
        }
        if (block_addr == NULL) {
            printk("ERROR: Data block not allocated\n");
            __asm__ volatile ("sti");
            return -1;
        }
        *(block_addr + block_offset) = buf[bytes_written];
        bytes_written++;
        current_offset++;
        fd_entry->file_offset = current_offset;
    }
    __asm__ volatile ("sti");
    return bytes_written;
}

int fs_lseek(int fd, uint32_t offset) {
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        printk("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (offset < 0) {
        printk("ERROR: Invalid offset\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        printk("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (offset > inode->size) {
        fd_entry->file_offset = inode->size; // set to end of file
    } else {
        fd_entry->file_offset = offset;
    }
    __asm__ volatile ("sti");
    return 0;
}
int fs_unlink(const char *path) {
    if (path == NULL) {
        printk("ERROR: Invalid path\n");
        return -1;
    }
    if (strcmp(path, "/") == 0) {
        printk("ERROR: Cannot delete root directory\n");
        return -1;
    }
    int is_dir = 0;
    if (path[strlen(path)-1] == '/') {
        is_dir = 1;
    }
    
    __asm__ volatile ("cli");
    inode_t* parent_dir = get_parent_dir_inode(path);
    if (parent_dir == NULL) {
        printk("ERROR: Parent directory does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    char name[MAX_FILENAME_LEN + 1];
    read_filename(path, name);
    dir_entry_t* dir_entry = fs_find_dir_entry(parent_dir, name);
    if (dir_entry == NULL) {
        printk("ERROR: File/Directory does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = &((inode_t*)((superblock_t*)fs_ptr)->inodes_start)[dir_entry->inode_index];
    if (inode == NULL) {
        printk("ERROR: File does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (inode->num_accessed > 0) {
        printk("ERROR: File is currently open or Directory is currently accessed\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (inode->type == FILE_TYPE_UNUSED) {
        printk("ERROR: Not a file or directory\n");
        __asm__ volatile ("sti");
        return -1;
    }
    // deallocate all data blocks
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (inode->location.direct_pointers[i] != NULL) {
            fs_deallocate_block(inode->location.direct_pointers[i]);
        }
    }
    // handle indirect pointers
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        if (inode->location.indirect_pointer[i] != NULL) {
            void** indirect_block = (void**)inode->location.indirect_pointer[i];
            for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                if (indirect_block[j] != NULL) {
                    fs_deallocate_block(indirect_block[j]);
                }
            }
            fs_deallocate_block(inode->location.indirect_pointer[i]);
        }
    }
    // handle double indirect pointers
    for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
        if (inode->location.double_indirect_pointer[i] != NULL) {
            void*** double_indirect_block = (void***)inode->location.double_indirect_pointer[i];
            for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                if (double_indirect_block[j] != NULL) {
                    void** indirect_block = (void**)double_indirect_block[j];
                    for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                        if (indirect_block[k] != NULL) {
                            fs_deallocate_block(indirect_block[k]);
                        }
                    }
                    fs_deallocate_block(double_indirect_block[j]);
                }
            }
            fs_deallocate_block(inode->location.double_indirect_pointer[i]);
        }
    }
    memset(inode, 0, sizeof(inode_t));
    inode->type = FILE_TYPE_UNUSED;

    
    // remove entry from parent directory by swaping with last entry
    dir_entry_t* last_entry = NULL;
    int found  = 0;
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (parent_dir->location.direct_pointers[i] == NULL) {
            found = 1;
            break;
        }
        dir_entry_t* entries = (dir_entry_t*)parent_dir->location.direct_pointers[i];
        for (int j = 0; j < ENTRIES_PER_DIR_BLOCK; j++) {
            if (entries[j].filename[0] == '\0') {
                found = 1;
                break;
            }
            last_entry = &entries[j];
        }
        if (found) {
            break;
        }
    }
    if (!found) {
        // check indirect pointers
        for (int i = 0; i < N_INDIRECT_POINTERS; i++)
        {
            if (parent_dir->location.indirect_pointer[i] == NULL) {
                found = 1;
                break; // no indirect blocks
            }
            void** indirect_block = (void**)parent_dir->location.indirect_pointer[i];
            for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                if (indirect_block[j] == NULL) {
                    found = 1;
                    break; // no data blocks
                }
                dir_entry_t* entries = (dir_entry_t*)indirect_block[j];
                uint32_t entry_idx = 0;
                while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                    last_entry = &entries[entry_idx];
                    entry_idx++;
                }
                if (entries[entry_idx].filename[0] == '\0') {
                    found = 1;
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
    if (!found) {
        // check double indirect pointers
        for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
            if (parent_dir->location.double_indirect_pointer[i] == NULL) {
                found = 1;
                break; // no double indirect blocks
            }
            void*** double_indirect_block = (void***)parent_dir->location.double_indirect_pointer[i];
            for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                if (double_indirect_block[j] == NULL) {
                    found = 1;
                    break; // no indirect blocks
                }
                void** indirect_block = (void**)double_indirect_block[j];
                for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                    if (indirect_block[k] == NULL) {
                        found = 1;
                        break; // no data blocks
                    }
                    dir_entry_t* entries = (dir_entry_t*)indirect_block[k];
                    uint32_t entry_idx = 0;
                    while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                        last_entry = &entries[entry_idx];
                        entry_idx++;
                    }
                    if (entries[entry_idx].filename[0] == '\0') {
                        found = 1;
                        break;
                    }
                }
                if (found) {
                    break;
                }
            }
            if (found) {
                break;
            }
        }
    }
    strncpy(dir_entry->filename, last_entry->filename, MAX_FILENAME_LEN); //TODO: this needs to be fixed, read_dir_entry does not lock the block
    dir_entry->inode_index = last_entry->inode_index;
    memset(last_entry, 0, sizeof(dir_entry_t));
    parent_dir->num_accessed--;
    parent_dir->size -= sizeof(dir_entry_t);
    __asm__ volatile ("sti");
    return 0;
}

int fs_readdir(int fd, char *buf) {
    
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        printk("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        printk("ERROR: Invalid buffer\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        printk("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (fd_entry->file_offset%sizeof(dir_entry_t) != 0) {
        printk("ERROR: Directory offset is not aligned with directory entry");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type != FILE_TYPE_DIRECTORY) {
        printk("ERROR: Not a directory\n");
        __asm__ volatile ("sti");
        return -1;
    }int read_ret = fs_read_helper(fd, buf, sizeof(dir_entry_t), 0);
    if (read_ret != sizeof(dir_entry_t)) {
        if (read_ret == -1) {
            printk("ERROR: fs_read errored\n");
            __asm__ volatile ("sti");
            return -1;
        }
        printk("ERROR: Could not read full directory entry\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if(fd_entry->file_offset >= inode->size) {
        __asm__ volatile ("sti");
        return 0;
    }
    else {
        __asm__ volatile ("sti");
        return 1;
    }
}
    */