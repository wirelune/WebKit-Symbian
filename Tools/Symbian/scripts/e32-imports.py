#!/usr/bin/env python3
"""Writes the DLL names in an E32 image's imports as Symbian's tools do.

Usage: e32-imports.py IMAGE

The SDK's converter names imported DLLs plainly ("euser.dll"). Symbian^3
Anna's loader takes a name without a version for version 0, finds no
compatible DLL and fails the whole image with KErrNotSupported (-5); Belle
accepts it. elf2e32 writes "euser{000a0000}[100039e5].dll", which both load:
the names come from e32-dll-names.txt. A DLL missing there gets the usual
version, {000a0000}. The image must be uncompressed, as the converter
writes it; the header CRC is updated.
"""
import os
import struct
import sys

HEADER_CRC = 0x14
DLL_REF_COUNT = 0x54
CODE_OFFSET = 0x64
IMPORT_OFFSET = 0x6C
CODE_RELOC_OFFSET = 0x70
DATA_RELOC_OFFSET = 0x74
UNCOMPRESSED_SIZE = 0x7C
COMPRESSION = 0x1C
CRC_INITIALISER = 0xC90FDAA2


def header_crc(image):
    """Symbian's Mem::Crc32 of the header, as in e32-header.py."""
    header = bytearray(image[: struct.unpack_from("<I", image, CODE_OFFSET)[0]])
    struct.pack_into("<I", header, HEADER_CRC, CRC_INITIALISER)
    crc = 0
    for byte in header:
        crc ^= byte
        for _ in range(8):
            crc = (crc >> 1) ^ (0xEDB88320 if crc & 1 else 0)
    return crc


def known_names():
    names = {}
    path = os.path.join(os.path.dirname(os.path.abspath(__file__)), "e32-dll-names.txt")
    for line in open(path, encoding="utf-8"):
        line = line.strip()
        if line and not line.startswith("#"):
            plain, decorated = line.split()
            names[plain.lower()] = decorated
    return names


def decorate(name, names):
    if "{" in name:
        return name
    known = names.get(name.lower())
    if known:
        return known
    print(f"e32-imports: {name} is not in e32-dll-names.txt, using version 000a0000", file=sys.stderr)
    root, _, extension = name.rpartition(".")
    return f"{root}{{000a0000}}.{extension}"


def main(path):
    image = bytearray(open(path, "rb").read())
    word = lambda offset: struct.unpack_from("<I", image, offset)[0]
    if image[0x10:0x14] != b"EPOC":
        sys.exit(f"{path}: not an E32 image")
    if word(COMPRESSION):
        sys.exit(f"{path}: compressed; e32-imports.py needs the converter's uncompressed image")
    imports, count = word(IMPORT_OFFSET), word(DLL_REF_COUNT)
    if not imports or not count:
        return
    names = known_names()

    # E32ImportSection: its size, then per DLL the name's offset (from the
    # section's start), the number of imports and their code offsets, then
    # the names.
    blocks = []
    position = imports + 4
    for _ in range(count):
        name_offset, entries = struct.unpack_from("<Ii", image, position)
        name = image[imports + name_offset:].split(b"\0", 1)[0].decode()
        blocks.append((decorate(name, names), entries, image[position + 8 : position + 8 + 4 * entries]))
        position += 8 + 4 * entries
    table_size = 4 + sum(8 + len(offsets) for _, _, offsets in blocks)
    table = b""
    strings = b""
    for name, entries, offsets in blocks:
        table += struct.pack("<Ii", table_size + len(strings), entries) + offsets
        strings += name.encode() + b"\0"
    body = table + strings
    body += b"\0" * (-(len(body) + 4) % 4)
    section = struct.pack("<I", len(body) + 4) + body

    # The relocation sections follow the imports; they move with the change.
    following = [word(field) for field in (CODE_RELOC_OFFSET, DATA_RELOC_OFFSET) if word(field)]
    end = min(following) if following else len(image)
    if end < imports:
        sys.exit(f"{path}: the import section is not before the relocations")
    delta = len(section) - (end - imports)
    image[imports:end] = section
    for field in (CODE_RELOC_OFFSET, DATA_RELOC_OFFSET):
        if word(field):
            struct.pack_into("<I", image, field, word(field) + delta)
    struct.pack_into("<I", image, UNCOMPRESSED_SIZE, len(image) - word(CODE_OFFSET))
    struct.pack_into("<I", image, HEADER_CRC, header_crc(image))
    open(path, "wb").write(image)
    print(f"{path}: {count} imported DLLs named for the loader")


if __name__ == "__main__":
    main(sys.argv[1])
