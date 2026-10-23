# shard

SHARD is the native binary format of tesseraOS, and this repo is its reference
implementation. It also contains a small ELF64 loader for the subset tesseraOS
accepts, so both formats load through one path.

- Specs: [docs/spec](docs/spec/README.md)
- Code is freestanding C (no libc). Tests run on the host.

## Layout
One directory per unit, the same rule as tessera:

    src/<subsystem>/<unit>/<unit>.c
    include/shard/<unit>.h   or   include/elf/<unit>.h

Subsystems: `common` (byte reads and writes), `shard` (format), `elf` (ELF subset), `convert` (SHARD to ELF).

## Tools
- `elfpack`: packs a plain ELF64 ET_DYN x86_64 file into SHARD (`elfpack in.elf out.shard`). This is how programs get into SHARD: write C, compile with gcc (static-pie), then pack. There is no SHARD compiler.
- `shardelf`: converts a SHARD image to a plain ELF64 ET_DYN file (`shardelf in.shard out.so`).
  The round trip SHARD -> ELF -> loader is tested in CI, so the two loaders are checked against each other.

## Build and test
    make test

## Related repos

- [tesseraOS](https://github.com/enderman-claude/tesseraOS): the OS that uses SHARD as its native binary format (`lib/shard`).
- [tessera](https://github.com/enderman-claude/tessera): the kernel whose L2 loader runs SHARD images.
- [ember](https://github.com/enderman-claude/ember): the Ember compiler. `--target shard` builds programs that `elfpack` packs into SHARD.

