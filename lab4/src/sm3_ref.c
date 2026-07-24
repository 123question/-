/* sm3_ref.c — Reference scalar SM3 implementation
 *
 * Implements GM/T 0004-2012 SM3 cryptographic hash algorithm.
 * Portable C, no SIMD, serves as baseline for correctness and performance.
 */

#include "sm3.h"
#include <string.h>

/* ------------------------------------------------------------------ */
/*  Constants                                                         */
/* ------------------------------------------------------------------ */

static const uint32_t SM3_IV[8] = {
    0x7380166F, 0x4914B2B9, 0x172442D7, 0xDA8A0600,
    0xA96F30BC, 0x163138AA, 0xE38DEE4D, 0xB0FB0E4E
};

/* T_j: 0..15 → 0x79CC4519,  16..63 → 0x7A879D8A */
static const uint32_t SM3_T[2] = { 0x79CC4519, 0x7A879D8A };

/* ------------------------------------------------------------------ */
/*  Helper functions                                                  */
/* ------------------------------------------------------------------ */

static inline uint32_t rotl32(uint32_t x, int n) {
    return (x << (n & 31)) | (x >> ((-n) & 31));
}

static inline uint32_t P0(uint32_t x) {
    return x ^ rotl32(x,  9) ^ rotl32(x, 17);
}

static inline uint32_t P1(uint32_t x) {
    return x ^ rotl32(x, 15) ^ rotl32(x, 23);
}

static inline uint32_t FF0(uint32_t x, uint32_t y, uint32_t z) {
    return x ^ y ^ z;
}

static inline uint32_t FF1(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) | (x & z) | (y & z);
}

static inline uint32_t GG0(uint32_t x, uint32_t y, uint32_t z) {
    return x ^ y ^ z;
}

static inline uint32_t GG1(uint32_t x, uint32_t y, uint32_t z) {
    return (x & y) | (~x & z);
}

/* load 4 bytes big-endian → uint32_t */
static inline uint32_t load_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) |
           ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] <<  8) |
           ((uint32_t)p[3]);
}

/* store uint32_t → 4 bytes big-endian */
static inline void store_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >>  8);
    p[3] = (uint8_t)(v);
}

/* ------------------------------------------------------------------ */
/*  Message expansion                                                 */
/* ------------------------------------------------------------------ */

static void sm3_expand(uint32_t W[68], uint32_t W1[64],
                       const uint8_t block[64])
{
    int j;

    /* W[0..15] = block words (big-endian) */
    for (j = 0; j < 16; j++)
        W[j] = load_be32(block + j * 4);

    /* W[16..67] */
    for (j = 16; j < 68; j++) {
        W[j] = P1(W[j - 16] ^ W[j - 9] ^ rotl32(W[j - 3], 15))
             ^ rotl32(W[j - 13], 7)
             ^ W[j - 6];
    }

    /* W'[0..63] = W[j] ^ W[j+4] */
    for (j = 0; j < 64; j++)
        W1[j] = W[j] ^ W[j + 4];
}

/* ------------------------------------------------------------------ */
/*  Compression function                                              */
/* ------------------------------------------------------------------ */

void sm3_ref_compress(uint32_t state[8], const uint8_t block[64])
{
    uint32_t W[68], W1[64];
    uint32_t A, B, C, D, E, F, G, H;
    uint32_t SS1, SS2, TT1, TT2;
    int j;

    sm3_expand(W, W1, block);

    A = state[0]; B = state[1]; C = state[2]; D = state[3];
    E = state[4]; F = state[5]; G = state[6]; H = state[7];

    for (j = 0; j < 64; j++) {
        uint32_t Tj = (j < 16) ? SM3_T[0] : SM3_T[1];
        uint32_t FF = (j < 16) ? FF0(A, B, C) : FF1(A, B, C);
        uint32_t GG = (j < 16) ? GG0(E, F, G) : GG1(E, F, G);

        SS1 = rotl32(rotl32(A, 12) + E + rotl32(Tj, j), 7);
        SS2 = SS1 ^ rotl32(A, 12);
        TT1 = FF + D + SS2 + W1[j];
        TT2 = GG + H + SS1 + W[j];

        D = C;
        C = rotl32(B, 9);
        B = A;
        A = TT1;
        H = G;
        G = rotl32(F, 19);
        F = E;
        E = P0(TT2);
    }

    state[0] ^= A; state[1] ^= B; state[2] ^= C; state[3] ^= D;
    state[4] ^= E; state[5] ^= F; state[6] ^= G; state[7] ^= H;
}

/* ------------------------------------------------------------------ */
/*  Public API                                                        */
/* ------------------------------------------------------------------ */

void sm3_init(sm3_ctx_t *ctx)
{
    memcpy(ctx->state, SM3_IV, sizeof(SM3_IV));
    ctx->count   = 0;
    ctx->buf_len = 0;
}

void sm3_update(sm3_ctx_t *ctx, const uint8_t *data, size_t len)
{
    ctx->count += (uint64_t)len * 8;

    /* process leftover + new data until < 1 block remains */
    if (ctx->buf_len > 0) {
        uint32_t fill = SM3_BLOCK_SIZE - ctx->buf_len;
        if (len < fill) {
            memcpy(ctx->buf + ctx->buf_len, data, len);
            ctx->buf_len += (uint32_t)len;
            return;
        }
        memcpy(ctx->buf + ctx->buf_len, data, fill);
        sm3_ref_compress(ctx->state, ctx->buf);
        data     += fill;
        len      -= fill;
        ctx->buf_len = 0;
    }

    /* process full blocks */
    while (len >= SM3_BLOCK_SIZE) {
        sm3_ref_compress(ctx->state, data);
        data += SM3_BLOCK_SIZE;
        len  -= SM3_BLOCK_SIZE;
    }

    /* save leftover */
    if (len > 0) {
        memcpy(ctx->buf, data, len);
        ctx->buf_len = (uint32_t)len;
    }
}

void sm3_final(sm3_ctx_t *ctx, uint8_t digest[SM3_DIGEST_SIZE])
{
    uint32_t i;
    uint64_t bits = ctx->count;

    /* pad: append 1, zeros, length */
    ctx->buf[ctx->buf_len++] = 0x80;

    if (ctx->buf_len > 56) {
        memset(ctx->buf + ctx->buf_len, 0, SM3_BLOCK_SIZE - ctx->buf_len);
        sm3_ref_compress(ctx->state, ctx->buf);
        ctx->buf_len = 0;
    }

    memset(ctx->buf + ctx->buf_len, 0, 56 - ctx->buf_len);
    /* append 64-bit big-endian length */
    ctx->buf[56] = (uint8_t)(bits >> 56);
    ctx->buf[57] = (uint8_t)(bits >> 48);
    ctx->buf[58] = (uint8_t)(bits >> 40);
    ctx->buf[59] = (uint8_t)(bits >> 32);
    ctx->buf[60] = (uint8_t)(bits >> 24);
    ctx->buf[61] = (uint8_t)(bits >> 16);
    ctx->buf[62] = (uint8_t)(bits >>  8);
    ctx->buf[63] = (uint8_t)(bits);
    sm3_ref_compress(ctx->state, ctx->buf);

    /* output */
    for (i = 0; i < 8; i++)
        store_be32(digest + i * 4, ctx->state[i]);
}

void sm3_hash(const uint8_t *data, size_t len, uint8_t digest[SM3_DIGEST_SIZE])
{
    sm3_ctx_t ctx;
    sm3_init(&ctx);
    sm3_update(&ctx, data, len);
    sm3_final(&ctx, digest);
}
