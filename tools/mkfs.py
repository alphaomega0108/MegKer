#!/usr/bin/env python3
"""tools/mkfs.py — builds the tiny read-only disk image fs/vfs.c reads.

Usage: mkfs.py <output.img> <file1> [file2 ...]

On-disk format (must match fs/vfs.c exactly):
  Sector 0             : superblock — magic, file_count,
                          dir_start_sector, data_start_sector
                          (all u32 little-endian), zero-padded to 512
  [dir_start_sector..)  : directory entries, 64 bytes each, 8 per
                          sector — name[32], start_sector (u32),
                          size_bytes (u32), reserved[24]
  [data_start_sector..) : file contents, each padded out to a sector
                          boundary
"""

import struct
import sys

SECTOR = 512
MAGIC = 0x4D4B4653  # "MKFS"
DIR_ENTRY_SIZE = 64
NAME_LEN = 32


def main():
    if len(sys.argv) < 3:
        print(f"usage: {sys.argv[0]} <output.img> <file1> [file2 ...]", file=sys.stderr)
        return 1

    out_path = sys.argv[1]
    in_paths = sys.argv[2:]

    file_count = len(in_paths)
    dir_sectors = (file_count * DIR_ENTRY_SIZE + SECTOR - 1) // SECTOR
    dir_start = 1
    data_start = dir_start + dir_sectors

    entries = []
    blobs = []
    cursor = data_start
    for path in in_paths:
        with open(path, "rb") as f:
            data = f.read()
        name = path.split("/")[-1][: NAME_LEN - 1]
        entries.append((name, cursor, len(data)))
        blobs.append(data)
        cursor += (len(data) + SECTOR - 1) // SECTOR

    with open(out_path, "wb") as out:
        superblock = struct.pack("<IIII", MAGIC, file_count, dir_start, data_start)
        out.write(superblock + b"\x00" * (SECTOR - len(superblock)))

        dir_bytes = bytearray()
        for name, start_sector, size in entries:
            name_field = name.encode("ascii").ljust(NAME_LEN, b"\x00")
            entry = name_field + struct.pack("<II", start_sector, size)
            entry = entry.ljust(DIR_ENTRY_SIZE, b"\x00")
            dir_bytes += entry
        dir_bytes += b"\x00" * (dir_sectors * SECTOR - len(dir_bytes))
        out.write(dir_bytes)

        for data in blobs:
            out.write(data)
            pad = (-len(data)) % SECTOR
            out.write(b"\x00" * pad)

    return 0


if __name__ == "__main__":
    sys.exit(main())
