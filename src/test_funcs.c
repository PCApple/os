#include "include/test_funcs.h"
#include "include/mem.h"


void test_write_final(void *args) {
    test_args_t *targs = (test_args_t *)args;
    if (targs == NULL) return;
    uint32_t tid = scheduler.current->tid;
    char_bufs_t *bufs =
        (char_bufs_t *)mem_kalloc(PAGE_SIZE); // tid = 0; period = 1; c = 2;
    bufs->zero = 0;
    itoa(bufs[0].buf, 'd', tid);
    itoa(bufs[1].buf, 'd', scheduler.current->t);
    itoa(bufs[2].buf, 'd', scheduler.current->c);
    terminal_writestring("<");
    terminal_writestring(bufs[0].buf);
    terminal_writestring(",");
    terminal_writestring(bufs[2].buf);
    terminal_writestring(",");
    terminal_writestring(bufs[1].buf);
    terminal_writestring(">|||");
    uint32_t cnt = targs->secs * pit_frequency; // total time slices to run
    uint32_t period_cnt = scheduler.current->period_cnt;
    while (period_cnt <= cnt) { // want to print cnt times
        period_cnt = scheduler.current->period_cnt;
    }
    __asm__ volatile("cli");
    terminal_writestring("TID ");
    terminal_writestring(bufs[0].buf);
    terminal_writestring(" L:");
    itoa(bufs[0].buf, 'd', scheduler.current->period_cnt);
    terminal_writestring(bufs[0].buf);
    terminal_writestring(" slices|||");
    __asm__ volatile("sti");
    mem_kfree(bufs);
}
void test_simple_fs(void *args) {
    int sfFD = fs_open("/SmallFile1");
    if (sfFD < 0) {
        terminal_writestring("Failed to open /SmallFile1\n");
        return;
    }
    terminal_writestring("/SmallFile1 opened successfully\n");
    char* write_buf1 = mem_kalloc(PAGE_SIZE);
    for (int i = 0; i < 100; i++) {
        write_buf1[i] = 'a' + (i % 26);
    }
    fs_write(sfFD, write_buf1, 26);
    fs_lseek(sfFD, 0);
    char* read_buf1 = mem_kalloc(PAGE_SIZE);
    memset(read_buf1, 0, 100);
    int read_ret = fs_read(sfFD, read_buf1, 26);
    if (read_ret < 0) {
        terminal_writestring("Failed to read from /SmallFile1\n");
        fs_close(sfFD);
        mem_kfree(write_buf1);
        mem_kfree(read_buf1);
        return;
    }
    __asm__ volatile("cli");
    terminal_writestring(read_buf1);
    terminal_writestring("\n");
    __asm__ volatile("sti");
    fs_close(sfFD);
    mem_kfree(write_buf1);
    mem_kfree(read_buf1);
}
void test_big_fs(void *args) {
    dir_entry_t ret_dir_entry;
    char path_buf[32];
    char int_buf[8];
    // --------------------first max out files (TEST 1)--------------------//
    for (int i = 0; i < MAX_INODES-1; i++) {
        
        strncpy(path_buf, "/SmallFile", strlen("/SmallFile"));
        itoa(int_buf, 'd', i); // start from 0
        strncpy(path_buf + strlen("/SmallFile"), int_buf, sizeof(path_buf) - strlen("/SmallFile") - 1);
        int ret_val = fs_create(path_buf);
        if (ret_val < 0) {
            terminal_writestring("Failed to create ");
            terminal_writestring(path_buf);
            terminal_writestring("\n");
            return;
        }
    }
    terminal_writestring("Created maximum number of files(1023) successfully\n");
    // now delete all files
    terminal_writestring("unlinking files\n");
    for (int i = 0; i < MAX_INODES-1; i++) {
        char path_buf[32];
        char int_buf[8];
        strncpy(path_buf, "/SmallFile", strlen("/SmallFile"));
        itoa(int_buf, 'd', i); // start from 0
        strncpy(path_buf + strlen("/SmallFile"), int_buf, sizeof(path_buf) - strlen("/SmallFile") - 1);
        int ret_val = fs_unlink(path_buf);
        if (ret_val < 0) {
            terminal_writestring("Failed to delete ");
            terminal_writestring(path_buf);
            terminal_writestring("\n");
            return;
        }
    }
    terminal_writestring("Deleted all files successfully\n");
    //--------------------Now try to max out one file (TEST 2)--------------------//
    int bigFileFD = fs_create("/BigFile");
    if (bigFileFD < 0) {
        terminal_writestring("Failed to create /BigFile\n");
        return;
    }
    terminal_writestring("Created /BigFile successfully\n");
    bigFileFD = fs_open("/BigFile");
    if (bigFileFD < 0) {
        terminal_writestring("Failed to open /BigFile\n");
        return;
    }
    terminal_writestring("Opened /BigFile successfully\n");
    void* ones = mem_kalloc(PAGE_SIZE);
    memset(ones, '1', BLOCK_SIZE);
    void* twos = mem_kalloc(PAGE_SIZE);
    memset(twos, '2', BLOCK_SIZE);
    void* threes = mem_kalloc(PAGE_SIZE);
    memset(threes, '3', BLOCK_SIZE);
    // write direct blocks
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        int write_ret = fs_write(bigFileFD, ones, BLOCK_SIZE);
        if (write_ret != BLOCK_SIZE) {
            terminal_writestring("Failed to write direct blocks to /BigFile\n");
            fs_close(bigFileFD);
            mem_kfree(ones);
            mem_kfree(twos);
            mem_kfree(threes);
            return;
        }
    }
    terminal_writestring("Wrote direct blocks to /BigFile successfully\n");
    // write single indirect blocks
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            int write_ret = fs_write(bigFileFD, twos, BLOCK_SIZE);
            if (write_ret != BLOCK_SIZE) {
                terminal_writestring("Failed to write single indirect blocks to /BigFile\n");
                fs_close(bigFileFD);
                mem_kfree(ones);
                mem_kfree(twos);
                mem_kfree(threes);
                return;
            }
        }
    }

    terminal_writestring("Wrote single indirect blocks to /BigFile successfully\n");
    // write double indirect blocks
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                int write_ret = fs_write(bigFileFD, threes, BLOCK_SIZE);
                if (write_ret == -1) {
                    int cnt = i * DIRECT_BLOCKS_PER_INDIRECT_BLOCK * DIRECT_BLOCKS_PER_INDIRECT_BLOCK +
                              j * DIRECT_BLOCKS_PER_INDIRECT_BLOCK + k;

                    terminal_writestring("Failed to write double indirect blocks to /BigFile\n");
                    terminal_writestring("Failed at [");
                    itoa(int_buf, 'd', cnt);
                    terminal_writestring(int_buf);
                    terminal_writestring("] blocks\n");
                    terminal_writestring("Error code: ");
                    itoa(int_buf, 'd', write_ret);
                    terminal_writestring(int_buf);
                    terminal_writestring("\n");                    
                    fs_close(bigFileFD);
                    mem_kfree(ones);
                    mem_kfree(twos);
                    mem_kfree(threes);
                    return;
                }
            }
        }
    }
    terminal_writestring("Wrote double indirect blocks to /BigFile successfully\n");
    //--------------------Now read back and verify (TEST 3)--------------------//
    int ret_lseek = fs_lseek(bigFileFD, 0);
    if (ret_lseek < 0) {
        terminal_writestring("Failed to lseek to beginning of /BigFile\n");
        fs_close(bigFileFD);
        mem_kfree(ones);
        mem_kfree(twos);
        mem_kfree(threes);
        return;
    }
    terminal_writestring("Lseeked to beginning of /BigFile successfully\n");
    // read and verify direct blocks
    void* read_buf = mem_kalloc(0);
    for (int i = 0; i < N_DIRECT_POINTERS; i++) {
        int read_ret = fs_read(bigFileFD, read_buf, BLOCK_SIZE);
        if (read_ret == -1 || memcmp(read_buf, ones, BLOCK_SIZE) != 0) {
            terminal_writestring("Failed to read/verify direct blocks from /BigFile\n");
            fs_close(bigFileFD);
            mem_kfree(ones);
            mem_kfree(twos);
            mem_kfree(threes);
            mem_kfree(read_buf);
            return;
        }
    }
    terminal_writestring("Read/verified direct blocks from /BigFile successfully\n");
    // read and verify single indirect blocks
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            int read_ret = fs_read(bigFileFD, read_buf, BLOCK_SIZE);
            if (read_ret == -1 || memcmp(read_buf, twos, BLOCK_SIZE) != 0) {
                terminal_writestring("Failed to read/verify single indirect blocks from /BigFile\n");
                fs_close(bigFileFD);
                mem_kfree(ones);
                mem_kfree(twos);
                mem_kfree(threes);
                mem_kfree(read_buf);
                return;
            }
        }
    }
    terminal_writestring("Read/verified single indirect blocks from /BigFile successfully\n");
    // read and verify double indirect blocks
    for (int i = 0; i < N_INDIRECT_POINTERS; i++) {
        for (int j = 0; j < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; j++) {
            for (int k = 0; k < DIRECT_BLOCKS_PER_INDIRECT_BLOCK; k++) {
                int read_ret = fs_read(bigFileFD, read_buf, BLOCK_SIZE);
                if (read_ret == -1 || memcmp(read_buf, threes, BLOCK_SIZE) != 0) {
                    terminal_writestring("Failed to read/verify double indirect blocks from /BigFile\n");
                    fs_close(bigFileFD);
                    mem_kfree(ones);
                    mem_kfree(twos);
                    mem_kfree(threes);
                    mem_kfree(read_buf);
                    return;
                }
            }
        }
    }
    terminal_writestring("Read/verified double indirect blocks from /BigFile successfully\n");
    // cleanup
    fs_close(bigFileFD);
    mem_kfree(ones);
    mem_kfree(twos);
    mem_kfree(threes);
    mem_kfree(read_buf);
    fs_unlink("/BigFile");
    terminal_writestring("Deleted /BigFile successfully\n");
    //--------------------Finally, test readdir  and dirs(TEST 4)--------------------//
    // first create some files
    int ret_create1 = fs_create("/File1");
    int ret_create2 = fs_create("/File2");
    int ret_create3 = fs_create("/File3");
    if (ret_create1 < 0 || ret_create2 < 0 || ret_create3 < 0) {
        terminal_writestring("Failed to create files 1-3 for readdir test\n");
        return;
    }
    //now a directory
    int ret_mkdir = fs_mkdir("/dir1/");
    if (ret_mkdir < 0) {
        terminal_writestring("Failed to create directory /dir1 for readdir test\n");
        return;
    }
    int ret_mkdir2 = fs_mkdir("/dir1/dir2/");
    if (ret_mkdir2 < 0) {
        terminal_writestring("Failed to create directory /dir1/dir2 for readdir test\n");
        return;
    }
    int ret_mkdir3 = fs_mkdir("/dir1/dir3/");
    if (ret_mkdir3 < 0) {
        terminal_writestring("Failed to create directory /dir1/dir3 for readdir test\n");
        return;
    }
    terminal_writestring("Created directories for readdir test successfully\n");
    // subfile in dir1
    int ret_create4 = fs_create("/dir1/SubFile1");
    if (ret_create4 < 0) {
        terminal_writestring("Failed to create /dir1/SubFile1 for readdir test\n");
        return;
    }
    int ret_create5 = fs_create("/dir1/dir2/SubFile2");
    if (ret_create5 < 0) {
        terminal_writestring("Failed to create /dir1/dir2/SubFile2 for readdir test\n");
        return;
    }
    int ret_create6 = fs_create("/dir1/dir3/SubFile3");
    if (ret_create6 < 0) {
        terminal_writestring("Failed to create /dir1/dir3/SubFile3 for readdir test\n");
        return;
    }
    terminal_writestring("Created files for readdir test successfully\n");
    // now for readdir
    terminal_writestring("reading root: ");
    int rootFD = fs_open("/");
    //terminal_writestring("Opened root directory successfully\n");
    for (int i = 0; i < MAX_INODES-1; i++) {
        memset(&ret_dir_entry, 0, sizeof(dir_entry_t));
        int read_ret = fs_readdir(rootFD, (char*)&ret_dir_entry);
        if (read_ret < 0) {
            terminal_writestring("Failed to read directory entry ");
            memset(int_buf, 0, 8);
            itoa(int_buf, 'd', i);
            terminal_writestring(int_buf);
            terminal_writestring("|");
            fs_close(rootFD);
            return;
        }
        memset(int_buf, 0, 8);
        memset(path_buf, 0, 32);
        itoa(int_buf, 'd', ret_dir_entry.inode_index);
        strncpy(path_buf, ret_dir_entry.filename, MAX_FILENAME_LEN);
        terminal_writestring("[");
        terminal_writestring(path_buf);
        terminal_writestring(",");
        terminal_writestring(int_buf);
        terminal_writestring("] ");
        if (read_ret == 0) {
            //terminal_writestring("No more directory entries to read\n");
            break;
        }
    }
    fs_close(rootFD);
    terminal_writestring("\n");
    terminal_writestring("reading /dir1/: ");
    int dir1FD = fs_open("/dir1/");
    for (int i = 0; i < MAX_INODES-1; i++) {
        memset(&ret_dir_entry, 0, sizeof(dir_entry_t));
        int read_ret = fs_readdir(dir1FD, (char*)&ret_dir_entry);
        if (read_ret < 0) {
            terminal_writestring("Failed to read directory entry ");
            memset(int_buf, 0, 8);
            itoa(int_buf, 'd', i);
            terminal_writestring(int_buf);
            terminal_writestring("|");
            fs_close(dir1FD);
            return;
        }
        memset(int_buf, 0, 8);
        memset(path_buf, 0, 32);
        itoa(int_buf, 'd', ret_dir_entry.inode_index);
        strncpy(path_buf, ret_dir_entry.filename, MAX_FILENAME_LEN);
        terminal_writestring("[");
        terminal_writestring(path_buf);
        terminal_writestring(",");
        terminal_writestring(int_buf);
        terminal_writestring("] ");
        if (read_ret == 0) {
            //terminal_writestring("No more directory entries to read\n");
            break;
        }
    }
    fs_close(dir1FD);
    terminal_writestring("\n");
    int dir2FD = fs_open("/dir1/dir2/");
    terminal_writestring("Reading /dir1/dir2/: ");
    for (int i = 0; i < MAX_INODES-1; i++) {
        memset(&ret_dir_entry, 0, sizeof(dir_entry_t));
        int read_ret = fs_readdir(dir2FD, (char*)&ret_dir_entry);
        if (read_ret < 0) {
            terminal_writestring("Failed to read directory entry ");
            memset(int_buf, 0, 8);
            itoa(int_buf, 'd', i);
            terminal_writestring(int_buf);
            terminal_writestring("|");
            fs_close(dir2FD);
            return;
        }
        memset(int_buf, 0, 8);
        memset(path_buf, 0, 32);
        itoa(int_buf, 'd', ret_dir_entry.inode_index);
        strncpy(path_buf, ret_dir_entry.filename, MAX_FILENAME_LEN);
        terminal_writestring("[");
        terminal_writestring(path_buf);
        terminal_writestring(",");
        terminal_writestring(int_buf);
        terminal_writestring("] ");
        if (read_ret == 0) {
            //terminal_writestring("No more directory entries to read\n");
            break;
        }
    }
    fs_close(dir2FD);
    terminal_writestring("\n");
    terminal_writestring("Reading /dir1/dir3/: ");
    int dir3FD = fs_open("/dir1/dir3/");
    for (int i = 0; i < MAX_INODES-1; i++) {
        memset(&ret_dir_entry, 0, sizeof(dir_entry_t));
        int read_ret = fs_readdir(dir3FD, (char*)&ret_dir_entry);
        if (read_ret < 0) {
            terminal_writestring("Failed to read directory entry ");
            memset(int_buf, 0, 8);
            itoa(int_buf, 'd', i);
            terminal_writestring(int_buf);
            terminal_writestring("|");
            fs_close(dir3FD);
            return;
        }
        memset(int_buf, 0, 8);
        memset(path_buf, 0, 32);
        itoa(int_buf, 'd', ret_dir_entry.inode_index);
        strncpy(path_buf, ret_dir_entry.filename, MAX_FILENAME_LEN);
        terminal_writestring("[");
        terminal_writestring(path_buf);
        terminal_writestring(",");
        terminal_writestring(int_buf);
        terminal_writestring("] ");
        if (read_ret == 0) {
            //terminal_writestring("No more directory entries to read\n");
            break;
        }
    }
    terminal_writestring("\n");
    terminal_writestring("Completed readdir tests successfully\n");
    fs_close(dir3FD);
    terminal_writestring("ALL TESTS PASSED");
}
void test_multi_fs(void *args) {
    int tid = scheduler.current->tid;
    int i = 0;
    if (tid%2 != 0){
        i++;
    }
    for (i = i; i < 600; i += 2){ // each thread should create 300 files
        char path_buf[32];
        char int_buf[8];
        strncpy(path_buf, "/TFile", strlen("/TFile"));
        itoa(int_buf, 'd', i); // unique file names per thread
        strncpy(path_buf + strlen("/TFile"), int_buf, sizeof(path_buf) - strlen("/TFile") - 1);
        int ret_val = fs_create(path_buf);
        if (ret_val < 0) {
            terminal_writestring("TID ");
            itoa(int_buf, 'd', tid);
            terminal_writestring(int_buf);
            terminal_writestring(" Failed to create ");
            terminal_writestring(path_buf);
            terminal_writestring("\n");
            return;
        }
    }
    terminal_writestring("TID ");
    char int_buf[8];
    itoa(int_buf, 'd', tid);
    terminal_writestring(int_buf);
    terminal_writestring(" Created 300 files successfully\n");
}
