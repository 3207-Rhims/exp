
#ifndef IOSHA_H
#define IOSHA_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

/* ---------- public duplex state ---------------------------------- */
typedef struct {
    uint64_t st[16];     /* 8 × 64-bit lanes  = 512-bit state   */
    size_t   idx;
    uint32_t rate;            /* byte position in current 64-byte block */
} iosha_ctx;

/* ---------- core helpers ----------------------------------------- */
void iosha_init(iosha_ctx *ctx, uint8_t tag);
void iosha_init_128(iosha_ctx *ctx, uint8_t tag);
void iosha_absorb(iosha_ctx *ctx, const uint8_t *in, size_t inlen);
void iosha_absorb_128(iosha_ctx *ctx, const uint8_t *in, size_t inlen);

void iosha_squeeze(iosha_ctx *ctx, uint8_t *out, size_t outlen);
void iosha_squeeze_128(iosha_ctx *ctx, uint8_t *out, size_t outlen);

/* One-shot convenience wrappers */
void iosha_xof_bytes(const uint8_t *seed, size_t seedlen,
                     uint16_t nonce, uint8_t *out, size_t outlen);
void iosha_crh_bytes(const uint8_t *in, size_t inlen,
                     uint8_t *out, size_t outlen);

#endif /* IOSHA_H */
