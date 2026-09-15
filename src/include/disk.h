#ifndef DISK_H
#define DISK_H
#include <stdint.h>
#define DISK_SECTOR_SIZE 512

typedef enum {
    DISK_OK = 0,
    DISK_ERR_ARGUMENT,
    DISK_ERR_NO_DEVICE,
    DISK_ERR_UNSUPPORTED,
    DISK_ERR_PROTECTED,
    DISK_ERR_RANGE,
    DISK_ERR_IO
} disk_status_t;

typedef struct {
    uint32_t sector_count;
    uint32_t sector_size;
    uint8_t available; /* ATA device present; protected drives remain discoverable. */
    uint8_t writable;
} disk_info_t;

/* All operations return DISK_OK on success. Addresses are absolute sector LBAs.
 * Buffers must cover count * DISK_SECTOR_SIZE bytes. A failed multi-sector
 * operation may have transferred a prefix; there is no rollback.
 */
disk_status_t disk_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, uint8_t *buffer);
disk_status_t disk_write_sectors(uint8_t drive, uint32_t lba, uint32_t count, const uint8_t *buffer);
/* Single-sector convenience wrappers with the same status convention. */
disk_status_t disk_read(uint8_t drive, uint32_t lba, uint8_t *buffer);
disk_status_t disk_write(uint8_t drive, uint32_t lba, const uint8_t *buffer);
/* Initialize info even when the drive is absent or unsupported. */
disk_status_t disk_get_info(uint8_t drive, disk_info_t *info);
/* Flush the device write cache, not the software block cache. */
disk_status_t disk_flush(uint8_t drive);
/* Fill a sector-sized memory buffer; performs no disk I/O. */
void fill_sector(uint8_t *buffer, uint8_t value);
#endif
