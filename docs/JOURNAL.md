# Journal

## Session 1
- Created repo. Wrote SHARD v1 spec (docs/spec/format.md) and the plain ELF subset (docs/spec/elf_subset.md).
- Implemented shard_open, shard_section_get, shard_reloc_get, shard_load, and a freestanding le_read.
- Implemented elf_open and elf_load for ET_DYN x86_64: PT_LOAD, PT_DYNAMIC, RELA.
- Found while testing: default-visibility shared objects use R_X86_64_64 against a
  defined symbol, not RELATIVE with symbol 0. Added support for that case (symbol must
  be defined in the image). Undefined symbols are rejected.
- make test: 23 checks pass, including a real gcc-built ELF fixture.
