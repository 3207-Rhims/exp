/*  iosha_stream.c  –  SHAKE-compatible stream helpers powered by IOSHA-v2  */
#include <string.h>
#include "symmetric.h"     /* pulls in params.h, iosha.h, fips202.h */

/* ---------- Helper: reinterpret keccak_state buffer as iosha_ctx ------ */
/* keccak_state is 25×64-bit = 200 bytes.  iosha_ctx is 8×64 + idx = 72 B.
   So we can safely overlay an iosha_ctx on top of the keccak_state memory. */
static inline iosha_ctx *as_iosha(keccak_state *st)
{
    return (iosha_ctx *)st;      /* aliasing OK on all modern compilers   */
}

/* ---------------- 128-bit security stream (was SHAKE-128) ------------- */
void dilithium_shake128_stream_init(keccak_state *st,
                                    const uint8_t seed[SEEDBYTES],
                                    uint16_t      nonce)
{
    iosha_ctx *ctx = as_iosha(st);
    iosha_init_128(ctx, 0x01);                       /* domain tag 0x01 = XOF */

    uint8_t t[2] = { (uint8_t)nonce, (uint8_t)(nonce >> 8) };
    iosha_absorb_128(ctx, seed, SEEDBYTES);          /* absorb seed */
    iosha_absorb_128(ctx, t, 2);                     /* absorb nonce */
    /* NOTE: no finalise needed – iosha_absorb contains the permute logic */
}

void stream128_squeezeblocks(uint8_t       *out,
                             size_t         nblocks,
                             stream128_state *st)
{
    iosha_squeeze_128(as_iosha(st), out,
                  nblocks * STREAM128_BLOCKBYTES);   /* 32-byte blocks */
}

/* ---------------- 256-bit security stream (was SHAKE-256) ------------- */
void dilithium_shake256_stream_init(keccak_state *st,
                                    const uint8_t seed[CRHBYTES],
                                    uint16_t      nonce)
{
    iosha_ctx *ctx = as_iosha(st);
    iosha_init(ctx, 0x01);                       /* same XOF tag */

    uint8_t t[2] = { (uint8_t)nonce, (uint8_t)(nonce >> 8) };
    iosha_absorb(ctx, seed, CRHBYTES);
    iosha_absorb(ctx, t, 2);
}

void stream256_squeezeblocks(uint8_t       *out,
                             size_t         nblocks,
                             stream256_state *st)
{
    iosha_squeeze(as_iosha(st), out,
                  nblocks * STREAM256_BLOCKBYTES);
}
