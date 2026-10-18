#ifndef SHARD_LINK_H
#define SHARD_LINK_H
#include "shard/shard_open.h"

/* Stub interface for dynamic linking. See docs/spec/dynlink.md.
 * All functions currently return SHARD_ERR_UNSUPPORTED. */

#define SHARD_IMPORT_FUNC 1
#define SHARD_IMPORT_DATA 2

struct shard_import {
    uint64_t slot_off;
    uint32_t name_off;
    uint32_t name_len;
    uint32_t kind;
};

/* Returns 0 and writes the address for name, or a negative status if unknown. */
typedef int (*shard_resolve_fn)(const char *name, uint32_t len, uint64_t *addr_out, void *ctx);

/* Number of imports in the image. */
int shard_import_count(const struct shard_image *img, uint32_t *count_out);

/* Patches every import slot in dst using resolve. dst must already hold the loaded image. */
int shard_link(const struct shard_image *img, uint8_t *dst, size_t dst_len, uint64_t load_base,
               shard_resolve_fn resolve, void *ctx);

#endif
