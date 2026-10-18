#include "shard/shard_load.h"
#include "shard/le_read.h"

static void zero_bytes(uint8_t *p, uint64_t n)
{
    for (uint64_t i = 0; i < n; i++) {
        p[i] = 0;
    }
}

static void copy_bytes(uint8_t *dst, const uint8_t *src, uint64_t n)
{
    for (uint64_t i = 0; i < n; i++) {
        dst[i] = src[i];
    }
}

int shard_load(const struct shard_image *img, uint8_t *dst, size_t dst_len,
               uint64_t load_base, uint64_t *entry_out)
{
    if (dst_len < img->image_size) {
        return SHARD_ERR_DEST;
    }
    zero_bytes(dst, img->image_size);

    for (uint32_t i = 0; i < img->sec_count; i++) {
        struct shard_section s;
        int rc = shard_section_get(img, i, &s);
        if (rc != SHARD_OK) {
            return rc;
        }
        copy_bytes(dst + s.vaddr, img->data + s.file_off, s.file_size);
    }

    for (uint32_t i = 0; i < img->rel_count; i++) {
        struct shard_reloc r;
        int rc = shard_reloc_get(img, i, &r);
        if (rc != SHARD_OK) {
            return rc;
        }
        uint64_t value = load_base + (uint64_t)r.addend;
        uint8_t *slot = dst + r.offset;
        for (int b = 0; b < 8; b++) {
            slot[b] = (uint8_t)(value >> (8 * b));
        }
    }

    *entry_out = load_base + img->entry;
    return SHARD_OK;
}
