#ifndef SHARD_OPEN_H
#define SHARD_OPEN_H
#include "shard/types.h"

struct shard_image {
    const uint8_t *data;
    size_t len;
    uint32_t flags;
    uint64_t entry;
    uint64_t image_size;
    uint32_t sec_count;
    uint32_t sec_off;
    uint32_t rel_count;
    uint32_t rel_off;
};

struct shard_section {
    uint32_t type;
    uint32_t flags;
    uint64_t file_off;
    uint64_t vaddr;
    uint64_t file_size;
    uint64_t mem_size;
};

struct shard_reloc {
    uint64_t offset;
    uint32_t type;
    int64_t addend;
};

/* Validates the whole image. Returns SHARD_OK or an error status. */
int shard_open(const uint8_t *data, size_t len, struct shard_image *img);

/* Reads section i (already validated by shard_open). */
int shard_section_get(const struct shard_image *img, uint32_t i, struct shard_section *out);

/* Reads relocation i (already validated by shard_open). */
int shard_reloc_get(const struct shard_image *img, uint32_t i, struct shard_reloc *out);

#endif
