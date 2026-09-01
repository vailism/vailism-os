#include "../include/ata.h"
#include "../include/io.h"
#include "../include/serial.h"

static bool g_ata_master_present = false;

bool ata_is_master_present(void) {
    return g_ata_master_present;
}

static void ata_wait_ready(void) {
    // 400ns delay by reading status port 4 times
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);
    inb(ATA_PRIMARY_STATUS);

    while (inb(ATA_PRIMARY_STATUS) & ATA_STATUS_BSY);
}

static bool ata_wait_drq(void) {
    ata_wait_ready();
    for (int i = 0; i < 10000; i++) {
        uint8_t status = inb(ATA_PRIMARY_STATUS);
        if (status & ATA_STATUS_ERR) return false;
        if (status & ATA_STATUS_DRQ) return true;
    }
    return false;
}

bool ata_init(void) {
    // Select Master Drive (0xA0)
    outb(ATA_PRIMARY_DRIVE_HEAD, 0xA0);
    io_wait();

    outb(ATA_PRIMARY_SECCOUNT, 0);
    outb(ATA_PRIMARY_LBA_LO, 0);
    outb(ATA_PRIMARY_LBA_MID, 0);
    outb(ATA_PRIMARY_LBA_HI, 0);

    // Send IDENTIFY command
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_IDENTIFY);
    io_wait();

    uint8_t status = inb(ATA_PRIMARY_STATUS);
    if (status == 0) {
        serial_puts("[ATA] No drive found on Primary Master.\n");
        return false;
    }

    // Wait until BSY clears
    while (status & ATA_STATUS_BSY) {
        status = inb(ATA_PRIMARY_STATUS);
    }

    if (inb(ATA_PRIMARY_LBA_MID) != 0 || inb(ATA_PRIMARY_LBA_HI) != 0) {
        serial_puts("[ATA] Drive is not standard ATA (ATAPI/SATA/etc).\n");
        return false;
    }

    if (!ata_wait_drq()) {
        serial_puts("[ATA] Drive DRQ timeout on IDENTIFY.\n");
        return false;
    }

    // Read 256 words (512 bytes) of IDENTIFY data
    uint16_t identify_buf[256];
    for (int i = 0; i < 256; i++) {
        // Read 16-bit word from port 0x1F0
        uint16_t word;
        __asm__ volatile ("inw %1, %0" : "=a"(word) : "Nd"((uint16_t)ATA_PRIMARY_DATA));
        identify_buf[i] = word;
    }

    g_ata_master_present = true;
    serial_puts("[ATA] Primary Master ATA Hard Disk detected and initialized (512-byte sectors).\n");
    return true;
}

bool ata_read_sector(uint32_t lba, uint8_t *buffer) {
    if (!buffer) return false;

    // Send LBA28 address and drive select (0xE0 | (master) | top 4 bits of LBA)
    outb(ATA_PRIMARY_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_PRIMARY_ERROR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, 1); // Read 1 sector
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_READ_PIO);

    if (!ata_wait_drq()) {
        return false;
    }

    uint16_t *buf16 = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        uint16_t word;
        __asm__ volatile ("inw %1, %0" : "=a"(word) : "Nd"((uint16_t)ATA_PRIMARY_DATA));
        buf16[i] = word;
    }

    return true;
}

bool ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    if (!buffer) return false;

    outb(ATA_PRIMARY_DRIVE_HEAD, (uint8_t)(0xE0 | ((lba >> 24) & 0x0F)));
    outb(ATA_PRIMARY_ERROR, 0x00);
    outb(ATA_PRIMARY_SECCOUNT, 1);
    outb(ATA_PRIMARY_LBA_LO, (uint8_t)(lba & 0xFF));
    outb(ATA_PRIMARY_LBA_MID, (uint8_t)((lba >> 8) & 0xFF));
    outb(ATA_PRIMARY_LBA_HI, (uint8_t)((lba >> 16) & 0xFF));
    outb(ATA_PRIMARY_COMMAND, ATA_CMD_WRITE_PIO);

    if (!ata_wait_drq()) {
        return false;
    }

    const uint16_t *buf16 = (const uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        uint16_t word = buf16[i];
        __asm__ volatile ("outw %0, %1" : : "a"(word), "Nd"((uint16_t)ATA_PRIMARY_DATA));
    }

    // Flush cache command (0xE7)
    outb(ATA_PRIMARY_COMMAND, 0xE7);
    ata_wait_ready();

    return true;
}
