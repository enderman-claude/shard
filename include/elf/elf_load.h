#ifndef ELF_LOAD_H
#define ELF_LOAD_H
#include "elf/elf_open.h"

/* Loads PT_LOAD segments into dst and applies R_X86_64_RELATIVE relocations for
 * load_base. dst_len must hold the full image span (max vaddr - min vaddr). */
int elf_load(const struct elf_image *img, uint8_t *dst, size_t dst_len,
             uint64_t load_base, uint64_t *entry_out, uint64_t *span_out);

#endif
