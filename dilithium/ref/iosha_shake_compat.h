/*  iosha_shake_compat.h
 *  --------------------
 *  Re-implements the five SHAKE-256 symbols that Dilithium uses,
 *  but runs them on IOSHA-v2.  No typedef clash with keccak_state.
 */
#ifndef IOSHA_SHAKE_COMPAT_H
#define IOSHA_SHAKE_COMPAT_H

#include "iosha.h"          /* iosha_ctx, iosha_* helpers           */

/* bring in keccak_state struct first, then we’ll just cast */
#include "fips202.h"        /* struct keccak_state { … }            */

/* forward declaration so inlines know the symbol                   */
void iosha_permute(uint64_t state[8]);

/* tag 0x02 distinguishes CRH/H domain                              */
#define IOSHA_TAG_CRH 0x02

/* helper: cast keccak_state* → iosha_ctx*                          */
static inline iosha_ctx *as_iosha(keccak_state *k)
{ return (iosha_ctx *)k; }

/* ───── streaming API wrappers ─────────────────────────────────── */
static inline void shake256_init(keccak_state *st)
{ iosha_init(as_iosha(st), IOSHA_TAG_CRH); }

static inline void shake256_absorb(keccak_state *st,
                                   const uint8_t *in, size_t len)
{ iosha_absorb(as_iosha(st), in, len); }

static inline void shake256_finalize(keccak_state *st)
{
    uint8_t pad = 0x80;
    iosha_absorb(as_iosha(st), &pad, 1);
    if (as_iosha(st)->idx) {
        iosha_permute(as_iosha(st)->st);
        as_iosha(st)->idx = 0;
    }
}

static inline void shake256_squeeze(uint8_t *out, size_t len,
                                    keccak_state *st)
{ iosha_squeeze(as_iosha(st), out, len); }

/* ───── one-shot helper ────────────────────────────────────────── */
static inline void shake256(uint8_t *out, size_t outlen,
                            const uint8_t *in, size_t inlen)
{
    iosha_ctx ctx;
    iosha_init(&ctx, IOSHA_TAG_CRH);
    iosha_absorb(&ctx, in, inlen);
    iosha_squeeze(&ctx, out, outlen);
}

#endif /* IOSHA_SHAKE_COMPAT_H */
