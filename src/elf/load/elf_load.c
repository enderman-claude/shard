#include "elf/elf_load.h"
#include "shard/le_read.h"

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define DT_NULL 0
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define DT_SYMTAB 6
#define DT_NEEDED 1
#define ELF_ERR_NEEDED -10
#define R_X86_64_64 1
#define R_X86_64_RELATIVE 8
#define ELF_ERR_RANGE -6
#define ELF_ERR_SEGMENT -7
#define ELF_ERR_RELOC -8
#define ELF_ERR_DEST -9

static const uint8_t *phdr(const struct elf_image *img, uint16_t i)
{
    return img->data + img->phoff + (uint64_t)i * 56;
}

static void put64(uint8_t *p, uint64_t v)
{
    for (int b = 0; b < 8; b++) {
        p[b] = (uint8_t)(v >> (8 * b));
    }
}

/* Computes the lowest vaddr and the span covering all PT_LOAD segments. */
static int measure(const struct elf_image *img, uint64_t *min_out, uint64_t *span_out)
{
    uint64_t lo = UINT64_MAX, hi = 0;
    int any = 0;
    for (uint16_t i = 0; i < img->phnum; i++) {
        const uint8_t *p = phdr(img, i);
        if (le_read32(p) != PT_LOAD) {
            continue;
        }
        uint64_t vaddr = le_read64(p + 16);
        uint64_t memsz = le_read64(p + 40);
        if (vaddr + memsz < vaddr) {
            return ELF_ERR_RANGE;
        }
        if (vaddr < lo) lo = vaddr;
        if (vaddr + memsz > hi) hi = vaddr + memsz;
        any = 1;
    }
    if (!any) {
        return ELF_ERR_SEGMENT;
    }
    *min_out = lo;
    *span_out = hi - lo;
    return 0;
}

static int copy_segments(const struct elf_image *img, uint8_t *dst, size_t dst_len, uint64_t min)
{
    for (uint16_t i = 0; i < img->phnum; i++) {
        const uint8_t *p = phdr(img, i);
        if (le_read32(p) != PT_LOAD) {
            continue;
        }
        uint64_t off = le_read64(p + 8);
        uint64_t vaddr = le_read64(p + 16);
        uint64_t filesz = le_read64(p + 32);
        uint64_t memsz = le_read64(p + 40);
        if (filesz > memsz) {
            return ELF_ERR_SEGMENT;
        }
        if (off > img->len || filesz > img->len - off) {
            return ELF_ERR_SEGMENT;
        }
        if (vaddr - min > dst_len || memsz > dst_len - (vaddr - min)) {
            return ELF_ERR_DEST;
        }
        uint8_t *d = dst + (vaddr - min);
        for (uint64_t k = 0; k < memsz; k++) {
            d[k] = k < filesz ? img->data[off + k] : 0;
        }
    }
    return 0;
}

/* Finds PT_DYNAMIC in the loaded image and applies RELA entries. */
static int apply_relocs(const struct elf_image *img, uint8_t *dst, size_t dst_len,
                        uint64_t min, uint64_t load_base)
{
    uint64_t dyn_vaddr = 0, dyn_size = 0;
    int have_dyn = 0;
    for (uint16_t i = 0; i < img->phnum; i++) {
        const uint8_t *p = phdr(img, i);
        if (le_read32(p) == PT_DYNAMIC) {
            dyn_vaddr = le_read64(p + 16);
            dyn_size = le_read64(p + 40);
            have_dyn = 1;
        }
    }
    if (!have_dyn) {
        return 0;
    }
    if (dyn_vaddr - min > dst_len || dyn_size > dst_len - (dyn_vaddr - min)) {
        return ELF_ERR_RANGE;
    }
    uint64_t rela = 0, relasz = 0, relaent = 24, symtab = 0;
    for (uint64_t off = 0; off + 16 <= dyn_size; off += 16) {
        const uint8_t *e = dst + (dyn_vaddr - min) + off;
        uint64_t tag = le_read64(e);
        uint64_t val = le_read64(e + 8);
        if (tag == DT_NULL) break;
        if (tag == DT_NEEDED) return ELF_ERR_NEEDED; /* shared library dependency: no dynamic linking */
        if (tag == DT_RELA) rela = val;
        if (tag == DT_RELASZ) relasz = val;
        if (tag == DT_RELAENT) relaent = val;
        if (tag == DT_SYMTAB) symtab = val;
    }
    if (relasz == 0) {
        return 0;
    }
    if (relaent != 24) {
        return ELF_ERR_RELOC;
    }
    if (rela - min > dst_len || relasz > dst_len - (rela - min)) {
        return ELF_ERR_RANGE;
    }
    for (uint64_t off = 0; off + 24 <= relasz; off += 24) {
        const uint8_t *r = dst + (rela - min) + off;
        uint64_t r_offset = le_read64(r);
        uint64_t r_info = le_read64(r + 8);
        uint64_t addend = le_read64(r + 16);
        uint32_t type = (uint32_t)r_info;
        uint64_t sym = r_info >> 32;
        uint64_t value;
        if (type == R_X86_64_RELATIVE && sym == 0) {
            value = load_base + addend;
        } else if (type == R_X86_64_64 && sym != 0) {
            /* Defined symbol in this image only: st_shndx at +6 must be non-zero. */
            if (symtab == 0 || symtab - min > dst_len || (sym + 1) * 24 > dst_len - (symtab - min)) {
                return ELF_ERR_RANGE;
            }
            const uint8_t *se = dst + (symtab - min) + sym * 24;
            if (le_read16(se + 6) == 0) {
                return ELF_ERR_RELOC; /* undefined symbol: no dynamic linking */
            }
            value = load_base + le_read64(se + 8) + addend;
        } else {
            return ELF_ERR_RELOC;
        }
        if (r_offset - min > dst_len || 8 > dst_len - (r_offset - min)) {
            return ELF_ERR_RANGE;
        }
        put64(dst + (r_offset - min), value);
    }
    return 0;
}

int elf_load(const struct elf_image *img, uint8_t *dst, size_t dst_len,
             uint64_t load_base, uint64_t *entry_out, uint64_t *span_out)
{
    uint64_t min = 0, span = 0;
    int rc = measure(img, &min, &span);
    if (rc != 0) {
        return rc;
    }
    if (span > dst_len) {
        return ELF_ERR_DEST;
    }
    for (uint64_t k = 0; k < span; k++) {
        dst[k] = 0;
    }
    rc = copy_segments(img, dst, dst_len, min);
    if (rc != 0) {
        return rc;
    }
    rc = apply_relocs(img, dst, dst_len, min, load_base);
    if (rc != 0) {
        return rc;
    }
    *entry_out = load_base + (img->entry - min);
    *span_out = span;
    return 0;
}
