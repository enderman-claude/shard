#include "shard/shard_open.h"
#include "shard/le_read.h"

int shard_reloc_get(const struct shard_image *img, uint32_t i, struct shard_reloc *out)
{
    if (i >= img->rel_count) {
        return SHARD_ERR_INDEX;
    }
    const uint8_t *p = img->data + img->rel_off + (uint64_t)i * SHARD_REL_SIZE;
    out->offset = le_read64(p);
    out->type = le_read32(p + 8);
    out->addend = (int64_t)le_read64(p + 16);
    return SHARD_OK;
}
