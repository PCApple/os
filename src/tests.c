#ifdef BLOCK_CACHE_HOST_TEST
#include "../tests/host.h"

#define CHECK(condition) do { if (!(condition)) { \
    printf("  FAIL line %d: %s\n", __LINE__, #condition); return 0; \
} } while (0)

/* Generate repeatable data that varies by block and byte offset. */
static uint8_t pattern(unsigned block, unsigned byte) {
    return (uint8_t)(block * 37 + byte * 13 + byte / 256);
}

/* Reset the host cache, I/O counters and fake disk before each test. */
static void reset_fixture(void) {
    cache_init();
    test_reads = test_writes = 0;
    for (unsigned i = 0; i < TEST_BLOCKS; ++i)
        for (unsigned j = 0; j < BLOCK_SIZE; ++j)
            test_disk[i][j] = pattern(i, j);
}

/* Return true when a clean, present cache entry matches the requested disk block. */
static int matches(cache_block_t *block, unsigned number) {
    return block && block->block_num == number &&
        block->flag_bits == PRESENT_FLAG &&
        memcmp(block->data, test_disk[number], BLOCK_SIZE) == 0;
}

/* Check that the first access reads disk and a repeated access reuses the entry. */
static int cold_read_and_hit(void) {
    cache_block_t *b = cache_get(0);
    CHECK(matches(b, 0));
    CHECK(cache_get(0) == b);
    CHECK(test_reads == 1 && test_writes == 0);
    return 1;
}

/* Fill every slot, then check round-robin eviction without unnecessary writes. */
static int capacity_and_clean_eviction(void) {
    cache_block_t *slots[CACHE_SIZE];
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        slots[i] = cache_get(i);
        CHECK(matches(slots[i], i));
        for (unsigned j = 0; j < i; ++j) CHECK(slots[i] != slots[j]);
    }
    CHECK(cache_get(0) == slots[0]);
    CHECK(cache_get(CACHE_SIZE) == slots[0]);
    CHECK(matches(slots[0], CACHE_SIZE));
    CHECK(test_reads == CACHE_SIZE + 1 && test_writes == 0);
    return 1;
}

/* Check dirty hits and repeated marking, then verify release writes data for reload. */
static int dirty_hit_and_release(void) {
    cache_block_t *b = cache_get(2);
    memset(b->data, 0xa5, BLOCK_SIZE);
    cache_mark_dirty(b);
    cache_mark_dirty(b);
    CHECK(b->flag_bits == (PRESENT_FLAG | DIRTY_FLAG));
    CHECK(cache_get(2) == b && b->data[BLOCK_SIZE - 1] == 0xa5);
    CHECK(test_reads == 1 && test_writes == 0);
    cache_release(b);
    CHECK(b->flag_bits == 0 && b->block_num == UINT32_MAX);
    CHECK(test_writes == 1);
    for (unsigned j = 0; j < BLOCK_SIZE; ++j) CHECK(test_disk[2][j] == 0xa5);
    CHECK(matches(cache_get(2), 2));
    CHECK(test_reads == 2);
    return 1;
}

/* Verify clean/repeated release and marking an invalid entry cause no writes. */
static int clean_release_and_invalid_dirty(void) {
    cache_block_t *b = cache_get(1);
    cache_release(b);
    cache_mark_dirty(b);
    cache_release(b);
    CHECK(b->flag_bits == 0 && b->block_num == UINT32_MAX);
    CHECK(test_writes == 0);
    CHECK(matches(cache_get(1), 1));
    CHECK(test_reads == 2);
    return 1;
}

/* Flush mixed clean/dirty entries and verify flags, write counts and reloaded data. */
static int flush_mixed_and_repeat(void) {
    cache_block_t *slots[CACHE_SIZE];
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        slots[i] = cache_get(i);
        if (i % 2 == 0) {
            for (unsigned j = 0; j < BLOCK_SIZE; ++j)
                slots[i]->data[j] = (uint8_t)~pattern(i, j);
            cache_mark_dirty(slots[i]);
        }
    }
    cache_flush_all();
    CHECK(test_writes == (CACHE_SIZE + 1) / 2);
    for (unsigned i = 0; i < CACHE_SIZE; ++i) {
        CHECK(cache_get(i) == slots[i] && matches(slots[i], i));
        for (unsigned j = 0; j < BLOCK_SIZE; ++j)
            CHECK(test_disk[i][j] == (i % 2 ? pattern(i, j) : (uint8_t)~pattern(i, j)));
        cache_release(slots[i]);
    }
    cache_flush_all();
    CHECK(test_writes == (CACHE_SIZE + 1) / 2 && test_reads == CACHE_SIZE);
    for (unsigned i = 0; i < CACHE_SIZE; ++i) CHECK(matches(cache_get(i), i));
    return 1;
}

/* Force several eviction cycles and verify every modified block reaches disk. */
static int dirty_eviction_and_wraparound(void) {
    for (unsigned i = 0; i < TEST_BLOCKS; ++i) {
        cache_block_t *b = cache_get(i);
        CHECK(matches(b, i));
        memset(b->data, (uint8_t)(i + 1), BLOCK_SIZE);
        cache_mark_dirty(b);
    }
    CHECK(test_writes == TEST_BLOCKS - CACHE_SIZE);
    cache_flush_all();
    CHECK(test_writes == TEST_BLOCKS);
    for (unsigned i = 0; i < TEST_BLOCKS; ++i) {
        CHECK(matches(cache_get(i), i));
        for (unsigned j = 0; j < BLOCK_SIZE; ++j) CHECK(test_disk[i][j] == i + 1);
    }
    CHECK(test_writes == TEST_BLOCKS);
    return 1;
}

/* Check that a released slot is reused before another cached block is evicted. */
static int reuse_empty_slot(void) {
    cache_block_t *first = cache_get(0);
    for (unsigned i = 1; i < CACHE_SIZE; ++i) CHECK(matches(cache_get(i), i));
    cache_block_t *hole = cache_get(CACHE_SIZE / 2);
    cache_release(hole);
    CHECK(cache_get(CACHE_SIZE) == hole);
    CHECK(cache_get(0) == first && matches(first, 0));
    CHECK(test_writes == 0);
    return 1;
}

/* Verify repeated flushes of an empty cache perform no disk I/O. */
static int empty_flush(void) {
    cache_flush_all();
    cache_flush_all();
    CHECK(test_reads == 0 && test_writes == 0);
    return 1;
}

/* Exercise the real disk interface with the in-memory IDE backend. */
static int disk_interface(void) {
    disk_info_t info;
    CHECK(disk_get_info(CACHE_DRIVE, &info) == DISK_OK);
    CHECK(info.available && info.writable && info.sector_count == TEST_BLOCKS);
    CHECK(info.sector_size == BLOCK_SIZE);
    CHECK(disk_get_info(4, &info) == DISK_ERR_NO_DEVICE);
    CHECK(!info.available);
    CHECK(disk_get_info(CACHE_DRIVE, NULL) == DISK_ERR_ARGUMENT);
    fill_sector(test_io, 0xa5);
    fill_sector(test_io + BLOCK_SIZE, 0x5a);
    CHECK(disk_write_sectors(CACHE_DRIVE, 1, 2, test_io) == DISK_OK);
    memset(test_io, 0, 2 * BLOCK_SIZE);
    CHECK(disk_read_sectors(CACHE_DRIVE, 1, 2, test_io) == DISK_OK);
    for (unsigned i = 0; i < 2 * BLOCK_SIZE; ++i)
        CHECK(test_io[i] == (i < BLOCK_SIZE ? 0xa5 : 0x5a));
    CHECK(disk_read_sectors(CACHE_DRIVE, TEST_BLOCKS - 1, 2, test_io) == DISK_ERR_RANGE);
    CHECK(disk_read_sectors(CACHE_DRIVE, UINT32_MAX, 2, test_io) == DISK_ERR_RANGE);
    CHECK(disk_read_sectors(CACHE_DRIVE, 0, 0, test_io) == DISK_ERR_ARGUMENT);
    CHECK(disk_read(CACHE_DRIVE, 0, NULL) == DISK_ERR_ARGUMENT);
    CHECK(test_reads == 1 && test_writes == 1);
    CHECK(disk_flush(CACHE_DRIVE) == DISK_OK);
    test_io_error = 1;
    disk_status_t read_error = disk_read(CACHE_DRIVE, 0, test_io);
    disk_status_t write_error = disk_write(CACHE_DRIVE, 0, test_io);
    disk_status_t flush_error = disk_flush(CACHE_DRIVE);
    cache_block_t *failed_read = cache_get(0);
    test_io_error = 0;
    CHECK(failed_read == NULL);
    cache_block_t *b = cache_get(0);
    CHECK(b != NULL);
    cache_mark_dirty(b);
    test_io_error = 1;
    cache_flush_all();
    cache_release(b);
    test_io_error = 0;
    CHECK(b->flag_bits == (PRESENT_FLAG | DIRTY_FLAG));
    cache_release(b);
    CHECK(b->flag_bits == 0);
    CHECK(read_error == DISK_ERR_IO && write_error == DISK_ERR_IO && flush_error == DISK_ERR_IO);
    return 1;
}

/* Run each host scenario twice with fresh state; return failure if any check fails. */
int main(void) {
    static const struct { const char *name; int (*run)(void); } cases[] = {
        {"cold read and cache hit", cold_read_and_hit},
        {"capacity and clean eviction", capacity_and_clean_eviction},
        {"dirty hit and release persistence", dirty_hit_and_release},
        {"clean release and invalid dirty mark", clean_release_and_invalid_dirty},
        {"mixed flush, repeat flush and reload", flush_mixed_and_repeat},
        {"dirty eviction and multiple wraps", dirty_eviction_and_wraparound},
        {"reuse empty slot before eviction", reuse_empty_slot},
        {"empty flush", empty_flush},
        {"disk interface", disk_interface},
    };
    unsigned failed = 0;
    for (unsigned repeat = 0; repeat < 2; ++repeat)
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            reset_fixture();
            int passed = cases[i].run();
            printf("%s: %s\n", passed ? "PASS" : "FAIL", cases[i].name);
            failed += !passed;
        }
    printf("%u cases run; %u failed\n", (unsigned)(2 * sizeof(cases) / sizeof(cases[0])), failed);
    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
#else
#include "include/tests.h"
#include "include/disk.h"
#include "include/utils.h"

#define DISK_TEST_BLOCKS (CACHE_SIZE * 4 + 3)
static uint8_t saved_sectors[DISK_TEST_BLOCKS][BLOCK_SIZE];
static uint8_t sector_buffer[BLOCK_SIZE];
static uint32_t scratch_start;

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
        if ((disk_write(CACHE_DRIVE, scratch_start + i, saved_sectors[i]) != DISK_OK) || (disk_read(CACHE_DRIVE, scratch_start + i, sector_buffer) != DISK_OK) ||
            memcmp(saved_sectors[i], sector_buffer, BLOCK_SIZE) != 0)
            ++restore_failed;
    }
    printk("Disk cache tests: %d failed; restore errors: %d\n", failed, restore_failed);
}
#endif
