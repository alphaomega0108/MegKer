/* arch/x86_64/drivers/ata.c
 * ATA PIO — primary bus (ports 0x1F0-0x1F7), master drive, LBA28,
 * polling. No IRQ handling: PIO reads just poll the status register,
 * which is the standard simple approach and fine for a boot-time
 * read-only filesystem.
 */

#include <arch/x86_64/ata.h>
#include <arch/x86_64/io.h>

#define ATA_DATA       0x1F0
#define ATA_SECCOUNT   0x1F2
#define ATA_LBA_LOW    0x1F3
#define ATA_LBA_MID    0x1F4
#define ATA_LBA_HIGH   0x1F5
#define ATA_DRIVE      0x1F6
#define ATA_STATUS     0x1F7
#define ATA_COMMAND    0x1F7

#define ATA_SR_BSY     0x80
#define ATA_SR_DRQ     0x08
#define ATA_SR_ERR     0x01

#define ATA_CMD_READ_SECTORS 0x20

static bool disk_present = false;

static void ata_delay(void)
{
    /* ~400ns: four wasted status reads, the standard trick. */
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
    inb(ATA_STATUS);
}

static void ata_wait_bsy_clear(void)
{
    while (inb(ATA_STATUS) & ATA_SR_BSY) { }
}

static bool ata_wait_drq(void)
{
    for (int i = 0; i < 100000; i++) {
        u8 status = inb(ATA_STATUS);
        if (status & ATA_SR_ERR)
            return false;
        if (status & ATA_SR_DRQ)
            return true;
    }
    return false;
}

void ata_init(void)
{
    outb(ATA_DRIVE, 0xE0);   /* select master, LBA mode */
    ata_delay();

    u8 status = inb(ATA_STATUS);
    disk_present = (status != 0xFF);   /* 0xFF = floating bus, no drive */
}

bool ata_available(void)
{
    return disk_present;
}

bool ata_read_sector(u32 lba, u8* buf)
{
    if (!disk_present)
        return false;

    ata_wait_bsy_clear();
    outb(ATA_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECCOUNT, 1);
    outb(ATA_LBA_LOW,  lba & 0xFF);
    outb(ATA_LBA_MID,  (lba >> 8)  & 0xFF);
    outb(ATA_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(ATA_COMMAND, ATA_CMD_READ_SECTORS);

    if (!ata_wait_drq())
        return false;

    u16* p = (u16*)buf;
    for (int i = 0; i < 256; i++)
        p[i] = inw(ATA_DATA);

    return true;
}
