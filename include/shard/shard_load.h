#ifndef SHARD_LOAD_H
#define SHARD_LOAD_H
#include "shard/shard_open.h"

/* Places the image in dst (dst_len bytes, destination memory) and patches relocations
 * as if it runs at load_base. On success *entry_out is the runtime entry address. */
int shard_load(const struct shard_image *img, uint8_t *dst, size_t dst_len,
               uint64_t load_base, uint64_t *entry_out);

#endif
