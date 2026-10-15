/* Run the real lookup code with an in-memory cache; no disk or QEMU needed. */
#include <stdio.h>
#include <setjmp.h>
#include "../src/include/fs.h"

/* These helpers are currently internal to fs.c, so keep declarations local. */
int fs_lookup_dir_entry_inode_index(inode_t *, const char *);
int fs_lookup_parent(const char *, inode_handle_t *);

static jmp_buf test_failure;

#define CHECK(expr) do { if (!(expr)) { \
    fprintf(stderr, "  line %d: %s\n", __LINE__, #expr); \
    longjmp(test_failure, 1); \
} } while (0)

enum { BLOCKS = 64, INODE_START = 3, DIRECT = 20, INDIRECT = 21,
       INDIRECT_DATA = 22, DOUBLE = 23, DOUBLE_INNER = 24, DOUBLE_DATA = 25 };
static cache_block_t blocks[BLOCKS];
static unsigned held[BLOCKS], calls;
static unsigned fail_hits;
static int fail_block = -1;
static int poison_on_release = -1;
static inode_t directory;

cache_block_t *cache_get(uint32_t number) {
    /* Bound repeated cache calls so a traversal stuck on a slash fails promptly. */
    CHECK(++calls < 1024);
    CHECK(number < BLOCKS);
    if ((int)number == fail_block) { ++fail_hits; return NULL; }
    ++held[number];
    return &blocks[number];
}

void cache_release(cache_block_t *block) {
    CHECK(block >= blocks && block < blocks + BLOCKS);
    unsigned number = (unsigned)(block - blocks);
    CHECK(held[number] > 0);
    --held[number];
    /* Model released data becoming unavailable; only enabled by lifetime tests. */
    if ((int)number == poison_on_release && held[number] == 0)
        for (unsigned i = 0; i < BLOCK_SIZE; ++i) block->data[i] = 0xa5;
}

static void zero(void *ptr, size_t size) {
    unsigned char *bytes = ptr;
    for (size_t i = 0; i < size; ++i) bytes[i] = 0;
}

static inode_t *inode(unsigned number) {
    return &((inode_t *)blocks[INODE_START + number / INODES_PER_BLOCK].data)
        [number % INODES_PER_BLOCK];
}

static int entry(unsigned block, unsigned slot, const char *name,
                          unsigned number) {
    dir_entry_t *result = &((dir_entry_t *)blocks[block].data)[slot];
    CHECK(strlen(name) <= MAX_FILENAME_LEN);
    zero(result, sizeof(*result));
    for (size_t i = 0; i <= strlen(name); ++i) result->filename[i] = name[i];
    result->inode_index = number;
    return number;
}

static void fixture(void) {
    zero(blocks, sizeof(blocks));
    zero(held, sizeof(held));
    zero(&directory, sizeof(directory));
    calls = 0;
    fail_hits = 0;
    fail_block = -1;
    poison_on_release = -1;
    directory.type = FILE_TYPE_DIRECTORY;
    for (unsigned i = 0; i < BLOCKS; ++i) blocks[i].block_num = i;
    superblock_t *sb = (superblock_t *)blocks[0].data;
    sb->inode_start = INODE_START;
    sb->root_inode = 0;
    sb->inode_count = 3 * INODES_PER_BLOCK;
    inode(0)->type = FILE_TYPE_DIRECTORY;
    inode(0)->location.direct_pointers[0] = 30;
    entry(30, 0, "dir", 1);
    entry(30, 1, "file", 2);
    inode(1)->type = FILE_TYPE_DIRECTORY;
    inode(1)->location.direct_pointers[0] = 31;
    inode(2)->type = FILE_TYPE_FILE;
    entry(31, 0, "sub", INODES_PER_BLOCK + 1);
    inode(INODES_PER_BLOCK + 1)->type = FILE_TYPE_DIRECTORY;
    inode(INODES_PER_BLOCK + 1)->location.direct_pointers[0] = 32;
}

static unsigned digest(void) {
    unsigned value = 2166136261u;
    const unsigned char *bytes = (const unsigned char *)blocks;
    for (size_t i = 0; i < sizeof(blocks); ++i) value = (value ^ bytes[i]) * 16777619u;
    bytes = (const unsigned char *)&directory;
    for (size_t i = 0; i < sizeof(directory); ++i) value = (value ^ bytes[i]) * 16777619u;
    return value;
}

static void unchanged(unsigned before) {
    CHECK(digest() == before);
    for (unsigned i = 0; i < BLOCKS; ++i) CHECK(held[i] == 0);
}

/* Sparse chains reach the final pointer and entry at every addressing level. */
static unsigned setup_level(unsigned level) {
    if (level == 0) {
        directory.location.direct_pointers[N_DIRECT_POINTERS - 1] = DIRECT;
        return DIRECT;
    }
    if (level == 1) {
        directory.location.indirect_pointer[0] = INDIRECT;
        ((uint32_t *)blocks[INDIRECT].data)[POINTERS_PER_BLOCK - 1] = INDIRECT_DATA;
        return INDIRECT_DATA;
    }
    directory.location.double_indirect_pointer[0] = DOUBLE;
    ((uint32_t *)blocks[DOUBLE].data)[POINTERS_PER_BLOCK - 1] = DOUBLE_INNER;
    ((uint32_t *)blocks[DOUBLE_INNER].data)[POINTERS_PER_BLOCK - 1] = DOUBLE_DATA;
    return DOUBLE_DATA;
}

static void lookup(inode_t *dir, const char *name, int expected) {
    unsigned before = digest();
    int actual = fs_lookup_dir_entry_inode_index(dir, name);
    CHECK(actual == expected);
    unchanged(before);
}

/* A successful parent lookup transfers exactly one cache acquisition. */
static void lookup_parent(const char *path, int expected) {
    unsigned before = digest();
    inode_handle_t handle = {0};
    int status = fs_lookup_parent(path, &handle);
    CHECK(status == (expected < 0 ? -1 : 0));
    if (expected >= 0) {
        unsigned block = INODE_START + (unsigned)expected / INODES_PER_BLOCK;
        CHECK(handle.block == &blocks[block]);
        CHECK(handle.inode == inode((unsigned)expected));
        CHECK(handle.inode->type == FILE_TYPE_DIRECTORY);
        for (unsigned i = 0; i < BLOCKS; ++i)
            CHECK(held[i] == (i == block ? 1u : 0u));
        CHECK(digest() == before);
        cache_release(handle.block);
    }
    unchanged(before);
}

static void dir_arguments(unsigned which) {
    switch (which) {
    case 0: lookup(NULL, "x", -1); break;
    case 1: lookup(&directory, NULL, -1); break;
    case 2: directory.type = FILE_TYPE_FILE; lookup(&directory, "x", -1); break;
    case 3: directory.type = FILE_TYPE_UNUSED; lookup(&directory, "x", -1); break;
    case 4: lookup(&directory, "missing", -1); break;
    case 5: setup_level(0); lookup(&directory, "", -1); break;
    }
}

static void dir_level(unsigned which) {
    unsigned level = which / 4, variant = which % 4;
    unsigned block = setup_level(level);
    if (variant == 0) lookup(&directory, "missing", -1);
    if (variant == 1) lookup(&directory, "first", entry(block, 0, "first", 7));
    if (variant == 2) lookup(&directory, "last",
        entry(block, ENTRIES_PER_DIR_BLOCK - 1, "last", 8));
    if (variant == 3) {
        /* Scan a nonmatching block before reaching a later match. */
        unsigned earlier = block + 16;
        if (level == 0) directory.location.direct_pointers[0] = earlier;
        if (level == 1) ((uint32_t *)blocks[INDIRECT].data)[0] = earlier;
        if (level == 2) {
            ((uint32_t *)blocks[DOUBLE].data)[0] = 42;
            ((uint32_t *)blocks[42].data)[0] = 43;
            ((uint32_t *)blocks[DOUBLE_INNER].data)[0] = earlier;
        }
        entry(earlier, 0, "other", 9);
        lookup(&directory, "last", entry(block, ENTRIES_PER_DIR_BLOCK - 1, "last", 8));
    }
}

static void dir_names(unsigned unused) {
    (void)unused;
    unsigned block = setup_level(0);
    int e = entry(block, 0, "abcdefghijklm", 65535);
    lookup(&directory, "abcdefghijklm", e);
    lookup(&directory, "abcdefghijkl", -1);
    lookup(&directory, "abcdefghijklmn", -1);
    lookup(&directory, "Abcdefghijklm", -1);
    lookup(&directory, ".", entry(block, 1, ".", 0));
}

static void dir_precedence(unsigned unused) {
    (void)unused;
    setup_level(0); setup_level(1); setup_level(2);
    int d = entry(DIRECT, 0, "same", 1);
    entry(INDIRECT_DATA, 0, "same", 2);
    entry(DOUBLE_DATA, 0, "same", 3);
    lookup(&directory, "same", d);
    lookup(&directory, "absent", -1); /* Exhaust all three addressing levels. */
}

static void dir_io_error(unsigned which) {
    static const unsigned levels[] = {0, 1, 1, 2, 2, 2};
    static const unsigned failures[] = {DIRECT, INDIRECT, INDIRECT_DATA,
                                        DOUBLE, DOUBLE_INNER, DOUBLE_DATA};
    unsigned block = setup_level(levels[which]);
    entry(block, 0, "target", 7);
    fail_block = failures[which];
    lookup(&directory, "target", -1);
    CHECK(fail_hits == 1);
}

static void parent(unsigned which) {
    static const struct { const char *path; int expected; } cases[] = {
        {"/new", 0}, {"/file", 0}, /* Leaf may be absent or already exist. */
        {"/dir/new", 1}, {"/dir/sub/new", INODES_PER_BLOCK + 1},
        {NULL, -1}, {"", -1}, {"relative", -1}, {"/", -1},
        {"/dir/", -1}, {"/missing/new", -1}, {"/file/new", -1},
        {"/dir/abcdefghijklm", 1},
        {"/abcdefghijklmn/new", -1},
        {"/dir/sub/missing/new", -1},
    };
    lookup_parent(cases[which].path, cases[which].expected);
}

static void parent_root_elsewhere(unsigned unused) {
    (void)unused;
    superblock_t *sb = (superblock_t *)blocks[0].data;
    sb->root_inode = INODES_PER_BLOCK + 2;
    inode(sb->root_inode)->type = FILE_TYPE_DIRECTORY;
    lookup_parent("/new", sb->root_inode);
}

static void parent_io_error(unsigned which) {
    static const unsigned failures[] = {0, INODE_START, 30, INODE_START + 1};
    fail_block = failures[which];
    lookup_parent("/dir/sub/new", -1);
    CHECK(fail_hits == 1);
}

static void dir_release_value(unsigned level) {
    unsigned block = setup_level(level);
    entry(block, ENTRIES_PER_DIR_BLOCK - 1, "target", 65535);
    poison_on_release = block;
    int actual = fs_lookup_dir_entry_inode_index(&directory, "target");
    CHECK(actual == 65535);
    for (unsigned i = 0; i < BLOCKS; ++i) CHECK(held[i] == 0);
}

static void parent_two_handles(unsigned which) {
    unsigned before = digest();
    inode_handle_t first = {0}, second = {0};
    unsigned second_inode = which ? INODES_PER_BLOCK + 1 : 1;
    CHECK(fs_lookup_parent("/new", &first) == 0);
    CHECK(fs_lookup_parent(which ? "/dir/sub/new" : "/dir/new", &second) == 0);
    CHECK(first.block == &blocks[INODE_START]);
    CHECK(first.inode == inode(0));
    CHECK(second.block == &blocks[INODE_START + second_inode / INODES_PER_BLOCK]);
    CHECK(second.inode == inode(second_inode));
    for (unsigned i = 0; i < BLOCKS; ++i) {
        unsigned expected = (i == INODE_START) +
            (i == INODE_START + second_inode / INODES_PER_BLOCK);
        CHECK(held[i] == expected);
    }
    cache_release(second.block);
    CHECK(held[INODE_START] == 1);
    CHECK(first.inode->type == FILE_TYPE_DIRECTORY);
    cache_release(first.block);
    unchanged(before);
}

/* Keep assertion failures local to one case, then reset the fixture for the next.
 * Crashes stop the process directly so GDB retains the failing stack. */
static int run_case(void (*run)(unsigned), unsigned which) {
    if (setjmp(test_failure) != 0) return 0;
    fixture();
    run(which);
    return 1;
}

int main(void) {
    static const struct { const char *name; void (*run)(unsigned); unsigned count; } groups[] = {
        {"directory arguments (null inode/name, file, unused, empty, empty name)", dir_arguments, 6},
        {"directory levels (direct/indirect/double: miss, first, last, scan)", dir_level, 12},
        {"directory exact names, max length, case, root inode zero", dir_names, 1},
        {"directory search order and exhaustive miss", dir_precedence, 1},
        {"directory cache failures and balanced releases", dir_io_error, 6},
        {"directory inode value survives data release", dir_release_value, 3},
        {"parent independent handle ownership", parent_two_handles, 2},
        {"parent paths (root, existing, nested, invalid, missing, length)", parent, 14},
        {"parent root inode in another table block", parent_root_elsewhere, 1},
        {"parent cache failures and balanced releases", parent_io_error, 4},
    };
    unsigned total = 0, failed = 0;
    for (unsigned g = 0; g < sizeof(groups) / sizeof(groups[0]); ++g) {
        for (unsigned i = 0; i < groups[g].count; ++i) {
            printf("RUN  %s [%u]\n", groups[g].name, i);
            fflush(stdout);
            int passed = run_case(groups[g].run, i);
            printf("%s %s [%u]\n", passed ? "PASS" : "FAIL", groups[g].name, i);
            ++total;
            failed += !passed;
        }
    }
    printf("%u tests: %u passed, %u failed\n", total, total - failed, failed);
    return failed ? 1 : 0;
}
