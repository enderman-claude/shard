/* shardelf: convert a SHARD image to a plain ELF64 ET_DYN file.
 * Usage: shardelf <input.shard> <output.so> */
#include <stdio.h>
#include <stdlib.h>
#include "shard/shard_open.h"
#include "convert/shard_to_elf.h"

static uint8_t *read_all(const char *path, size_t *len)
{
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (n < 0) { fclose(f); return NULL; }
    uint8_t *buf = malloc((size_t)n);
    if (!buf) { fclose(f); return NULL; }
    *len = fread(buf, 1, (size_t)n, f);
    fclose(f);
    return buf;
}

int main(int argc, char **argv)
{
    if (argc != 3) {
        fprintf(stderr, "usage: %s <input.shard> <output.so>\n", argv[0]);
        return 2;
    }
    size_t in_len = 0;
    uint8_t *in = read_all(argv[1], &in_len);
    if (!in) {
        fprintf(stderr, "shardelf: cannot read %s\n", argv[1]);
        return 1;
    }
    struct shard_image img;
    int rc = shard_open(in, in_len, &img);
    if (rc != SHARD_OK) {
        fprintf(stderr, "shardelf: invalid SHARD image (status %d)\n", rc);
        return 1;
    }
    size_t cap = 0;
    shard_to_elf_size(&img, &cap);
    uint8_t *out = malloc(cap);
    size_t written = 0;
    if (!out || shard_to_elf(&img, out, cap, &written) != SHARD_OK) {
        fprintf(stderr, "shardelf: conversion failed\n");
        return 1;
    }
    FILE *f = fopen(argv[2], "wb");
    if (!f || fwrite(out, 1, written, f) != written) {
        fprintf(stderr, "shardelf: cannot write %s\n", argv[2]);
        return 1;
    }
    fclose(f);
    free(out);
    free(in);
    return 0;
}
