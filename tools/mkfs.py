#!/usr/bin/env python3
"""Build a RaptorFS disk image.

usage: mkfs.py OUT.img SIZE_MIB ROOTDIR [/name=host/path ...]
Every file under ROOTDIR is stored as "/<relative path>"; extra /name=path args add more.
"""
import os, struct, sys

SECTOR = 512
DIR_ENTRIES = 64
DIR_SECTORS = DIR_ENTRIES * 64 // SECTOR      # 8
DATA_START = 1 + DIR_SECTORS


def main():
    out, size_mib, root, *extra = sys.argv[1:]
    total = int(size_mib) * 1024 * 1024 // SECTOR
    files = []
    for dirpath, _, names in os.walk(root):
        for n in sorted(names):
            full = os.path.join(dirpath, n)
            files.append(("/" + os.path.relpath(full, root).replace(os.sep, "/"), full))
    for e in extra:
        name, path = e.split("=", 1)
        files.append((name, path))
    if len(files) > DIR_ENTRIES:
        sys.exit("too many files")

    img = bytearray(total * SECTOR)
    entries = bytearray(DIR_ENTRIES * 64)
    nxt = DATA_START
    for i, (name, path) in enumerate(files):
        data = open(path, "rb").read()
        sectors = (len(data) + SECTOR - 1) // SECTOR
        if nxt + sectors > total:
            sys.exit("image too small")
        img[nxt * SECTOR: nxt * SECTOR + len(data)] = data
        entries[i * 64:(i + 1) * 64] = struct.pack("<48sIIII", name.encode(), nxt, len(data), sectors, 1)
        nxt += sectors
    img[0:32] = struct.pack("<8sIIIIII", b"RAPTORFS", 1, total, 1, DIR_SECTORS, DATA_START, nxt)
    img[SECTOR:SECTOR + len(entries)] = entries
    open(out, "wb").write(img)
    print(f"{out}: {len(files)} files, {nxt - DATA_START} data sectors used")


main()
