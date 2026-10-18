#include "shard/shard_open.h"
#include "shard/le_read.h"

static int range_ok(uint64_t off, uint64_t size, uint64_t limit)
{
    return off <= limit && size <= limit - off;
}

int shard_open(const uint8_t *data, size_t len, struct shard_image *img)
{
    if (len < SHARD_HDR_SIZE) {
        return SHARD_ERR_TOO_SHORT;
    }
    if (data[0] != 'S' || data[1] != 'H' || data[2] != 'R' || data[3] != 'D') {
        return SHARD_ERR_MAGIC;
    }
    if (le_read16(data + 4) != SHARD_VERSION) {
        return SHARD_ERR_VERSION;
    }
    if (le_read16(data + 6) != SHARD_MACHINE_X86_64) {
        return SHARD_ERR_MACHINE;
    }

    img->data = data;
    img->len = len;
    img->flags = le_read32(data + 8);
    img->entry = le_read64(data + 12);
    img->image_size = le_read64(data + 20);
    img->sec_count = le_read32(data + 28);
    img->sec_off = le_read32(data + 32);
    img->rel_count = le_read32(data + 36);
    img->rel_off = le_read32(data + 40);

    if (img->image_size == 0 || img->image_size > 0xFFFFFFFFull) {
        return SHARD_ERR_RANGE;
    }
    if (img->entry >= img->image_size) {
        return SHARD_ERR_RANGE;
    }
    if (img->sec_count > SHARD_MAX_SECTIONS) {
        return SHARD_ERR_SECTION;
    }
    if (!range_ok(img->sec_off, (uint64_t)img->sec_count * SHARD_SEC_SIZE, len)) {
        return SHARD_ERR_RANGE;
    }
    if (!range_ok(img->rel_off, (uint64_t)img->rel_count * SHARD_REL_SIZE, len)) {
        return SHARD_ERR_RANGE;
    }

    for (uint32_t i = 0; i < img->sec_count; i++) {
        struct shard_section s;
        int rc = shard_section_get(img, i, &s);
        if (rc != SHARD_OK) {
            return rc;
        }
        if (s.type < SHARD_SEC_CODE || s.type > SHARD_SEC_BSS) {
            return SHARD_ERR_SECTION;
        }
        if (s.mem_size == 0 || s.file_size > s.mem_size) {
            return SHARD_ERR_SECTION;
        }
        if (s.type == SHARD_SEC_BSS && s.file_size != 0) {
            return SHARD_ERR_SECTION;
        }
        if (!range_ok(s.file_off, s.file_size, len)) {
            return SHARD_ERR_RANGE;
        }
        if (!range_ok(s.vaddr, s.mem_size, img->image_size)) {
            return SHARD_ERR_RANGE;
        }
        for (uint32_t j = 0; j < i; j++) {
            struct shard_section o;
            if (shard_section_get(img, j, &o) != SHARD_OK) {
                return SHARD_ERR_SECTION;
            }
            if (s.vaddr < o.vaddr + o.mem_size && o.vaddr < s.vaddr + s.mem_size) {
                return SHARD_ERR_SECTION;
            }
        }
    }

    for (uint32_t i = 0; i < img->rel_count; i++) {
        struct shard_reloc r;
        int rc = shard_reloc_get(img, i, &r);
        if (rc != SHARD_OK) {
            return rc;
        }
        if (r.type != SHARD_REL_RELATIVE) {
            return SHARD_ERR_RELOC;
        }
        if (!range_ok(r.offset, 8, img->image_size)) {
            return SHARD_ERR_RELOC;
        }
        int inside = 0;
        for (uint32_t k = 0; k < img->sec_count; k++) {
            struct shard_section s;
            shard_section_get(img, k, &s);
            if ((s.flags & SHARD_F_WRITE) && r.offset >= s.vaddr &&
                r.offset + 8 <= s.vaddr + s.mem_size) {
                inside = 1;
                break;
            }
        }
        if (!inside) {
            return SHARD_ERR_RELOC;
        }
    }
    return SHARD_OK;
}
