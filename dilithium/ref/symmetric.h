#ifndef SYMMETRIC_H
#define SYMMETRIC_H

#include <stdint.h>
#include <stddef.h>
#include "params.h"
#include "fips202.h"           /* for original keccak_state struct   */
#include "iosha.h"

/* ---------------- stream-state aliases --------------------------- */
typedef keccak_state stream128_state;   /* unchanged struct          */
typedef keccak_state stream256_state;

/* 32-byte rate equals 256-bit capacity                              */
#define STREAM128_BLOCKBYTES 32
#define STREAM256_BLOCKBYTES 32

/* ---------- API (names stay identical to SHAKE version) ---------- */
void dilithium_shake128_stream_init(keccak_state       *st,
                                    const uint8_t      seed[SEEDBYTES],
                                    uint16_t           nonce);

void dilithium_shake256_stream_init(keccak_state       *st,
                                    const uint8_t      seed[CRHBYTES],
                                    uint16_t           nonce);

void stream128_squeezeblocks(uint8_t *out, size_t nblocks,
                             stream128_state *st);

void stream256_squeezeblocks(uint8_t *out, size_t nblocks,
                             stream256_state *st);

/* ---------- macro wrappers used by other modules ---------------- */
#define stream128_init(STATE,SEED,NONCE) \
        dilithium_shake128_stream_init(STATE,SEED,NONCE)
#define stream256_init(STATE,SEED,NONCE) \
        dilithium_shake256_stream_init(STATE,SEED,NONCE)

#endif
