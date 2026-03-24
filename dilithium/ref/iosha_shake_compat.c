/*  iosha_shake_compat.c
 *  -------------------------------------------------------------
 *  Implements Dilithium's SHAKE-256 interface with IOSHA-v2.
 *  Drop this file into the same directory as the other sources
 *  and add it to your Makefile.
 */

#include "iosha.h"
#include "fips202.h"      /* brings in keccak_state typedef         */

/* helper: cast       */
static inline iosha_ctx *as_iosha(keccak_state *k)
{ return (iosha_ctx *)k;  }

#define TAG_CRH 0x02      /* matches Dilithium’s CRH domain tag     */

/* ---- streaming API ------------------------------------------- */
void shake256_init(keccak_state *st)
{
    iosha_init(as_iosha(st), TAG_CRH);
}

void shake256_absorb(keccak_state *st,
                     const uint8_t *in, size_t inlen)
{
    iosha_absorb(as_iosha(st), in, inlen);
}

void shake256_finalize(keccak_state *st)
{
    uint8_t pad = 0x80;
    iosha_absorb(as_iosha(st), &pad, 1);
    if(as_iosha(st)->idx) {
        iosha_permute(as_iosha(st)->st);
        as_iosha(st)->idx = 0;
    }
}

void shake256_squeeze(uint8_t *out, size_t outlen,
                      keccak_state *st)
{
    iosha_squeeze(as_iosha(st), out, outlen);
}

/* ---- one-shot helper ----------------------------------------- */
void shake256(uint8_t *out, size_t outlen,
              const uint8_t *in, size_t inlen)
{
    iosha_ctx ctx;
    iosha_init(&ctx, TAG_CRH);
    iosha_absorb(&ctx, in, inlen);
    iosha_squeeze(&ctx, out, outlen);
}
