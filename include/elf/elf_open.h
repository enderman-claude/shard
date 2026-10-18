#ifndef ELF_OPEN_H
#define ELF_OPEN_H
#include <stddef.h>
#include <stdint.h>

struct elf_image {
    const uint8_t *data;
    size_t len;
    uint64_t entry;
    uint64_t phoff;
    uint16_t phnum;
};

/* Checks the ELF64 header for the supported subset (ET_DYN, x86_64, little-endian).
 * Returns 0 on success, negative on error. */
int elf_open(const uint8_t *data, size_t len, struct elf_image *img);

#endif
