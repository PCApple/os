#include "include/disk.h"
#include "include/ide.h"

/* Keep IDE device details behind the disk interface. */
disk_status_t disk_get_info(uint8_t drive, disk_info_t *info) {
    if (!info) return DISK_ERR_ARGUMENT;
    *info = (disk_info_t){0};
    if (drive >= 4 || !ide_devices[drive].Reserved) return DISK_ERR_NO_DEVICE;
    if (ide_devices[drive].Type != IDE_ATA) return DISK_ERR_UNSUPPORTED;
    info->sector_count = ide_devices[drive].Size;
    info->sector_size = DISK_SECTOR_SIZE;
    info->available = 1;
    info->writable = drive != 0;
    return DISK_OK;
}

/* Validate the full transfer before issuing any I/O, avoiding integer overflow. */
static disk_status_t validate(uint8_t drive, uint32_t lba, uint32_t count,
                              const void *buffer) {
    disk_info_t info;
    disk_status_t status = disk_get_info(drive, &info);
    if (status != DISK_OK) return status;
    if (drive == 0) return DISK_ERR_PROTECTED;
    if (!buffer || !count) return DISK_ERR_ARGUMENT;
    if (lba >= info.sector_count || count > info.sector_count - lba)
        return DISK_ERR_RANGE;
    if ((uintptr_t)buffer > UINT32_MAX ||
        count > (UINT32_MAX - (uintptr_t)buffer) / DISK_SECTOR_SIZE)
        return DISK_ERR_ARGUMENT;
    return DISK_OK;
}

/* Split large requests into the IDE driver's nonzero 8-bit sector counts. */
static disk_status_t transfer(uint8_t drive, uint32_t lba, uint32_t count,
                              const void *buffer, int write) {
    disk_status_t status = validate(drive, lba, count, buffer);
    if (status != DISK_OK) return status;
    uintptr_t address = (uintptr_t)buffer;
    while (count) {
        uint8_t chunk = count > 255 ? 255 : (uint8_t)count;
        uint8_t error = write
            ? ide_write_sectors(drive, lba, chunk, DS, (uint32_t)address)
            : ide_read_sectors(drive, lba, chunk, DS, (uint32_t)address);
        if (error) return DISK_ERR_IO;
        lba += chunk;
        count -= chunk;
        address += (uint32_t)chunk * DISK_SECTOR_SIZE;
    }
    return DISK_OK;
}

/* Read consecutive sectors into a caller-owned buffer. */
disk_status_t disk_read_sectors(uint8_t drive, uint32_t lba, uint32_t count, uint8_t *buffer) {
    return transfer(drive, lba, count, buffer, 0);
}

/* Write consecutive sectors from a caller-owned buffer. */
disk_status_t disk_write_sectors(uint8_t drive, uint32_t lba, uint32_t count, const uint8_t *buffer) {
    return transfer(drive, lba, count, buffer, 1);
}

/* Read one sector using the same validation as a multi-sector request. */
disk_status_t disk_read(uint8_t drive, uint32_t lba, uint8_t *buffer) {
    return disk_read_sectors(drive, lba, 1, buffer);
}

/* Write one sector using the same validation as a multi-sector request. */
disk_status_t disk_write(uint8_t drive, uint32_t lba, const uint8_t *buffer) {
    return disk_write_sectors(drive, lba, 1, buffer);
}

/* Explicitly complete pending device writes without touching software cache entries. */
disk_status_t disk_flush(uint8_t drive) {
    disk_info_t info;
    disk_status_t status = disk_get_info(drive, &info);
    if (status != DISK_OK) return status;
    if (drive == 0) return DISK_ERR_PROTECTED;
    return ide_flush(drive) == 0 ? DISK_OK : DISK_ERR_IO;
}

/* Initialize a sector buffer, for example with zero before writing it to disk. */
void fill_sector(uint8_t *buffer, uint8_t value) {
    for (unsigned i = 0; i < DISK_SECTOR_SIZE; ++i) buffer[i] = value;
}
