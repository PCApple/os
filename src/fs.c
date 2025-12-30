#include "include/fs.h"


static void* fs_ptr; // pointer to the start of the filesystem in memory

static fd_table_t fd_table[MAX_NUM_THREADS]; // global file descriptor table


// -------------------------------- internal helper functions --------------------------------//
int ceiling(int numerator, int denominator) {
    if (numerator % denominator == 0) {
        return numerator / denominator;
    }
    else{
        return (numerator / denominator) + 1;
    }
}
/*
    returns a pointer to the a free data block and marks it as used in the block bitmap and decrements n_free_blocks in the superblock
    returns NULL if no free block is available
*/

void* allocate_block(){
    if (((superblock_t*)fs_ptr)->n_free_blocks <= 0) {
        return NULL; // no free blocks
    }
    block_bitmap_t* bb = (block_bitmap_t*)((uint8_t*)fs_ptr + BLOCK_SIZE + INODE_BLOCKS * BLOCK_SIZE);
    superblock_t* sb = (superblock_t*)fs_ptr;
    for (uint32_t byte_idx = 0; byte_idx < sizeof(bb->bitmap); byte_idx++) {
        if (bb->bitmap[byte_idx] != 0xFF) {
            for (uint8_t bit_idx = 0; bit_idx < 8; bit_idx++) {
                if (!(bb->bitmap[byte_idx] & (1 << bit_idx))) {
                    bb->bitmap[byte_idx] |= (1 << bit_idx); // mark it as used
                    uint32_t block_num = byte_idx * 8 + bit_idx;
                    sb->n_free_blocks--;
                    char* block_addr = (char*)fs_ptr + BLOCK_SIZE + INODE_BLOCKS * BLOCK_SIZE + BLOCK_BITMAP_BLOCKS * BLOCK_SIZE + block_num * BLOCK_SIZE;
                    memset(block_addr, 0, BLOCK_SIZE); // zero out the block
                    return (void*)  block_addr;
                }
            }
        }
    }
    return NULL; // no free block found
}
int deallocate_block(void* block_addr) {
    superblock_t* sb = (superblock_t*)fs_ptr;
    block_bitmap_t* bb = sb->block_bitmap_start;
    uint32_t block_num = (block_addr - sb->data_blocks_start) / BLOCK_SIZE;
    uint32_t byte_idx = block_num / 8;
    uint8_t bit_idx = block_num % 8;
    uint8_t bits = bb->bitmap[byte_idx];
    if (bits & (1 << bit_idx)) {
        bb->bitmap[byte_idx] &= ~(1 << bit_idx); // mark it as free
        sb->n_free_blocks++;
        memset(block_addr, 0, BLOCK_SIZE);
        return 0;
    } else {
        return -1; // block was already free
    }
}
dir_entry_t* find_dir_entry(inode_t* dir_inode, const char* filename) {
    if (dir_inode->type != FILE_TYPE_DIRECTORY) {
        return NULL; // not a directory
    }
    // check direct pointers
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (dir_inode->location.direct_pointers[i] == NULL) {
            return NULL; // no data blocks
        }
        dir_entry_t* entries = (dir_entry_t*)dir_inode->location.direct_pointers[i]; // assuming single block for simplicity
        uint32_t entry_idx = 0;
        while (entries[entry_idx].filename[0] != '\0') {
            if (strcmp(entries[entry_idx].filename, filename) == 0) {
                return &entries[entry_idx];
            }
            entry_idx++;
        }
    
    }
    // check indirect pointers
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        if (dir_inode->location.indirect_pointer[i] == NULL) {
            return NULL; // no indirect blocks
        }
        void** indirect_block = (void**)dir_inode->location.indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (indirect_block[j] == NULL) {
                return NULL; // no data blocks
            }
            dir_entry_t* entries = (dir_entry_t*)indirect_block[j];
            uint32_t entry_idx = 0;
            while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                if (strcmp(entries[entry_idx].filename, filename) == 0) {
                    return &entries[entry_idx];
                }
                entry_idx++;
            }
        }
    }
    // check double indirect pointers
    for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
        if (dir_inode->location.double_indirect_pointer[i] == NULL) {
            return NULL; // no double indirect blocks
        }
        void*** double_indirect_block = (void***)dir_inode->location.double_indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (double_indirect_block[j] == NULL) {
                return NULL; // no indirect blocks
            }
            void** indirect_block = (void**)double_indirect_block[j];
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                if (indirect_block[k] == NULL) {
                    return NULL; // no data blocks
                }
                dir_entry_t* entries = (dir_entry_t*)indirect_block[k];
                uint32_t entry_idx = 0;
                while (entries[entry_idx].filename[0] != '\0' || entry_idx < ENTRIES_PER_DIR_BLOCK) {
                    if (strcmp(entries[entry_idx].filename, filename) == 0) {
                        return &entries[entry_idx];
                    }
                    entry_idx++;
                }
            }
        }
    }
    return NULL; // not found
}
inode_t* get_parent_dir_inode(const char* path){
    superblock_t* sb = (superblock_t*)fs_ptr;
    uint32_t len = strlen(path);
    uint32_t path_idx = 1;
    inode_t* curr_dir = &((inode_t*)(sb->inodes_start))[sb->root_inode_index];
    inode_t* parent_dir = NULL;
    while (path_idx < strlen(path)) {
        if (curr_dir->type != FILE_TYPE_DIRECTORY) {
            return NULL; // not a directory
        }
        char name[MAX_FILENAME_LEN + 1];
        uint32_t name_idx = 0;
        while (path[path_idx] != '/' && path[path_idx] != '\0' && name_idx < MAX_FILENAME_LEN) {
            name[name_idx++] = path[path_idx++];
        }
        if (path_idx == len || (path[path_idx] == '/' && path_idx + 1 == len)) {
            return curr_dir; // reached the target file/directory
        }
        name[name_idx] = '\0';
        // search for name in curr_dir
        dir_entry_t* dir_entry = find_dir_entry(curr_dir, name);
        parent_dir = curr_dir;
        curr_dir = &((inode_t*)(sb->inodes_start))[dir_entry->inode_index];
        if (path[path_idx] == '/') {
            path_idx++;
        }
    }
    return NULL; // should not reach here probably
}
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

int expand_file_size(inode_t* inode, uint32_t new_size) {
        if (new_size <= inode->size) {
            return 0; // no need to expand
        }
        if (new_size < 1) {
            terminal_writestring("ERROR: File size is 0\n");
            return -1;
        }
        int remaining_blocks = ceiling(new_size, BLOCK_SIZE);
        for (int i = 0; i < N_DIRECT_POINTERS; i++) {
            if (inode->location.direct_pointers[i] == NULL) {
                void* new_block = allocate_block();
                if (new_block == NULL) {
                    terminal_writestring("ERROR: Failed to allocate data block\n");
                    return -1;
                }
                inode->location.direct_pointers[i] = new_block;
            }
            remaining_blocks--;
            if (remaining_blocks == 0) {
                inode->size = new_size; 
                return 0;
            }
        }
        if (remaining_blocks >0) { // doing one level of indirection
            // handle indirect pointers
            for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
                if (inode->location.indirect_pointer[i] == NULL) {
                    inode->location.indirect_pointer[i] = allocate_block();
                    if (inode->location.indirect_pointer[i] == NULL) {
                        terminal_writestring("ERROR: Failed to allocate indirect block\n");
                        return -1;
                    }
                }
                void** indirect_block = (void**)inode->location.indirect_pointer[i];
                for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                    if (indirect_block[j] == NULL) {
                        void* new_block = allocate_block();
                        if (new_block == NULL) {
                            terminal_writestring("ERROR: Failed to allocate data block\n");
                            return -1;
                        }
                        indirect_block[j] = new_block;
                    }
                    remaining_blocks--;
                    if (remaining_blocks == 0) {
                        inode->size = new_size; 
                        return 0;
                    }
                }                
            }
        }
        if (remaining_blocks > 0) { // doing double indirection
            for (int i = 0; i < N_DOUBLE_INDIRECT_POINTERS; i++) {
                if (inode->location.double_indirect_pointer[i] == NULL) {
                    inode->location.double_indirect_pointer[i] = allocate_block();
                    if (inode->location.double_indirect_pointer[i] == NULL) {
                        terminal_writestring("ERROR: Failed to allocate double indirect block\n");
                        return -1;
                    }
                }
                void*** double_indirect_block = (void***)inode->location.double_indirect_pointer[i];
                for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                    if (double_indirect_block[j] == NULL) {
                        double_indirect_block[j] = allocate_block();
                        if (double_indirect_block[j] == NULL) {
                            terminal_writestring("ERROR: Failed to allocate indirect block\n");
                            return -1;
                        }
                    }
                    void** indirect_block = (void**)double_indirect_block[j];
                    for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                        if (indirect_block[k] == NULL) {
                            void* new_block = allocate_block();
                            if (new_block == NULL) {
                                terminal_writestring("ERROR: Failed to allocate data block\n");
                                return -1;
                            }
                            indirect_block[k] = new_block;
                        }
                        remaining_blocks--;
                        if (remaining_blocks == 0) {
                            inode->size = new_size; 
                            return 0;
                        }
                    }
                }
            }
        }
        if (remaining_blocks > 0) {
            terminal_writestring("ERROR: File size exceeds maximum limit\n");
            return -1;
        }
            
    return 0;
}
int create_or_mkdir(const char* path) {
    superblock_t* sb = (superblock_t*)fs_ptr;
    if (path == NULL || path[0] != '/') {
        terminal_writestring("ERROR: Invalid path\n");
        return -1;
    }
    uint8_t is_dir = 0;
    if (path[strlen(path)-1] == '/') {
        is_dir = 1;
    }
    inode_t* parent_dir = get_parent_dir_inode(path);
    if (parent_dir == NULL) {
        terminal_writestring("ERROR: Parent directory does not exist\n");
        return -1;
    }
    // check if file/dir already exists
    char name[MAX_FILENAME_LEN + 1];  
    read_filename(path, name);
    dir_entry_t* dir_entry = find_dir_entry(parent_dir, name);
    if (dir_entry != NULL) {
        terminal_writestring("ERROR: File/Directory already exists\n");
        return -2;
    }
    // allocate new inode
    inode_t* inodes = (inode_t*)sb->inodes_start;
    uint32_t new_inode_idx = -1;
    for (int i = 0; i < MAX_INODES; i++)
    {
        if (inodes[i].type == FILE_TYPE_UNUSED) {
            new_inode_idx = i;
            break;
        }
    }
    if (new_inode_idx == -1) {
        terminal_writestring("ERROR: No free inodes available\n");
        return -1;
    }
    inode_t* new_inode = &inodes[new_inode_idx];
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
    if (is_dir) { // need to setup directory block
        new_inode->location.direct_pointers[0] = allocate_block();
        if (new_inode->location.direct_pointers[0] == NULL) {
            terminal_writestring("ERROR: Failed to allocate block for new directory\n");
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
            parent_dir->location.direct_pointers[i] = allocate_block(); // allocate new block for directory entries
            if (parent_dir->location.direct_pointers[i] == NULL) {
                terminal_writestring("ERROR: Failed to allocate block for parent directory entries\n");
                return -1;
            }
        }
        dir_entry_t* entries = (dir_entry_t*)parent_dir->location.direct_pointers[i];
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
            parent_dir->location.indirect_pointer[i] = allocate_block();
            if (parent_dir->location.indirect_pointer[i] == NULL) {
                terminal_writestring("ERROR: Failed to allocate indirect block for parent directory entries\n");
                return -1;
            }
        }
        void** direct_blocks = (void**)parent_dir->location.indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (direct_blocks[j] == NULL) {
                direct_blocks[j] = allocate_block();
                if (direct_blocks[j] == NULL) {
                    terminal_writestring("ERROR: Failed to allocate block for parent directory entries\n");
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
            parent_dir->location.double_indirect_pointer[i] = allocate_block();
            if (parent_dir->location.double_indirect_pointer[i] == NULL) {
                terminal_writestring("ERROR: Failed to allocate double indirect block for parent directory entries\n");
                return -1;
            }
        }
        void*** indirect_blocks = (void***)parent_dir->location.double_indirect_pointer[i];
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            if (indirect_blocks[j] == NULL) {
                indirect_blocks[j] = allocate_block();
                if (indirect_blocks[j] == NULL) {
                    terminal_writestring("ERROR: Failed to allocate indirect block for parent directory entries\n");
                    return -1;
                }
            }
            void** direct_blocks = (void**)indirect_blocks[j];
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                if (direct_blocks[k] == NULL) {
                    direct_blocks[k] = allocate_block();
                    if (direct_blocks[k] == NULL) {
                        terminal_writestring("ERROR: Failed to allocate block for parent directory entries\n");
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
int fs_init(void* fs_start, uint32_t fs_size) {
    // 
    __asm__ volatile ("cli");
    fs_ptr = fs_start;
    if (fs_start == NULL || fs_size != FS_SIZE) {
        __asm__ volatile ("sti");
        return -1;
    }

    superblock_t* sb = (superblock_t*)fs_start;
    sb->n_free_blocks = DATA_BLOCKS;
    sb->n_free_inodes = MAX_INODES;
    // Initialize inodes
    inode_t* inodes = (inode_t*)((uint8_t*)fs_start + BLOCK_SIZE); // inodes start after superblock
    for (uint32_t i = 0; i < MAX_INODES; i++)
    {
        inodes[i].type = FILE_TYPE_UNUSED; // default type
        inodes[i].size = 0;
        for (int j = 0; j < N_DIRECT_POINTERS; j++) {
            inodes[i].location.direct_pointers[j] = 0;
        }
        for (int j = 0; j < N_INDIRECT_POINTERS; j++) {
            inodes[i].location.indirect_pointer[j] = 0;
        }
        for (int j = 0; j < N_DOUBLE_INDIRECT_POINTERS; j++) {
            inodes[i].location.double_indirect_pointer[j] = 0;
        }
    }
    // Initialize block bitmap
    block_bitmap_t* bb = (block_bitmap_t*)((uint8_t*)inodes + INODE_BLOCKS * BLOCK_SIZE);
    memset(bb->bitmap, 0, sizeof(bb->bitmap));
    // setup root directory
    inodes[0].type = FILE_TYPE_DIRECTORY;
    inodes[0].idx = 0;
    inodes[0].size = 0;
    inodes[0].location.direct_pointers[0] = allocate_block();
    if (inodes[0].location.direct_pointers[0] == NULL) {
        __asm__ volatile ("sti");
        return -1; // failed to allocate block for root directory
    }
    inodes[0].size += sizeof(dir_entry_t);
    sb->n_free_inodes--;
    dir_entry_t* root_dir = (dir_entry_t*)inodes[0].location.direct_pointers[0];
    root_dir[0].filename[0] = '.';
    root_dir[0].inode_index = 0;
    root_dir[0].filename[1] = '\0'; // end of entries
    sb->root_inode_index = 0;
    sb->inodes_start = (void*)inodes;
    sb->block_bitmap_start = (void*)bb;
    sb->data_blocks_start = (void*)(sb->block_bitmap_start + BLOCK_BITMAP_BLOCKS * BLOCK_SIZE);
    // __asm__ volatile ("sti");
    return 0;

}


int fs_create(const char* path) {
    if (path == NULL) {
        return -1;
    }
    if (path[strlen(path)-1] == '/') {
        terminal_writestring("ERROR: Path ends with '/', use fs_mkdir for directories\n");
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
        terminal_writestring("ERROR: Path does not end with '/', use fs_create for files\n");
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
        terminal_writestring("ERROR: Invalid path\n");
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
            terminal_writestring("ERROR: Parent directory does not exist\n");
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
            terminal_writestring("ERROR: No free file descriptor available\n");
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
    dir_entry_t* dir_entry = find_dir_entry(parent_dir, name);
    if (dir_entry == NULL) {
        terminal_writestring("ERROR: File does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* file_inode = &((inode_t*)sb->inodes_start)[dir_entry->inode_index];
    if (file_inode == NULL) {
        terminal_writestring("ERROR: File does not exist\n");
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
        terminal_writestring("ERROR: No free file descriptor available\n");
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
        terminal_writestring("ERROR: Invalid file descriptor\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        terminal_writestring("ERROR: File descriptor not open\n");
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
        terminal_writestring("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        terminal_writestring("ERROR: Invalid buffer\n");
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
        terminal_writestring("ERROR: File descriptor not open\n");
        if (should_lock) {
            __asm__ volatile ("sti");
        }
        return -1;
    }
    if ((fd_entry->status_flags & 0x2) == 0) {
        terminal_writestring("ERROR: File descriptor not opened for reading\n");
        if (should_lock) {
            __asm__ volatile ("sti");
        }
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type == FILE_TYPE_UNUSED) {
        terminal_writestring("ERROR: Not a File or DIR type\n");
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
            terminal_writestring("ERROR: Data block not allocated\n");
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
        terminal_writestring("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        terminal_writestring("ERROR: Invalid buffer\n");
        return -1;
    }
    if (n_bytes == 0) {
        terminal_writestring("whyyyyyyy\n");
        return 0; // nothing to write
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        terminal_writestring("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if ((fd_entry->status_flags & 0x4) == 0) {
        terminal_writestring("ERROR: File descriptor not opened for writing\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type != FILE_TYPE_FILE) {
        terminal_writestring("ERROR: Not a File type\n");
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
            terminal_writestring("ERROR: Data block not allocated\n");
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
        terminal_writestring("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (offset < 0) {
        terminal_writestring("ERROR: Invalid offset\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        terminal_writestring("ERROR: File descriptor not open\n");
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
        terminal_writestring("ERROR: Invalid path\n");
        return -1;
    }
    if (strcmp(path, "/") == 0) {
        terminal_writestring("ERROR: Cannot delete root directory\n");
        return -1;
    }
    int is_dir = 0;
    if (path[strlen(path)-1] == '/') {
        is_dir = 1;
    }
    
    __asm__ volatile ("cli");
    inode_t* parent_dir = get_parent_dir_inode(path);
    if (parent_dir == NULL) {
        terminal_writestring("ERROR: Parent directory does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    char name[MAX_FILENAME_LEN + 1];
    read_filename(path, name);
    dir_entry_t* dir_entry = find_dir_entry(parent_dir, name);
    if (dir_entry == NULL) {
        terminal_writestring("ERROR: File/Directory does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = &((inode_t*)((superblock_t*)fs_ptr)->inodes_start)[dir_entry->inode_index];
    if (inode == NULL) {
        terminal_writestring("ERROR: File does not exist\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (inode->num_accessed > 0) {
        terminal_writestring("ERROR: File is currently open or Directory is currently accessed\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (inode->type == FILE_TYPE_UNUSED) {
        terminal_writestring("ERROR: Not a file or directory\n");
        __asm__ volatile ("sti");
        return -1;
    }
    // deallocate all data blocks
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        if (inode->location.direct_pointers[i] != NULL) {
            deallocate_block(inode->location.direct_pointers[i]);
        }
    }
    // handle indirect pointers
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        if (inode->location.indirect_pointer[i] != NULL) {
            void** indirect_block = (void**)inode->location.indirect_pointer[i];
            for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
                if (indirect_block[j] != NULL) {
                    deallocate_block(indirect_block[j]);
                }
            }
            deallocate_block(inode->location.indirect_pointer[i]);
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
                            deallocate_block(indirect_block[k]);
                        }
                    }
                    deallocate_block(double_indirect_block[j]);
                }
            }
            deallocate_block(inode->location.double_indirect_pointer[i]);
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
    strncpy(dir_entry->filename, last_entry->filename, MAX_FILENAME_LEN);
    dir_entry->inode_index = last_entry->inode_index;
    memset(last_entry, 0, sizeof(dir_entry_t));
    parent_dir->num_accessed--;
    parent_dir->size -= sizeof(dir_entry_t);
    __asm__ volatile ("sti");
    return 0;
}

int fs_readdir(int fd, char *buf) {
    
    if (fd < 0 || fd >= MAX_FD_ENTRIES) {
        terminal_writestring("ERROR: Invalid file descriptor\n");
        return -1;
    }
    if (buf == NULL) {
        terminal_writestring("ERROR: Invalid buffer\n");
        return -1;
    }
    
    __asm__ volatile ("cli");
    int tid = scheduler.current->tid;
    fd_table_entry_t* fd_entry = &fd_table[tid].entries[fd];
    if ((fd_entry->status_flags & 0x1) == 0) {
        terminal_writestring("ERROR: File descriptor not open\n");
        __asm__ volatile ("sti");
        return -1;
    }
    if (fd_entry->file_offset%sizeof(dir_entry_t) != 0) {
        terminal_writestring("ERROR: Directory offset is not aligned with directory entry");
        __asm__ volatile ("sti");
        return -1;
    }
    inode_t* inode = fd_entry->inode;
    if (inode->type != FILE_TYPE_DIRECTORY) {
        terminal_writestring("ERROR: Not a directory\n");
        __asm__ volatile ("sti");
        return -1;
    }int read_ret = fs_read_helper(fd, buf, sizeof(dir_entry_t), 0);
    if (read_ret != sizeof(dir_entry_t)) {
        if (read_ret == -1) {
            terminal_writestring("ERROR: fs_read errored\n");
            __asm__ volatile ("sti");
            return -1;
        }
        terminal_writestring("ERROR: Could not read full directory entry\n");
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