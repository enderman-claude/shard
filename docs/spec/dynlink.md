# Dynamic linking (planned, stubs only)

Status: not implemented. The stubs in `include/shard/shard_link.h` reserve the
interface. Until this is built, any image that asks for imports is rejected.

## Principle
The kernel owns all symbol resolution. A SHARD image never names a library or a
file. It names symbols, and the loader asks the OS to supply each address.

## Planned SHARD v2 additions
- Header `flags` bit 0: `HAS_IMPORTS`. v1 requires `flags` to be 0, so a v1 loader
  rejects v2 images instead of silently ignoring imports.
- Import table: one record per imported symbol.
  - `slot_off` (8 bytes): image-relative address of an 8-byte slot to patch.
  - `name_off` (4), `name_len` (4): name in a string table inside the image.
  - `kind` (4): 1 = function address, 2 = data address.
- The loader calls a resolver `int resolve(name, len, &addr, ctx)` for each import
  and writes `addr` into the slot. A missing symbol fails the whole load.

## Export table (not decided)
Images that provide symbols to other images are not planned yet. The kernel may
expose a fixed table of its own functions, and that is the first thing to decide.

## ELF
ELF `DT_NEEDED` entries (shared libraries) are rejected. Undefined symbols are
rejected. An ELF with imports should be converted to SHARD with imports, once v2 exists.
