#include "convert/shard_to_elf.h"
#include "shard/le_write.h"

#define EHDR_SIZE 64
#define PHDR_SIZE 56
#define DYN_ENTRY 16
#define RELA_SIZE 24
#define DYN_BYTES (4 * DYN_ENTRY)

#define PT_LOAD 1
#define PT_DYNAMIC 2
#define PF_X 1
#define PF_W 2
#define PF_R 4
#define DT_NULL 0
#define DT_RELA 7
#define DT_RELASZ 8
#define DT_RELAENT 9
#define R_X86_64_RELATIVE 8
#define EM_X86_64 62
#define ET_DYN 3

struct plan {
    uint32_t nseg;
    uint64_t sec_base;   /* file offset of the first section's bytes */
    uint64_t dyn_off;    /* file offset of the dynamic table */
    uint64_t dyn_vaddr;  /* memory address of the dynamic table */
    uint64_t total;      /* output size */
    int anchor;          /* 1 if a zero-size PT_LOAD at vaddr 0 is needed */
};

/* A section at vaddr 0 already pins the loader's base. Otherwise add a zero-size anchor at 0. */
static int needs_anchor(const struct shard_image *img)
{
    for (uint32_t i = 0; i < img->sec_count; i++) {
        struct shard_section s;
        shard_section_get(img, i, &s);
        if (s.vaddr == 0) return 0;
    }
    return 1;
}

#define PAGE 4096u

static uint64_t align8(uint64_t v)
{
    return (v + 7) & ~(uint64_t)7;
}

/* Linux maps a PT_LOAD by page: the file offset must be congruent to the virtual address
 * modulo the page size. Returns the first offset >= cursor with off % PAGE == vaddr % PAGE. */
static uint64_t place(uint64_t cursor, uint64_t vaddr)
{
    uint64_t want = vaddr % PAGE;
    uint64_t have = cursor % PAGE;
    return cursor + ((want + PAGE - have) % PAGE);
}

static void make_plan(const struct shard_image *img, struct plan *p)
{
    p->anchor = needs_anchor(img);
    p->nseg = img->sec_count + 2 + (p->anchor ? 1 : 0); /* [anchor], sections, dyn data, PT_DYNAMIC */
    p->sec_base = EHDR_SIZE + (uint64_t)p->nseg * PHDR_SIZE;
    uint64_t cursor = p->sec_base;
    for (uint32_t i = 0; i < img->sec_count; i++) {
        struct shard_section s;
        shard_section_get(img, i, &s);
        cursor = place(cursor, s.vaddr) + align8(s.file_size);
    }
    /* The dynamic table gets pages of its own, so no two segments share a page. */
    p->dyn_vaddr = (img->image_size + PAGE - 1) & ~(uint64_t)(PAGE - 1);
    p->dyn_off = place(cursor, p->dyn_vaddr);
    p->total = p->dyn_off + DYN_BYTES + (uint64_t)img->rel_count * RELA_SIZE;
}

/* Output offset of section i. Sections are stored in order, each padded to 8 bytes. */
static uint64_t sec_out_off(const struct shard_image *img, const struct plan *p, uint32_t i)
{
    uint64_t off = p->sec_base;
    for (uint32_t j = 0; j <= i; j++) {
        struct shard_section s;
        shard_section_get(img, j, &s);
        off = place(off, s.vaddr);
        if (j < i) off += align8(s.file_size);
    }
    return off;
}

static uint32_t elf_flags(uint32_t shard_flags)
{
    uint32_t f = 0;
    if (shard_flags & SHARD_F_READ) f |= PF_R;
    if (shard_flags & SHARD_F_WRITE) f |= PF_W;
    if (shard_flags & SHARD_F_EXEC) f |= PF_X;
    return f;
}

/* Linux maps whole pages, so two segments on one page share one mapping. Each section's flags
 * are the union of every section that shares a page with it. Permissions within a shared page
 * are therefore weaker than the SHARD flags say. */
static uint32_t page_flags(const struct shard_image *img, uint32_t i)
{
    struct shard_section a;
    shard_section_get(img, i, &a);
    uint64_t a_lo = a.vaddr / PAGE, a_hi = (a.vaddr + a.mem_size - 1) / PAGE;
    uint32_t f = elf_flags(a.flags);
    for (uint32_t j = 0; j < img->sec_count; j++) {
        if (j == i) continue;
        struct shard_section b;
        shard_section_get(img, j, &b);
        uint64_t b_lo = b.vaddr / PAGE, b_hi = (b.vaddr + b.mem_size - 1) / PAGE;
        if (a_lo <= b_hi && b_lo <= a_hi) f |= elf_flags(b.flags);
    }
    return f;
}

static void put_phdr(uint8_t *ph, uint32_t type, uint32_t flags, uint64_t off, uint64_t vaddr,
                     uint64_t filesz, uint64_t memsz, uint64_t align)
{
    le_write32(ph, type);
    le_write32(ph + 4, flags);
    le_write64(ph + 8, off);
    le_write64(ph + 16, vaddr);
    le_write64(ph + 24, vaddr);
    le_write64(ph + 32, filesz);
    le_write64(ph + 40, memsz);
    le_write64(ph + 48, align);
}

int shard_to_elf_size(const struct shard_image *img, size_t *size_out)
{
    struct plan p;
    make_plan(img, &p);
    *size_out = (size_t)p.total;
    return SHARD_OK;
}

int shard_to_elf(const struct shard_image *img, uint8_t *out, size_t cap, size_t *written)
{
    struct plan p;
    make_plan(img, &p);
    if (p.total > cap) {
        return SHARD_ERR_DEST;
    }
    for (uint64_t i = 0; i < p.total; i++) {
        out[i] = 0;
    }

    /* ELF header */
    out[0] = 0x7F; out[1] = 'E'; out[2] = 'L'; out[3] = 'F';
    out[4] = 2; /* ELFCLASS64 */
    out[5] = 1; /* little-endian */
    out[6] = 1; /* version */
    le_write16(out + 16, ET_DYN);
    le_write16(out + 18, EM_X86_64);
    le_write32(out + 20, 1);
    le_write64(out + 24, img->entry);
    le_write64(out + 32, EHDR_SIZE);
    le_write16(out + 52, EHDR_SIZE);
    le_write16(out + 54, PHDR_SIZE);
    le_write16(out + 56, (uint16_t)p.nseg);
    le_write16(out + 58, 64); /* e_shentsize, no section table */

    uint8_t *ph = out + EHDR_SIZE;
    if (p.anchor) {
        /* anchor so the loader's lowest vaddr is 0, matching SHARD addressing */
        put_phdr(ph, PT_LOAD, PF_R, 0, 0, 0, 0, PAGE);
        ph += PHDR_SIZE;
    }

    for (uint32_t i = 0; i < img->sec_count; i++) {
        struct shard_section s;
        shard_section_get(img, i, &s);
        uint64_t off = sec_out_off(img, &p, i);
        put_phdr(ph, PT_LOAD, page_flags(img, i), off, s.vaddr, s.file_size, s.mem_size, PAGE);
        for (uint64_t k = 0; k < s.file_size; k++) {
            out[off + k] = img->data[s.file_off + k];
        }
        ph += PHDR_SIZE;
    }

    uint64_t rela_vaddr = p.dyn_vaddr + DYN_BYTES;
    uint64_t dyn_bytes = DYN_BYTES + (uint64_t)img->rel_count * RELA_SIZE;
    put_phdr(ph, PT_LOAD, PF_R | PF_W, p.dyn_off, p.dyn_vaddr, dyn_bytes, dyn_bytes, PAGE);
    ph += PHDR_SIZE;
    put_phdr(ph, PT_DYNAMIC, PF_R | PF_W, p.dyn_off, p.dyn_vaddr, DYN_BYTES, DYN_BYTES, 8);

    /* dynamic table: DT_RELA, DT_RELASZ, DT_RELAENT, DT_NULL */
    uint8_t *d = out + p.dyn_off;
    le_write64(d, DT_RELA);          le_write64(d + 8, rela_vaddr);
    le_write64(d + 16, DT_RELASZ);   le_write64(d + 24, (uint64_t)img->rel_count * RELA_SIZE);
    le_write64(d + 32, DT_RELAENT);  le_write64(d + 40, RELA_SIZE);
    le_write64(d + 48, DT_NULL);     le_write64(d + 56, 0);

    for (uint32_t i = 0; i < img->rel_count; i++) {
        struct shard_reloc r;
        shard_reloc_get(img, i, &r);
        uint8_t *e = out + p.dyn_off + DYN_BYTES + (uint64_t)i * RELA_SIZE;
        le_write64(e, r.offset);
        le_write64(e + 8, (uint64_t)R_X86_64_RELATIVE);
        le_write64(e + 16, (uint64_t)r.addend);
    }

    *written = (size_t)p.total;
    return SHARD_OK;
}
