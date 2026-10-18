#include "shard/shard_open.h"
#include "shard/le_read.h"

int shard_section_get(const struct shard_image *img, uint32_t i, struct shard_section *out)
{
    if (i >= img->sec_count) {
        return SHARD_ERR_INDEX;
    }
    const uint8_t *p = img->data + img->sec_off + (uint64_t)i * SHARD_SEC_SIZE;
    out->type = le_read32(p);
    out->flags = le_read32(p + 4);
    out->file_off = le_read64(p + 8);
    out->vaddr = le_read64(p + 16);
    out->file_size = le_read64(p + 24);
    out->mem_size = le_read64(p + 32);
    return SHARD_OK;
}
