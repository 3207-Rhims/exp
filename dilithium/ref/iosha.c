#include "iosha.h"
#include <string.h>

#define ROTL64(x,r) ( ((uint64_t)(x) << ((r)&63)) | ((uint64_t)(x) >> (64-((r)&63))) )
#define ROTR64(x,r) ( ((uint64_t)(x) >> ((r)&63)) | ((uint64_t)(x) << (64-((r)&63))) )

#ifndef IOSHA_ROUNDS
#define IOSHA_ROUNDS 8
#endif

#define RATE_BYTES 64  /* 8 lanes × 8 bytes */
#define CAPA_BYTES 64  /* 8 lanes × 8 bytes */

static inline uint64_t splitmix64_next(uint64_t *x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ULL);
    z ^= z >> 30; z *= 0xBF58476D1CE4E5B9ULL;
    z ^= z >> 27; z *= 0x94D049BB133111EBULL
    z ^= z >> 31;
    return z;
}

/* Fixed, public round constants (generated once via SplitMix64). */
static uint64_t RC[IOSHA_ROUNDS][8];
static int RC_READY = 0;
static void iosha_rc_init(void) {
    if (RC_READY) return;
    uint64_t seed = 0x494F5348415F5652ULL ^ 0xC001D00D5EEDULL; /* "IOSHA_VR" ^ salt */
    for (int r = 0; r < IOSHA_ROUNDS; ++r)
        for (int i = 0; i < 8; ++i)
            RC[r][i] = splitmix64_next(&seed);
    RC_READY = 1;
}

// /* Lane rotation schedules (odd, distinct; chosen for diffusion) */
// static const unsigned ROT_A[8] = {23, 59,  7,  3, 13, 43, 29, 19};
// static const unsigned ROT_B[8] = {41,  5, 17, 31, 27, 11, 37, 47};
// static const unsigned ROT_G[8] = { 1,  3,  6, 10, 15, 21, 28, 36};

static const unsigned ROT_A[8] = {63,19,39,15,23,31,11,5};
static const unsigned ROT_B[8] = {21,3,31,33,45,5,17,7};
static const unsigned ROT_G[8] = {25,45,51,5,63,47,7,1};

/* 8-lane ARX box; Feistel makes overall mapping bijective. */
static inline void arxbox8(uint64_t x[8], const uint64_t rc[8]) {
    /* Mix 1: add-rot across neighbors */
    for (int i = 0; i < 8; ++i)
        x[i] += ROTL64(x[(i + 1) & 7], ROT_A[i]);

    /* Mix 2: xor-rotr across next-neighbors */
    for (int i = 0; i < 8; ++i)
        x[i] ^= ROTR64(x[(i + 2) & 7], ROT_B[i]);

    /* Inject public round constants */
    for (int i = 0; i < 8; ++i)
        x[i] += rc[i];

    /* Global parity diffusion */
    uint64_t t = x[0]^x[1]^x[2]^x[3]^x[4]^x[5]^x[6]^x[7];
    for (int i = 0; i < 8; ++i)
        x[i] ^= ROTL64(t, ROT_G[i]);
}

/* Full-state permutation: Feistel over (rate=L, capacity=R) halves. */
static void iosha_permute(uint64_t s[16]) {
    iosha_rc_init();

    uint64_t L[8], R[8];
    for (int i = 0; i < 8; ++i) { L[i] = s[i]; R[i] = s[8 + i]; }

    for (int r = 0; r < IOSHA_ROUNDS; ++r) {
        uint64_t T[8];
        for (int i = 0; i < 8; ++i) T[i] = R[i];     /* T = R */
        arxbox8(T, RC[r]);                           /* T = F(R, rc[r]) */
        for (int i = 0; i < 8; ++i) L[i] ^= T[i];    /* L = L ^ T       */
        /* swap(L, R) */
        for (int i = 0; i < 8; ++i) { uint64_t tmp = L[i]; L[i] = R[i]; R[i] = tmp; }
    }

    /* After N swaps, halves are swapped; restore so rate stays s[0..7]. */
    if (IOSHA_ROUNDS & 1) {
        for (int i = 0; i < 8; ++i) { uint64_t tmp = L[i]; L[i] = R[i]; R[i] = tmp; }
    }

    for (int i = 0; i < 8; ++i) { s[i] = L[i]; s[8 + i] = R[i]; }
}

/* ===================== Sponge core (pad10*1) ===================== */

static inline void iosha_pad10star1_and_permute(iosha_ctx *ctx) {
    uint8_t *b = (uint8_t*)ctx->st;
    b[ctx->idx]      ^= 0x01;          /* domain delimiter for sponge */
    b[ctx->rate - 1] ^= 0x80;          /* final '1' at end of *current* rate */
    iosha_permute(ctx->st);
    ctx->idx = 0;
}

/* ===================== Public API impl ===================== */

static inline void zero_state(uint64_t st[16]) { memset(st, 0, 16*sizeof(uint64_t)); }

static void iosha_init_common(iosha_ctx *ctx, uint8_t tag, uint32_t rate_bytes) {
    ctx->idx  = 0;
    ctx->rate = rate_bytes;
    zero_state(ctx->st);

    /* Write IV as bytes so outputs are identical on LE/BE machines */
    uint8_t *b = (uint8_t*)ctx->st;
    memcpy(b +  0, "IOSHA\0\0\0", 8);  /* 5 chars + padding */
    memcpy(b +  8, "signed\0\0", 8);   /* 6 chars + padding */
    memcpy(b + 16, "dilith\0\0", 8);   /* 6 chars + padding */
    b[24] = tag;                       /* domain tag byte */
}

/* 256-bit-strength stream (SHAKE256-like): rate=64, capacity=64 */
void iosha_init(iosha_ctx *ctx, uint8_t tag) {
    iosha_init_common(ctx, tag, 64);
}

/* 128-bit-strength stream (SHAKE128-like): rate=96, capacity=32 */
void iosha_init_128(iosha_ctx *ctx, uint8_t tag) {
    iosha_init_common(ctx, tag, 96);
}

/* XOR-absorb into the first 64 bytes (the rate). Permute on full blocks. */
void iosha_absorb(iosha_ctx *ctx, const uint8_t *in, size_t inlen)
{
    uint8_t *b = (uint8_t*)ctx->st;
    while (inlen) {
        size_t room = ctx->rate - ctx->idx;
        if (room > inlen) room = inlen;

        for (size_t i = 0; i < room; ++i)
            b[ctx->idx + i] ^= in[i];

        ctx->idx += room;
        in      += room;
        inlen   -= room;

        if (ctx->idx == ctx->rate) {
            iosha_permute(ctx->st);
            ctx->idx = 0;
        }
    }
}

void iosha_absorb_128(iosha_ctx *ctx, const uint8_t *in, size_t inlen)
{
    /* Same rate and permutation; 128-variant is just an alias. */
    iosha_absorb(ctx, in, inlen);
}

void iosha_squeeze(iosha_ctx *ctx, uint8_t *out, size_t outlen)
{
    iosha_pad10star1_and_permute(ctx);

    uint8_t *b = (uint8_t*)ctx->st;
    while (outlen) {
        size_t n = (outlen < ctx->rate) ? outlen : ctx->rate;
        memcpy(out, b, n);      /* read from the first 'rate' bytes */
        out    += n;
        outlen -= n;
        if (outlen) iosha_permute(ctx->st);
    }
}

void iosha_squeeze_128(iosha_ctx *ctx, uint8_t *out, size_t outlen)
{
    /* Using same state/perm; identical to iosha_squeeze for this demo. */
    iosha_squeeze(ctx, out, outlen);
}

/* One-shot XOF: tag=0x01, absorb nonce (LE16) || seed, squeeze outlen bytes. */
void iosha_xof_bytes(const uint8_t *seed, size_t seedlen,
                     uint16_t nonce, uint8_t *out, size_t outlen)
{
    iosha_ctx ctx;
    iosha_init_128(&ctx, 0x01);            /* 128-bit XOF */
    uint8_t t[2] = { (uint8_t)(nonce & 0xFFu), (uint8_t)(nonce >> 8) };
    iosha_absorb(&ctx, t, 2);
    iosha_absorb(&ctx, seed, seedlen);
    iosha_squeeze(&ctx, out, outlen);
}

void iosha_crh_bytes(const uint8_t *in, size_t inlen,
                     uint8_t *out, size_t outlen)
{
    iosha_ctx ctx;
    iosha_init(&ctx, 0x02);                /* 256-bit CRH stream */
    iosha_absorb(&ctx, in, inlen);
    iosha_squeeze(&ctx, out, outlen);      /* ask for 64 bytes for 256-bit collisions */
}









// /*  iosha.c  –  IOSHA-v2 implementation (512-bit duplex sponge)
//  *  ----------------------------------------------------------- */
// #include "iosha.h"
// #include <string.h>   /* for memcpy */
// #include "farmhash/src/farmhash_wrapper.h"


// #define ROTL64(x,r) (((x) << ((r)&63)) | ((x) >> (64-((r)&63))))
// #define ROTR64(x,r) (((x) >> ((r)&63)) | ((x) << (64-((r)&63))))


// static inline uint64_t rotl64(uint64_t x, unsigned r)
// { return (x << (r & 63)) | (x >> ((64 - r) & 63)); }

// static void xs128pp_next(uint64_t st[2], uint64_t out[2])
// {
//     uint64_t s0 = st[0], s1 = st[1];
//     out[0] = rotl64(s0 + s1, 17) + s0;            /* k1                  */
//     s1    ^= s0;
//     st[0]  = rotl64(s0, 49) ^ s1 ^ (s1 << 21);
//     st[1]  = rotl64(s1, 28);
//     out[1] = st[0] + st[1];                       /* k2                  */
// }


// void iosha_permute(uint64_t s[16])
// {
//     const uint64_t beta = 64;

//     C_Hash128 seed = farmhash128_c((const uint8_t *)s, 64);
//     uint64_t  prng[2] = { seed.h1, seed.h2 };

//     for (int rnd = 0; rnd < 8; ++rnd) {
//         uint64_t k[2];                    /* k[0]=k1 , k[1]=k2          */
//         xs128pp_next(prng, k);            /* ~35 cycles / call          */

//         uint64_t z   = s[rnd & 7];        /* pivot lane                 */
//         uint64_t spr = s[(rnd + 3) & 7] ^ k[0];
//         uint64_t r   = (k[0] ^ k[1]) & 63;
//         z ^= ROTR64(k[1], (int)r);

//         /*      gives one DSP-class MUL on Cortex-M4/M7           */
//         uint64_t mix = (uint64_t)((uint32_t)z * 0x9E37u);

//         /* 3.c final branchless stir and add ‘spring’             */
//         z = (mix ^ rotl64(z, r)) + spr;

//         s[rnd & 7] = z;

//         /* feed-forward into the other seven lanes */
//         for (int i = 1; i < 8; ++i)
//             s[(rnd + i) & 7] = ROTL64(s[(rnd + i) & 7] ^ z, i);
//     }
// }


// void iosha_init(iosha_ctx *ctx, uint8_t tag)
// {
//     ctx->st[0] = 0x49534F48AULL;        /* “IOSHA”   */
//     ctx->st[1] = 0x7369676E656420ULL;   /* “signed”  */
//     ctx->st[2] = 0x64696C697468ULL;     /* “dilith”  */
//     ctx->st[3] = tag;                   /* domain tag */
//    /* Clear the remaining 16 lanes */
//     for (int i = 4; i < 16; i++) {
//         ctx->st[i] = 0ULL;
//     }
//     ctx->idx = 0;
// }

// void iosha_init_128(iosha_ctx *ctx, uint8_t tag)
// {
//     ctx->st[0] = 0x49534F48AULL;        /* “IOSHA”   */
//     ctx->st[1] = 0x7369676E656420ULL;   /* “signed”  */
//     ctx->st[2] = 0x64696C697468ULL;     /* “dilith”  */
//     ctx->st[3] = tag;                   /* domain tag */
//    /* Clear the remaining 16 lanes */
//     for (int i = 4; i < 12; i++) {
//         ctx->st[i] = 0ULL;
//     }
//     ctx->idx = 0;
// }

// void iosha_absorb(iosha_ctx *ctx, const uint8_t *in, size_t inlen)
// {
//     while (inlen) {
//         size_t room = 64 - ctx->idx;
//         if (room > inlen) room = inlen;

//         for (size_t i = 0; i < room; ++i) {
//             uint8_t *lane = (uint8_t *)&ctx->st[(ctx->idx + i) / 8];
//             lane[(ctx->idx + i) % 8] ^= in[i];
//         }

//         ctx->idx += room;
//         in      += room;
//         inlen   -= room;

//         if (ctx->idx == 64) {
//             iosha_permute(ctx->st);
//             ctx->idx = 0;
//         }
//     }
// }


// void iosha_absorb_128(iosha_ctx *ctx, const uint8_t *in, size_t inlen)
// {
//     while (inlen) {
//         size_t room = 64 - ctx->idx;
//         if (room > inlen) room = inlen;

//         for (size_t i = 0; i < room; ++i) {
//             uint8_t *lane = (uint8_t *)&ctx->st[(ctx->idx + i) / 8];
//             lane[(ctx->idx + i) % 8] ^= in[i];
//         }

//         ctx->idx += room;
//         in      += room;
//         inlen   -= room;

//         if (ctx->idx == 64) {
//             iosha_permute(ctx->st);
//             ctx->idx = 0;
//         }
//     }
// }

// void iosha_squeeze(iosha_ctx *ctx, uint8_t *out, size_t outlen)
// {
//     /* 10*1 padding */
//     uint8_t pad = 0x80;
//     iosha_absorb(ctx, &pad, 1);

//     if (ctx->idx) { iosha_permute(ctx->st); ctx->idx = 0; }

//     while (outlen) {
//         iosha_permute(ctx->st);
//         size_t n = outlen < 64 ? outlen : 64;     /* 64-byte rate */
//         memcpy(out, ctx->st, n);
//         out     += n;
//         outlen  -= n;
//     }
// }

// void iosha_squeeze_128(iosha_ctx *ctx, uint8_t *out, size_t outlen)
// {
//     /* 10*1 padding */
//     uint8_t pad = 0x80;
//     iosha_absorb(ctx, &pad, 1);

//     if (ctx->idx) { iosha_permute(ctx->st); ctx->idx = 0; }

//     while (outlen) {
//         iosha_permute(ctx->st);
//         size_t n = outlen < 64 ? outlen : 64;     /* 64-byte rate */
//         memcpy(out, ctx->st, n);
//         out     += n;
//         outlen  -= n;
//     }
// }

// /* ---------------------------------------------------------------
//  *  3.  One-shot wrappers (XOF / CRH)
//  * ------------------------------------------------------------- */
// void iosha_xof_bytes(const uint8_t *seed, size_t seedlen,
//                      uint16_t nonce,
//                      uint8_t *out,   size_t outlen)
// {
//     iosha_ctx ctx;
//     iosha_init(&ctx, 0x01);                  /* tag 0x01 = XOF */

//     uint8_t t[2] = { (uint8_t)(nonce & 0xFFu), (uint8_t)(nonce >> 8) };
//     iosha_absorb(&ctx, t, 2);
//     iosha_absorb(&ctx, seed, seedlen);
//     iosha_squeeze(&ctx, out, outlen);
// }

// void iosha_crh_bytes(const uint8_t *in, size_t inlen,
//                      uint8_t *out,  size_t outlen)
// {
//     iosha_ctx ctx;
//     iosha_init(&ctx, 0x02);                  /* tag 0x02 = CRH */
//     iosha_absorb(&ctx, in, inlen);
//     iosha_squeeze(&ctx, out, outlen);
// }
