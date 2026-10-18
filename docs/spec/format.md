# SHARD v1 binary format

SHARD is the native executable format of tesseraOS. It is position-independent:
an image is a flat block of bytes plus a list of relocations, and the loader places
it at any base address.

All integers are little-endian. Offsets in the header and section table are
byte offsets from the start of the image.

## Header (64 bytes)

| Offset | Size | Field | Meaning |
|---|---|---|---|
| 0 | 4 | magic | `SHRD` |
| 4 | 2 | version | `1` |
| 6 | 2 | machine | `62` (x86_64, same value as ELF `e_machine`) |
| 8 | 4 | flags | reserved, must be 0 |
| 12 | 8 | entry | entry point, image-relative, must be less than `image_size` |
| 20 | 8 | image_size | size in memory of the whole image (bytes) |
| 28 | 4 | section_count | at most 64 |
| 32 | 4 | section_off | offset of the section table |
| 36 | 4 | reloc_count | number of relocation records |
| 40 | 4 | reloc_off | offset of the relocation table |
| 44 | 20 | reserved | must be 0 |

## Section table (40 bytes per entry)

| Offset | Size | Field |
|---|---|---|
| 0 | 4 | type: 1 = code, 2 = data, 3 = bss |
| 4 | 4 | flags: bit0 read, bit1 write, bit2 execute |
| 8 | 8 | file_off: offset in the image where the bytes are stored |
| 16 | 8 | vaddr: image-relative address where the section is placed |
| 24 | 8 | file_size: bytes stored in the image |
| 32 | 8 | mem_size: bytes occupied in memory, at least `file_size` |

Rules:
- Sections must lie within `image_size` and must not overlap in memory.
- Bytes in memory beyond `file_size` (up to `mem_size`) are zero.
- A bss section has `file_size` 0.

## Relocation record (24 bytes)

| Offset | Size | Field |
|---|---|---|
| 0 | 8 | offset: image-relative address of an 8-byte slot to patch |
| 8 | 4 | type: 1 = RELATIVE |
| 12 | 4 | reserved, must be 0 |
| 16 | 8 | addend |

RELATIVE: the slot becomes `load_base + addend`.
The slot must lie inside a writable section.

## Loading
1. Validate the header and every section and relocation (see `shard_open`).
2. Zero `image_size` bytes at the destination.
3. Copy each section's `file_size` bytes to `vaddr`.
4. Apply relocations with the runtime base address.

## Not in v1
Symbols, imports, dynamic linking, compression, signatures.
