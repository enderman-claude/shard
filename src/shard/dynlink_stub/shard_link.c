#include "shard/shard_link.h"

int shard_import_count(const struct shard_image *img, uint32_t *count_out)
{
    (void)img;
    (void)count_out;
    return SHARD_ERR_UNSUPPORTED;
}

int shard_link(const struct shard_image *img, uint8_t *dst, size_t dst_len, uint64_t load_base,
               shard_resolve_fn resolve, void *ctx)
{
    (void)img;
    (void)dst;
    (void)dst_len;
    (void)load_base;
    (void)resolve;
    (void)ctx;
    return SHARD_ERR_UNSUPPORTED;
}
