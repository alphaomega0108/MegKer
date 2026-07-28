/* include/arch/x86_64/ata.h
 * ATA PIO driver — primary bus, master drive, LBA28, polling (no
 * IRQ needed). Read-only: enough to back a simple read-only VFS.
 */

#ifndef ARCH_X86_64_ATA_H
#define ARCH_X86_64_ATA_H

#include <kernel/types.h>

void ata_init(void);
bool ata_available(void);

/* Reads one 512-byte sector into buf. */
bool ata_read_sector(u32 lba, u8* buf);

#endif /* ARCH_X86_64_ATA_H */
