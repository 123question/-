/* sm3_neon.c — ARM NEON multi-buffer SM3 (4-way parallel)
 *
 * Processes 4 independent equal-length messages simultaneously using
 * 128-bit NEON registers (uint32x4_t).  Each 32-bit lane holds the
 * corresponding variable for one message (SoA layout).
 *
 * ARM NEON provides 32 × 128-bit registers (A64), sufficient to hold
 * the entire W[0..67] + W'[0..63] array without spilling.
 *
 * Build requirement: -march=armv8-a+crypto  (or arm64 target)
 */

#include "sm3.h"

#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))

#include <string.h>
#include <arm_neon.h>

/* ================================================================
 *  NEON helpers — 32-bit rotate-left
 * ================================================================ */

static inline uint32x4_t neon_rotl32(uint32x4_t x, int n) {
    return vorrq_u32(vshlq_n_u32(x, n), vshrq_n_u32(x, 32 - n));
}

#define P0_NEON(x)  veorq_u32(veorq_u32(x, neon_rotl32(x, 9)),  neon_rotl32(x, 17))
#define P1_NEON(x)  veorq_u32(veorq_u32(x, neon_rotl32(x, 15)), neon_rotl32(x, 23))

#define FF0_NEON(a,b,c)  veorq_u32(veorq_u32(a, b), c)
#define FF1_NEON(a,b,c)  vorrq_u32(vorrq_u32(vandq_u32(a,b), vandq_u32(a,c)), vandq_u32(b,c))
#define GG0_NEON(e,f,g)  veorq_u32(veorq_u32(e, f), g)
#define GG1_NEON(e,f,g)  vorrq_u32(vandq_u32(e, f), vbicq_u32(g, e))
/* Note: (~e & g) = g & ~e = vbicq_u32(g, e) = bit-clear: g AND NOT e */

/* ================================================================
 *  Constants
 * ================================================================ */

static const uint32_t SM3_TJ_ROTL[64] = {
    0x79CC4519, 0xF3988A32, 0xE7311465, 0xCE6228CB,
    0x9CC45197, 0x3988A32F, 0x7311465E, 0xE6228CBC,
    0xCC451979, 0x988A32F3, 0x311465E7, 0x6228CBCE,
    0xC451979C, 0x88A32F39, 0x11465E73, 0x228CBCE6,
    0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C,
    0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE,
    0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC,
    0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5,
    0x7A879D8A, 0xF50F3B14, 0xEA1E7629, 0xD43CEC53,
    0xA879D8A7, 0x50F3B14F, 0xA1E7629E, 0x43CEC53D,
    0x879D8A7A, 0x0F3B14F5, 0x1E7629EA, 0x3CEC53D4,
    0x79D8A7A8, 0xF3B14F50, 0xE7629EA1, 0xCEC53D43,
    0x9D8A7A87, 0x3B14F50F, 0x7629EA1E, 0xEC53D43C,
    0xD8A7A879, 0xB14F50F3, 0x629EA1E7, 0xC53D43CE,
    0x8A7A879D, 0x14F50F3B, 0x29EA1E76, 0x53D43CEC,
    0xA7A879D8, 0x4F50F3B1, 0x9EA1E762, 0x3D43CEC5
};

/* ================================================================
 *  Load 4 messages into SoA layout
 * ================================================================ */

static void load_transpose_x4(const uint8_t *buf[SM3_MB_X4],
                              uint32x4_t W[16])
{
    for (int j = 0; j < 16; j++) {
        uint32_t vals[SM3_MB_X4];
        for (int k = 0; k < SM3_MB_X4; k++) {
            const uint8_t *b = buf[k] + j * 4;
            vals[k] = ((uint32_t)b[0] << 24) |
                      ((uint32_t)b[1] << 16) |
                      ((uint32_t)b[2] <<  8) |
                      ((uint32_t)b[3]);
        }
        W[j] = vld1q_u32(vals);
    }
}

/* ================================================================
 *  Multi-buffer compression — 4 messages, single block each
 * ================================================================ */

static void sm3_mb_x4_compress(uint32x4_t state[8], uint32x4_t W_in[16])
{
    uint32x4_t W[68];
    uint32x4_t W1[64];
    uint32x4_t A, B, C, D, E, F, G, H;
    uint32x4_t SS1, SS2, TT1, TT2;
    uint32x4_t Tj_vec, ff, gg;
    int j;

    for (j = 0; j < 16; j++)
        W[j] = W_in[j];

    /* Message expansion: W[16..67] */
    for (j = 16; j < 68; j++) {
        uint32x4_t t = veorq_u32(
            veorq_u32(W[j - 16], W[j - 9]),
            neon_rotl32(W[j - 3], 15));
        W[j] = veorq_u32(
            veorq_u32(P1_NEON(t), neon_rotl32(W[j - 13], 7)),
            W[j - 6]);
    }

    /* W'[j] */
    for (j = 0; j < 64; j++)
        W1[j] = veorq_u32(W[j], W[j + 4]);

    A = state[0]; B = state[1]; C = state[2]; D = state[3];
    E = state[4]; F = state[5]; G = state[6]; H = state[7];

    /* 64 rounds */
    for (j = 0; j < 64; j++) {
        Tj_vec = vld1q_dup_u32(&SM3_TJ_ROTL[j]);

        ff = (j < 16) ? FF0_NEON(A, B, C) : FF1_NEON(A, B, C);
        gg = (j < 16) ? GG0_NEON(E, F, G) : GG1_NEON(E, F, G);

        SS1 = neon_rotl32(
            vaddq_u32(
                vaddq_u32(neon_rotl32(A, 12), E),
                Tj_vec),
            7);

        SS2 = veorq_u32(SS1, neon_rotl32(A, 12));

        TT1 = vaddq_u32(
            vaddq_u32(
                vaddq_u32(ff, D),
                SS2),
            W1[j]);

        TT2 = vaddq_u32(
            vaddq_u32(
                vaddq_u32(gg, H),
                SS1),
            W[j]);

        D = C;
        C = neon_rotl32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = neon_rotl32(F, 19);
        F = E;
        E = P0_NEON(TT2);
    }

    state[0] = veorq_u32(state[0], A);
    state[1] = veorq_u32(state[1], B);
    state[2] = veorq_u32(state[2], C);
    state[3] = veorq_u32(state[3], D);
    state[4] = veorq_u32(state[4], E);
    state[5] = veorq_u32(state[5], F);
    state[6] = veorq_u32(state[6], G);
    state[7] = veorq_u32(state[7], H);
}

/* ================================================================
 *  Multi-buffer API — 4-way
 * ================================================================ */

static const uint32_t SM3_IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

extern void sm3_ref_compress(uint32_t state[8], const uint8_t block[64]);

void sm3_mb_x4_init(sm3_mb_x4_ctx_t *ctx)
{
    int i, k;
    for (i = 0; i < 8; i++)
        for (k = 0; k < SM3_MB_X4; k++)
            ctx->state[i * SM3_MB_X4 + k] = SM3_IV[i];
    for (k = 0; k < SM3_MB_X4; k++) {
        ctx->count[k]   = 0;
        ctx->buf_len[k] = 0;
    }
}

void sm3_mb_x4_update(sm3_mb_x4_ctx_t *ctx,
                      const uint8_t *msg[SM3_MB_X4], size_t len)
{
    int k;
    uint64_t bits = (uint64_t)len * 8;

    for (k = 0; k < SM3_MB_X4; k++)
        ctx->count[k] += bits;

    while (len >= SM3_BLOCK_SIZE) {
        uint32x4_t state[8], W[16];

        for (int i = 0; i < 8; i++) {
            uint32_t vals[SM3_MB_X4];
            for (k = 0; k < SM3_MB_X4; k++)
                vals[k] = ctx->state[i * SM3_MB_X4 + k];
            state[i] = vld1q_u32(vals);
        }

        load_transpose_x4(msg, W);
        sm3_mb_x4_compress(state, W);

        for (int i = 0; i < 8; i++) {
            uint32_t vals[SM3_MB_X4];
            vst1q_u32(vals, state[i]);
            for (k = 0; k < SM3_MB_X4; k++)
                ctx->state[i * SM3_MB_X4 + k] = vals[k];
        }

        for (k = 0; k < SM3_MB_X4; k++)
            msg[k] += SM3_BLOCK_SIZE;
        len -= SM3_BLOCK_SIZE;
    }

    if (len > 0) {
        for (k = 0; k < SM3_MB_X4; k++) {
            memcpy(ctx->buf[k], msg[k], len);
            ctx->buf_len[k] = (uint32_t)len;
        }
    }
}

void sm3_mb_x4_final(sm3_mb_x4_ctx_t *ctx,
                     uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE])
{
    int i, k;

    for (k = 0; k < SM3_MB_X4; k++) {
        uint8_t block[SM3_BLOCK_SIZE];
        uint64_t bits = ctx->count[k];
        uint32_t blen = ctx->buf_len[k];

        memcpy(block, ctx->buf[k], blen);
        block[blen++] = 0x80;

        if (blen > 56) {
            memset(block + blen, 0, SM3_BLOCK_SIZE - blen);
            uint32_t st[8];
            for (i = 0; i < 8; i++)
                st[i] = ctx->state[i * SM3_MB_X4 + k];
            sm3_ref_compress(st, block);
            for (i = 0; i < 8; i++)
                ctx->state[i * SM3_MB_X4 + k] = st[i];
            blen = 0;
        }

        memset(block + blen, 0, 56 - blen);
        block[56] = (uint8_t)(bits >> 56);
        block[57] = (uint8_t)(bits >> 48);
        block[58] = (uint8_t)(bits >> 40);
        block[59] = (uint8_t)(bits >> 32);
        block[60] = (uint8_t)(bits >> 24);
        block[61] = (uint8_t)(bits >> 16);
        block[62] = (uint8_t)(bits >>  8);
        block[63] = (uint8_t)(bits);

        uint32_t st[8];
        for (i = 0; i < 8; i++)
            st[i] = ctx->state[i * SM3_MB_X4 + k];
        sm3_ref_compress(st, block);

        for (i = 0; i < 8; i++) {
            uint32_t v = st[i];
            digest[k][i * 4 + 0] = (uint8_t)(v >> 24);
            digest[k][i * 4 + 1] = (uint8_t)(v >> 16);
            digest[k][i * 4 + 2] = (uint8_t)(v >>  8);
            digest[k][i * 4 + 3] = (uint8_t)(v);
        }
    }
}

void sm3_mb_x4_hash(const uint8_t *msg[SM3_MB_X4], size_t len,
                    uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE])
{
    sm3_mb_x4_ctx_t ctx;
    sm3_mb_x4_init(&ctx);
    sm3_mb_x4_update(&ctx, msg, len);
    sm3_mb_x4_final(&ctx, digest);
}

#endif /* __aarch64__ || (__ARM_NEON && __ARM_ARCH_ISA_A64) */
