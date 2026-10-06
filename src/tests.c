
#include "include/tests.h"
#include "include/fs.h"
#include "include/disk.h"
#include "include/utils.h"

#define DISK_TEST_BLOCKS (CACHE_SIZE * 4 + 3)
static uint8_t saved_sectors[DISK_TEST_BLOCKS][BLOCK_SIZE];
static uint8_t sector_buffer[BLOCK_SIZE];
static uint32_t scratch_start;
static uint32_t CACHE_DRIVE = 1;

#define REQUIRE(condition) do { if (!(condition)) { \
    printk("FAIL line %d: %s\n", __LINE__, #condition); return 0; \
} } while (0)

/* Generate original or modified test bytes for a scratch sector and byte offset. */
static uint8_t disk_pattern(unsigned index, unsigned byte, unsigned changed) {
    return (uint8_t)((index * 37 + byte * 13 + byte / 256) ^ (changed ? 0xa5 : 0));
}

/* Fill an entire sector with the selected original or modified test pattern. */
static void fill_test_sector(uint8_t *data, unsigned index, unsigned changed) {
    for (unsigned j = 0; j < BLOCK_SIZE; ++j)
        data[j] = disk_pattern(index, j, changed);
}

/* Return true only if every byte matches the expected sector pattern. */
static int sector_matches(uint8_t *data, unsigned index, unsigned changed) {
    for (unsigned j = 0; j < BLOCK_SIZE; ++j)
        if (data[j] != disk_pattern(index, j, changed)) return 0;
    return 1;
}

/* Translate a scratch-sector index to its disk address and fetch it through the cache. */
static cache_block_t *get_sector(uint32_t scratch_start, unsigned index) {
    return cache_get(scratch_start + index);
}

/* Flush pending writes, fill all slots, then release them to leave an empty cache.
 * Reuses the existing allocation instead of calling cache_init again.
 */
static void drain_cache(uint32_t scratch_start) {
    cache_block_t *slots[CACHE_SIZE];
    cache_flush_all();
    for (unsigned i = 0; i < CACHE_SIZE; ++i) slots[i] = get_sector(scratch_start, i);
    for (unsigned i = 0; i < CACHE_SIZE; ++i)
        if (slots[i]) cache_release(slots[i]);
}

/* Empty the cache, seed all scratch sectors and verify the writes directly through IDE. */
static int prepare_disk(uint32_t scratch_start) {
    uint8_t sector_buffer[BLOCK_SIZE];
    drain_cache(scratch_start);
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i) {
        fill_test_sector(sector_buffer, i, 0);
        if ((disk_write(CACHE_DRIVE, scratch_start + i, sector_buffer) != DISK_OK) ||
            (disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) != DISK_OK) ||
            !sector_matches(sector_buffer, i, 0)) {
            printk("Disk preparation failed at scratch sector %d\n", i);
            return 0;
        }
    }
    return 1;
}

/* Check cached edits stay off disk until release, then verify direct reads and reload. */
static int kernel_hit_release(void) {
    cache_block_t *b = get_sector(scratch_start, 0);
    REQUIRE(b && b->block_num == scratch_start && b->flag_bits == PRESENT_FLAG);
    REQUIRE(sector_matches(b->data, 0, 0));
    REQUIRE(get_sector(scratch_start, 0) == b);
    fill_test_sector(b->data, 0, 1);
    cache_mark_dirty(b);
    cache_mark_dirty(b);
    REQUIRE(b->flag_bits == (PRESENT_FLAG | DIRTY_FLAG));
    REQUIRE(get_sector(scratch_start, 0) == b && sector_matches(b->data, 0, 1));
    REQUIRE((disk_read(CACHE_DRIVE, scratch_start + 0, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, 0, 0));
    cache_release(b);
    REQUIRE(b->flag_bits == 0 && b->block_num == UINT32_MAX);
    REQUIRE((disk_read(CACHE_DRIVE, scratch_start + 0, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, 0, 1));
    b = get_sector(scratch_start, 0);
    REQUIRE(b && sector_matches(b->data, 0, 1));
    cache_release(b);
    cache_mark_dirty(b);
    cache_release(b);
    REQUIRE(b->flag_bits == 0);
    REQUIRE((disk_read(CACHE_DRIVE, scratch_start + 0, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, 0, 1));
    return 1;
}

/* Check mixed/repeated flushes preserve entries and persist full sectors for reload. */
static int kernel_flush(void) {
    cache_block_t *slots[CACHE_SIZE];
    cache_flush_all();
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        slots[i] = get_sector(scratch_start, i);
        REQUIRE(slots[i] && sector_matches(slots[i]->data, i, 0));
        if (i % 2 == 0) {
            fill_test_sector(slots[i]->data, i, 1);
            cache_mark_dirty(slots[i]);
        }
    }
    cache_flush_all();
    cache_flush_all();
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        REQUIRE(get_sector(scratch_start, i) == slots[i] && slots[i]->flag_bits == PRESENT_FLAG);
        REQUIRE(sector_matches(slots[i]->data, i, i % 2 == 0));
        REQUIRE((disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) == DISK_OK));
        REQUIRE(sector_matches(sector_buffer, i, i % 2 == 0));
        cache_release(slots[i]);
        cache_block_t *b = get_sector(scratch_start, i);
        REQUIRE(b && sector_matches(b->data, i, i % 2 == 0));
        cache_release(b);
    }
    return 1;
}

/* Force dirty eviction across multiple cycles and verify writeback through direct IDE reads. */
static int kernel_eviction(void) {
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i) {
        cache_block_t *b = get_sector(scratch_start, i);
        REQUIRE(b && b->block_num == scratch_start + i);
        REQUIRE(b->flag_bits == PRESENT_FLAG && sector_matches(b->data, i, 0));
        fill_test_sector(b->data, i, 1);
        cache_mark_dirty(b);
    }
    /* Verify evicted dirty sectors before flushing the remaining entries. */
    for (unsigned i = 0; i < DISK_TEST_BLOCKS - CACHE_SIZE; ++i) {
        REQUIRE((disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, i, 1));
    }
    cache_flush_all();
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i) {
        REQUIRE((disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, i, 1));
        cache_block_t *b = get_sector(scratch_start, i);
        REQUIRE(b && b->block_num == scratch_start + i);
        REQUIRE(sector_matches(b->data, i, 1));
    }
    return 1;
}

/* Check slot reuse and clean eviction while verifying the disk contents remain intact. */
static int kernel_empty_slot(void) {
    cache_block_t *slots[CACHE_SIZE];
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        slots[i] = get_sector(scratch_start, i);
        REQUIRE(slots[i] && sector_matches(slots[i]->data, i, 0));
        for (unsigned j = 0; j < i; ++j) REQUIRE(slots[i] != slots[j]);
    }
    cache_release(slots[CACHE_SIZE / 2]);
    REQUIRE(get_sector(scratch_start, CACHE_SIZE) == slots[CACHE_SIZE / 2]);
    REQUIRE(get_sector(scratch_start, 0) == slots[0]);
    for (unsigned i = CACHE_SIZE + 1; i < DISK_TEST_BLOCKS; ++i) {
        cache_block_t *b = get_sector(scratch_start, i);
        REQUIRE(b && b->block_num == scratch_start + i);
        REQUIRE(sector_matches(b->data, i, 0));
    }
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i)
        REQUIRE((disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) == DISK_OK) && sector_matches(sector_buffer, i, 0));
    return 1;
}

/* Verify multi-sector transfers and an explicit device flush through real IDE. */
static int kernel_disk_interface(void) {
    static uint8_t buffer[3 * BLOCK_SIZE];
    for (unsigned i = 0; i < 3; ++i) fill_test_sector(buffer + i * BLOCK_SIZE, i, 1);
    REQUIRE(disk_write_sectors(CACHE_DRIVE, scratch_start, 3, buffer) == DISK_OK);
    REQUIRE(disk_flush(CACHE_DRIVE) == DISK_OK);
    for (unsigned i = 0; i < 3; ++i) fill_sector(buffer + i * BLOCK_SIZE, 0);
    REQUIRE(disk_read_sectors(CACHE_DRIVE, scratch_start, 3, buffer) == DISK_OK);
    for (unsigned i = 0; i < 3; ++i)
        REQUIRE(sector_matches(buffer + i * BLOCK_SIZE, i, 1));
    return 1;
}

/* Back up the scratch range, run kernel tests, then restore and verify the original data. */
void test_block_cache(void) {
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"hit, dirty release and reload", kernel_hit_release},
        {"mixed and repeated flush", kernel_flush},
        {"dirty eviction and wraparound", kernel_eviction},
        {"empty slot and clean eviction", kernel_empty_slot},
        {"multi-sector disk interface", kernel_disk_interface},
    };
    disk_info_t info;
    if (disk_get_info(CACHE_DRIVE, &info) != DISK_OK ||
        info.sector_count <= DISK_TEST_BLOCKS) {
        printk("Disk cache tests SKIPPED: ATA drive 1 is missing or too small.\n");
        return;
    }
    scratch_start = info.sector_count - DISK_TEST_BLOCKS;
    printk("Disk cache tests: drive 1, sectors %d through %d\n",
           scratch_start, info.sector_count - 1);
    cache_flush_all();
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i) {
        if ((disk_read(CACHE_DRIVE, scratch_start + i, saved_sectors[i]) != DISK_OK)) {
            printk("Disk cache tests ABORTED: backup read failed.\n");
            return;
        }
    }
    unsigned failed = 0;
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int passed = prepare_disk(scratch_start) && cases[i].run();
        printk("%s: %s\n", passed ? "PASS" : "FAIL", cases[i].name);
        failed += !passed;
    }
    /* Finish cache writes before restoring; leave no stale scratch entries. */
    drain_cache(scratch_start);
    unsigned restore_failed = 0;
    for (unsigned i = 0; i < DISK_TEST_BLOCKS; ++i) {
        if ((disk_write(1, scratch_start + i, saved_sectors[i]) != DISK_OK) || (disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) != DISK_OK) ||
            memcmp(saved_sectors[i], sector_buffer, BLOCK_SIZE) != 0)
            ++restore_failed;
    }
    printk("Disk cache tests: %d failed; restore errors: %d\n", failed, restore_failed);
}

/* Filesystem tests run once per boot, before any other fs_init call.
 * Formatting changes sectors 0 through data_start. Back up that range plus
 * one ordinary data sector, and restore it even when a REQUIRE fails.
 */
#define FS_TEST_DRIVE 1
#define FS_BACKUP_MAX_BLOCKS 4096 /* Limit the backup to 2 MiB. */
static superblock_t expected_sb;
static uint32_t fs_backup_blocks;

/* Check the on-disk layout, including every bitmap bit and unused inode byte. */
static int fs_layout_matches(void) {
    REQUIRE(disk_read(FS_TEST_DRIVE, 0, sector_buffer) == DISK_OK);
    REQUIRE(memcmp(sector_buffer, &expected_sb, BLOCK_SIZE) == 0);

    for (unsigned kind = 0; kind < 2; ++kind) {
        uint32_t start = kind ? expected_sb.inode_bitmap_start : expected_sb.block_bitmap_start;
        uint32_t count = kind ? expected_sb.inode_bitmap_blocks : expected_sb.block_bitmap_blocks;
        uint32_t limit = kind ? expected_sb.inode_count : expected_sb.total_blocks;
        for (uint32_t block = 0; block < count; ++block) {
            REQUIRE(disk_read(FS_TEST_DRIVE, start + block, sector_buffer) == DISK_OK);
            for (uint32_t bit = 0; bit < BLOCK_SIZE * 8; ++bit) {
                uint32_t index = block * BLOCK_SIZE * 8 + bit;
                int used = index >= limit || (kind ? index == 0 : index <= expected_sb.data_start);
                REQUIRE(((sector_buffer[bit / 8] >> (bit % 8)) & 1) == used);
            }
        }
    }

    uint8_t expected[BLOCK_SIZE];
    for (uint32_t block = 0; block < expected_sb.inode_blocks; ++block) {
        memset(expected, 0, BLOCK_SIZE);
        if (block == 0) {
            inode_t root = {0};
            root.type = FILE_TYPE_DIRECTORY;
            root.location.direct_pointers[0] = expected_sb.data_start;
            root.size = 2 * sizeof(dir_entry_t);
            memcpy(expected, &root, sizeof(root));
        }
        REQUIRE(disk_read(FS_TEST_DRIVE, expected_sb.inode_start + block, sector_buffer) == DISK_OK);
        REQUIRE(memcmp(sector_buffer, expected, BLOCK_SIZE) == 0);
    }

    memset(expected, 0, BLOCK_SIZE);
    dir_entry_t entries[2] = {0};
    entries[0].filename[0] = '.';
    entries[1].filename[0] = '.';
    entries[1].filename[1] = '.';
    /* Both entries point to inode 0; the remaining directory bytes stay zero. */
    memcpy(expected, entries, sizeof(entries));
    REQUIRE(disk_read(FS_TEST_DRIVE, expected_sb.data_start, sector_buffer) == DISK_OK);
    REQUIRE(memcmp(sector_buffer, expected, BLOCK_SIZE) == 0);
    return 1;
}

/* A failed init must allow retry; an invalid magic number must trigger format. */
static int kernel_fs_init(void) {
    REQUIRE(fs_init(4) == -1); /* IDE exposes only drives 0 through 3. */
    fill_sector(sector_buffer, 0xa5);
    REQUIRE(disk_write(FS_TEST_DRIVE, 0, sector_buffer) == DISK_OK);
    REQUIRE(fs_init(FS_TEST_DRIVE) == 0);
    return fs_layout_matches();
}

/* Repeated initialization must preserve existing contents rather than format again. */
static int kernel_fs_init_repeat(void) {
    uint8_t saved[BLOCK_SIZE];
    REQUIRE(disk_read(FS_TEST_DRIVE, 0, saved) == DISK_OK);
    saved[BLOCK_SIZE - 1] = 0x5a; /* Marker in the superblock's unused padding. */
    REQUIRE(disk_write(FS_TEST_DRIVE, 0, saved) == DISK_OK);
    REQUIRE(fs_init(FS_TEST_DRIVE) == 0);
    REQUIRE(fs_init(FS_TEST_DRIVE) == 0);
    REQUIRE(disk_read(FS_TEST_DRIVE, 0, sector_buffer) == DISK_OK);
    REQUIRE(memcmp(sector_buffer, saved, BLOCK_SIZE) == 0);
    return 1;
}

/* Start with nonzero metadata so missing clears cannot pass by accident.
 * Check format twice, and ensure the next ordinary data sector is untouched.
 */
static int kernel_fs_format(void) {
    drain_cache(0);
    fill_sector(sector_buffer, 0xa5);
    for (uint32_t block = 0; block < fs_backup_blocks; ++block)
        REQUIRE(disk_write(FS_TEST_DRIVE, block, sector_buffer) == DISK_OK);

    for (unsigned attempt = 0; attempt < 2; ++attempt) {
        REQUIRE(fs_format(FS_TEST_DRIVE) == 0);
        REQUIRE(fs_layout_matches());
        REQUIRE(disk_read(FS_TEST_DRIVE, expected_sb.data_start + 1, sector_buffer) == DISK_OK);
        for (unsigned byte = 0; byte < BLOCK_SIZE; ++byte)
            REQUIRE(sector_buffer[byte] == 0xa5);
    }
    return 1;
}

/* Formatting a nonexistent drive must fail without changing the current disk. */
static int kernel_fs_format_invalid(void) {
    uint8_t saved[BLOCK_SIZE];
    REQUIRE(disk_read(FS_TEST_DRIVE, 0, saved) == DISK_OK);
    REQUIRE(fs_format(4) == -1);
    REQUIRE(disk_read(FS_TEST_DRIVE, 0, sector_buffer) == DISK_OK);
    REQUIRE(memcmp(sector_buffer, saved, BLOCK_SIZE) == 0);
    return 1;
}

/* Like test_block_cache, verify through direct disk reads and always restore.
 * fs_init has no reset API: run this suite once, then reboot before mounting.
 */
void test_fs(void) {
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"fs_init failure, retry and automatic format", kernel_fs_init},
        {"fs_init repeated calls preserve contents", kernel_fs_init_repeat},
        {"fs_format layout, root, bitmaps and repeat", kernel_fs_format},
        {"fs_format rejects missing drive", kernel_fs_format_invalid},
    };
    disk_info_t info;
    if (disk_get_info(FS_TEST_DRIVE, &info) != DISK_OK || !info.writable ||
        info.sector_size != BLOCK_SIZE) {
        printk("Filesystem tests SKIPPED: writable ATA drive 1 is required.\n");
        return;
    }

    /* Build the expected geometry independently of the superblock on disk. */
    memset(&expected_sb, 0, sizeof(expected_sb));
    expected_sb.magic_number = MAGIC_NUMBER;
    expected_sb.total_blocks = info.sector_count;
    expected_sb.inode_count = info.sector_count / BLOCKS_PER_INODE;
    expected_sb.block_bitmap_start = 1;
    expected_sb.block_bitmap_blocks = DIV_ROUND_UP(info.sector_count, BLOCK_SIZE * 8);
    expected_sb.inode_bitmap_start = 1 + expected_sb.block_bitmap_blocks;
    expected_sb.inode_bitmap_blocks = DIV_ROUND_UP(expected_sb.inode_count, BLOCK_SIZE * 8);
    expected_sb.inode_start = expected_sb.inode_bitmap_start + expected_sb.inode_bitmap_blocks;
    expected_sb.inode_blocks = DIV_ROUND_UP(expected_sb.inode_count, INODES_PER_BLOCK);
    expected_sb.data_start = expected_sb.inode_start + expected_sb.inode_blocks;
    if (expected_sb.inode_count == 0 || expected_sb.data_start + 1 >= info.sector_count ||
        expected_sb.data_start + 2 > FS_BACKUP_MAX_BLOCKS) {
        printk("Filesystem tests SKIPPED: disk layout exceeds the test backup limits.\n");
        return;
    }
    expected_sb.data_blocks = info.sector_count - expected_sb.data_start;
    expected_sb.free_blocks = expected_sb.data_blocks - 1;
    expected_sb.free_inodes = expected_sb.inode_count - 1;
    expected_sb.root_inode = 0;
    fs_backup_blocks = expected_sb.data_start + 2;

    uint8_t *backup = mem_kalloc(fs_backup_blocks * BLOCK_SIZE);
    if (!backup) {
        printk("Filesystem tests SKIPPED: could not allocate backup.\n");
        return;
    }
    drain_cache(0);
    for (uint32_t block = 0; block < fs_backup_blocks; ++block) {
        if (disk_read(FS_TEST_DRIVE, block, backup + block * BLOCK_SIZE) != DISK_OK) {
            printk("Filesystem tests ABORTED: backup read failed.\n");
            mem_kfree(backup);
            return;
        }
    }

    unsigned failed = 0;
    /* Later cases need the cache on drive 1 even if the first init fails. */
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        int passed = cases[i].run();
        printk("%s: %s\n", passed ? "PASS" : "FAIL", cases[i].name);
        failed += !passed;
        if (i == 0 && !passed) {
            printk("Filesystem tests: remaining cases SKIPPED after init failure.\n");
            break;
        }
    }

    /* Clear cached test data before restoring the original disk contents. */
    cache_init(FS_TEST_DRIVE);
    drain_cache(0);
    unsigned restore_failed = 0;
    for (uint32_t block = 0; block < fs_backup_blocks; ++block) {
        uint8_t *saved = backup + block * BLOCK_SIZE;
        if (disk_write(FS_TEST_DRIVE, block, saved) != DISK_OK ||
            disk_read(FS_TEST_DRIVE, block, sector_buffer) != DISK_OK ||
            memcmp(saved, sector_buffer, BLOCK_SIZE) != 0)
            ++restore_failed;
    }
    if (disk_flush(FS_TEST_DRIVE) != DISK_OK) ++restore_failed;
    mem_kfree(backup);
    printk("Filesystem tests: %d failed; restore errors: %d\n", failed, restore_failed);
}
