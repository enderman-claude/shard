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

Subsystems: `common` (byte reads), `shard` (format), `elf` (ELF subset).

## Build and test
    make test
