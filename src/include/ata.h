#ifndef ATA_H
#define ATA_H

#include "types.h"

#define ATA_PRIMARY_DATA         0x1F0
#define ATA_PRIMARY_ERROR        0x1F1
#define ATA_PRIMARY_SECCOUNT     0x1F2
#define ATA_PRIMARY_LBA_LO       0x1F3
#define ATA_PRIMARY_LBA_MID      0x1F4
#define ATA_PRIMARY_LBA_HI       0x1F5
#define ATA_PRIMARY_DRIVE_HEAD   0x1F6
#define ATA_PRIMARY_STATUS       0x1F7
#define ATA_PRIMARY_COMMAND      0x1F7
#define ATA_PRIMARY_CONTROL      0x3F6

#define ATA_CMD_READ_PIO         0x20
#define ATA_CMD_WRITE_PIO        0x30
#define ATA_CMD_IDENTIFY         0xEC

#define ATA_STATUS_ERR           0x01
#define ATA_STATUS_DRQ           0x08
#define ATA_STATUS_DF            0x20
#define ATA_STATUS_RDY           0x40
#define ATA_STATUS_BSY           0x80

#define ATA_SECTOR_SIZE          512

/**
 * Initialize Primary ATA Bus and identify connected hard disks.
 */
bool ata_init(void);

/**
 * Read 512-byte sector at LBA28 address into buffer.
 */
bool ata_read_sector(uint32_t lba, uint8_t *buffer);

/**
 * Write 512-byte sector at LBA28 address from buffer.
 */
bool ata_write_sector(uint32_t lba, const uint8_t *buffer);

#endif // ATA_H
