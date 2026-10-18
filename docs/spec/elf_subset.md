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
