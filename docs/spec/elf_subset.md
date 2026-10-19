# ELF subset supported by tesseraOS

This is plain ELF64, little-endian, for x86_64. It is not GNU ELF: GNU hash tables,
symbol versioning, and GNU program headers are ignored and not required.

Accepted:
- `e_type` = `ET_DYN` (position-independent executables and shared objects).
- `PT_LOAD` segments (copied to memory; `p_memsz` beyond `p_filesz` is zeroed).
- `PT_DYNAMIC`, read to find the relocation table (`DT_RELA`, `DT_RELASZ`, `DT_RELAENT`).
- Relocation type `R_X86_64_RELATIVE` (8) with symbol index 0: value is `base + addend`.
- Relocation type `R_X86_64_64` (1) against a symbol defined in this image
  (`st_shndx` non-zero): value is `base + st_value + addend`. Found through `DT_SYMTAB`.

Rejected with an error:
- Any `DT_NEEDED` entry (the image depends on a shared library).
- `ET_EXEC` (fixed addresses are not supported).
- Any other relocation type, and any symbol that is undefined (no dynamic linking).
- Segments that fall outside the file.

The loader places the image at a chosen base. Segments are laid out at their
`p_vaddr` relative to the lowest `p_vaddr`.

## Converter output (SHARDELF) and Linux
The converter's output is Linux-runnable: a SHARD image with no relocations converts to an
ET_DYN that the kernel can execute. Layout rules that make this work:
- Each PT_LOAD's file offset is congruent to its virtual address modulo 4096.
- The dynamic table has its own page after the image.
- Segments that share a page take the union of their permissions, since Linux maps whole pages.
  Within a shared page the SHARD permissions are therefore weaker than declared.
- A zero-size PT_LOAD at vaddr 0 is added only when no section starts at 0.
Relocations are applied by the loader, not by the kernel, so Linux runs an image with
relocations only if it applies them itself. The shard tests check a relocation-free image.
