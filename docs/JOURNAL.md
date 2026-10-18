# Journal

## Session 1
- Created repo. Wrote SHARD v1 spec (docs/spec/format.md) and the plain ELF subset (docs/spec/elf_subset.md).
- Implemented shard_open, shard_section_get, shard_reloc_get, shard_load, and a freestanding le_read.
- Implemented elf_open and elf_load for ET_DYN x86_64: PT_LOAD, PT_DYNAMIC, RELA.
- Found while testing: default-visibility shared objects use R_X86_64_64 against a
  defined symbol, not RELATIVE with symbol 0. Added support for that case (symbol must
  be defined in the image). Undefined symbols are rejected.
- make test: 23 checks pass, including a real gcc-built ELF fixture.

## Session 2
- Added dynamic-linking stubs (docs/spec/dynlink.md, include/shard/shard_link.h). They return SHARD_ERR_UNSUPPORTED.
- shard_open rejects any header flag bit (v2 feature); v1 reserves flags = 0.
- ELF loader rejects DT_NEEDED. Tested against a fixture linked with libm.
- make test: all checks pass.

## Session 3
- Added SHARELF: src/convert/shard_to_elf turns a validated SHARD image into a plain ELF64 ET_DYN.
  Layout: zero-size anchor at vaddr 0, one PT_LOAD per section, a RW PT_LOAD for the dynamic table and RELA records, and PT_DYNAMIC.
- tools/shardelf wraps it as a command-line tool.
- The round-trip test (SHARD -> ELF -> loader) checks entry address and memory bytes against the SHARD loader.
- CI workflow runs make test and converts the sample with readelf inspection.
