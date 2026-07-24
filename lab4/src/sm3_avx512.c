/* sm3_avx512.c — AVX-512 multi-buffer SM3 (16-way parallel)
 *
 * Processes 16 independent equal-length messages simultaneously using
 * 512-bit ZMM registers.  Leverages AVX-512F native rotate (VPROLD)
 * for efficient 32-bit lane rotation without shift+or emulation.
 *
 * Build requirement: -mavx512f -mavx512vl
 */

#include "sm3.h"

#if defined(__AVX512F__) && defined(__AVX512VL__)

#include <string.h>
#include <immintrin.h>

/* ================================================================
 *  SIMD helpers — AVX-512 has native VPROLD for 32-bit rotate
 * ================================================================ */

#define mm512_rotl32(x, n)  _mm512_rol_epi32(x, n)

#define P0_512(x)  _mm512_xor_si512(_mm512_xor_si512(x, mm512_rotl32(x, 9)),  mm512_rotl32(x, 17))
#define P1_512(x)  _mm512_xor_si512(_mm512_xor_si512(x, mm512_rotl32(x, 15)), mm512_rotl32(x, 23))

#define FF0_512(a,b,c)  _mm512_xor_si512(_mm512_xor_si512(a, b), c)
#define FF1_512(a,b,c)  _mm512_or_si512(_mm512_or_si512(_mm512_and_si512(a,b), _mm512_and_si512(a,c)), _mm512_and_si512(b,c))
#define GG0_512(e,f,g)  _mm512_xor_si512(_mm512_xor_si512(e, f), g)
#define GG1_512(e,f,g)  _mm512_or_si512(_mm512_and_si512(e, f), _mm512_andnot_si512(e, g))

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
 *  Load 16 messages into SoA layout
 * ================================================================ */

/* Load word j (0..15) from each of 16 messages into one ZMM register.
 * buf[k] points to message k (64 bytes).
 */
static void load_transpose_x16(const uint8_t *buf[SM3_MB_X16],
                               __m512i W[16])
{
    for (int j = 0; j < 16; j++) {
        uint32_t vals[SM3_MB_X16];
        for (int k = 0; k < SM3_MB_X16; k++) {
            const uint8_t *b = buf[k] + j * 4;
            vals[k] = ((uint32_t)b[0] << 24) |
                      ((uint32_t)b[1] << 16) |
                      ((uint32_t)b[2] <<  8) |
                      ((uint32_t)b[3]);
        }
        W[j] = _mm512_loadu_si512(vals);
    }
}

/* ================================================================
 *  Multi-buffer compression — 16 messages, single block each
 * ================================================================ */

static void sm3_mb_x16_compress(__m512i state[8], __m512i W_in[16])
{
    __m512i W[68];
    __m512i W1[64];
    __m512i A, B, C, D, E, F, G, H;
    __m512i SS1, SS2, TT1, TT2;
    __m512i Tj_vec, ff, gg;
    int j;

    for (j = 0; j < 16; j++)
        W[j] = W_in[j];

    /* Message expansion: W[16..67] */
    for (j = 16; j < 68; j++) {
        __m512i t = _mm512_xor_si512(
            _mm512_xor_si512(W[j - 16], W[j - 9]),
            mm512_rotl32(W[j - 3], 15));
        W[j] = _mm512_xor_si512(
            _mm512_xor_si512(P1_512(t), mm512_rotl32(W[j - 13], 7)),
            W[j - 6]);
    }

    /* W'[j] */
    for (j = 0; j < 64; j++)
        W1[j] = _mm512_xor_si512(W[j], W[j + 4]);

    A = state[0]; B = state[1]; C = state[2]; D = state[3];
    E = state[4]; F = state[5]; G = state[6]; H = state[7];

    /* 64 rounds */
    for (j = 0; j < 64; j++) {
        Tj_vec = _mm512_set1_epi32(SM3_TJ_ROTL[j]);

        ff = (j < 16) ? FF0_512(A, B, C) : FF1_512(A, B, C);
        gg = (j < 16) ? GG0_512(E, F, G) : GG1_512(E, F, G);

        SS1 = mm512_rotl32(
            _mm512_add_epi32(
                _mm512_add_epi32(mm512_rotl32(A, 12), E),
                Tj_vec),
            7);

        SS2 = _mm512_xor_si512(SS1, mm512_rotl32(A, 12));

        TT1 = _mm512_add_epi32(
            _mm512_add_epi32(
                _mm512_add_epi32(ff, D),
                SS2),
            W1[j]);

        TT2 = _mm512_add_epi32(
            _mm512_add_epi32(
                _mm512_add_epi32(gg, H),
                SS1),
            W[j]);

        D = C;
        C = mm512_rotl32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = mm512_rotl32(F, 19);
        F = E;
        E = P0_512(TT2);
    }

    state[0] = _mm512_xor_si512(state[0], A);
    state[1] = _mm512_xor_si512(state[1], B);
    state[2] = _mm512_xor_si512(state[2], C);
    state[3] = _mm512_xor_si512(state[3], D);
    state[4] = _mm512_xor_si512(state[4], E);
    state[5] = _mm512_xor_si512(state[5], F);
    state[6] = _mm512_xor_si512(state[6], G);
    state[7] = _mm512_xor_si512(state[7], H);
}

/* ================================================================
 *  Multi-buffer API — 16-way
 * ================================================================ */

static const uint32_t SM3_IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

/* Forward declaration of scalar compress for final block padding */
extern void sm3_ref_compress(uint32_t state[8], const uint8_t block[64]);

void sm3_mb_x16_init(sm3_mb_x16_ctx_t *ctx)
{
    int i, k;
    for (i = 0; i < 8; i++)
        for (k = 0; k < SM3_MB_X16; k++)
            ctx->state[i * SM3_MB_X16 + k] = SM3_IV[i];
    for (k = 0; k < SM3_MB_X16; k++) {
        ctx->count[k]   = 0;
        ctx->buf_len[k] = 0;
    }
}

void sm3_mb_x16_update(sm3_mb_x16_ctx_t *ctx,
                       const uint8_t *msg[SM3_MB_X16], size_t len)
{
    int k;
    uint64_t bits = (uint64_t)len * 8;

    for (k = 0; k < SM3_MB_X16; k++)
        ctx->count[k] += bits;

    while (len >= SM3_BLOCK_SIZE) {
        __m512i state[8], W[16];

        for (int i = 0; i < 8; i++) {
            uint32_t vals[SM3_MB_X16];
            for (k = 0; k < SM3_MB_X16; k++)
                vals[k] = ctx->state[i * SM3_MB_X16 + k];
            state[i] = _mm512_loadu_si512(vals);
        }

        load_transpose_x16(msg, W);
        sm3_mb_x16_compress(state, W);

        for (int i = 0; i < 8; i++) {
            uint32_t vals[SM3_MB_X16];
            _mm512_storeu_si512(vals, state[i]);
            for (k = 0; k < SM3_MB_X16; k++)
                ctx->state[i * SM3_MB_X16 + k] = vals[k];
        }

        for (k = 0; k < SM3_MB_X16; k++)
            msg[k] += SM3_BLOCK_SIZE;
        len -= SM3_BLOCK_SIZE;
    }

    if (len > 0) {
        for (k = 0; k < SM3_MB_X16; k++) {
            memcpy(ctx->buf[k], msg[k], len);
            ctx->buf_len[k] = (uint32_t)len;
        }
    }
}

void sm3_mb_x16_final(sm3_mb_x16_ctx_t *ctx,
                      uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE])
{
    int i, k;

    for (k = 0; k < SM3_MB_X16; k++) {
        uint8_t block[SM3_BLOCK_SIZE];
        uint64_t bits = ctx->count[k];
        uint32_t blen = ctx->buf_len[k];

        memcpy(block, ctx->buf[k], blen);
        block[blen++] = 0x80;

        if (blen > 56) {
            memset(block + blen, 0, SM3_BLOCK_SIZE - blen);
            uint32_t st[8];
            for (i = 0; i < 8; i++)
                st[i] = ctx->state[i * SM3_MB_X16 + k];
            sm3_ref_compress(st, block);
            for (i = 0; i < 8; i++)
                ctx->state[i * SM3_MB_X16 + k] = st[i];
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
            st[i] = ctx->state[i * SM3_MB_X16 + k];
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

void sm3_mb_x16_hash(const uint8_t *msg[SM3_MB_X16], size_t len,
                     uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE])
{
    sm3_mb_x16_ctx_t ctx;
    sm3_mb_x16_init(&ctx);
    sm3_mb_x16_update(&ctx, msg, len);
    sm3_mb_x16_final(&ctx, digest);
}

#endif /* __AVX512F__ && __AVX512VL__ */
