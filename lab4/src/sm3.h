#ifndef SM3_H
#define SM3_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SM3_BLOCK_SIZE   64
#define SM3_DIGEST_SIZE  32
#define SM3_HASH_SIZE    32

/* ================================================================
 *  Single-message context (reference / hybrid)
 * ================================================================ */
typedef struct {
    uint32_t state[8];
    uint64_t count;          /* total bits processed */
    uint8_t  buf[SM3_BLOCK_SIZE];
    uint32_t buf_len;        /* bytes in buffer */
} sm3_ctx_t;

void sm3_init(sm3_ctx_t *ctx);
void sm3_update(sm3_ctx_t *ctx, const uint8_t *data, size_t len);
void sm3_final(sm3_ctx_t *ctx, uint8_t digest[SM3_DIGEST_SIZE]);

/* one-shot */
void sm3_hash(const uint8_t *data, size_t len, uint8_t digest[SM3_DIGEST_SIZE]);

/* ----------------------------------------------------------------
 *  SIMD-accelerated single-message (hybrid: SIMD expansion + GP compression)
 * ---------------------------------------------------------------- */
void sm3_hash_hybrid(const uint8_t *data, size_t len, uint8_t digest[SM3_DIGEST_SIZE]);

/* ================================================================
 *  Multi-buffer context — process N independent equal-length messages
 * ================================================================ */

/* ---- AVX2: 8-way ------------------------------------------------- */
#if defined(__AVX2__)
#define SM3_MB_X8  8

typedef struct {
    uint32_t state[8 * SM3_MB_X8];   /* interleaved: state[0..7]=msg0, state[8..15]=msg1, ... */
    uint64_t count[SM3_MB_X8];
    uint8_t  buf[SM3_MB_X8][SM3_BLOCK_SIZE];
    uint32_t buf_len[SM3_MB_X8];
} sm3_mb_x8_ctx_t;

void sm3_mb_x8_init(sm3_mb_x8_ctx_t *ctx);
void sm3_mb_x8_update(sm3_mb_x8_ctx_t *ctx,
                      const uint8_t *msg[SM3_MB_X8], size_t len);
void sm3_mb_x8_final(sm3_mb_x8_ctx_t *ctx,
                     uint8_t digest[SM3_MB_X8][SM3_DIGEST_SIZE]);
void sm3_mb_x8_hash(const uint8_t *msg[SM3_MB_X8], size_t len,
                    uint8_t digest[SM3_MB_X8][SM3_DIGEST_SIZE]);
#endif /* __AVX2__ */

/* ---- AVX-512: 16-way --------------------------------------------- */
#if defined(__AVX512F__) && defined(__AVX512VL__)
#define SM3_MB_X16 16

typedef struct {
    uint32_t state[8 * SM3_MB_X16];
    uint64_t count[SM3_MB_X16];
    uint8_t  buf[SM3_MB_X16][SM3_BLOCK_SIZE];
    uint32_t buf_len[SM3_MB_X16];
} sm3_mb_x16_ctx_t;

void sm3_mb_x16_init(sm3_mb_x16_ctx_t *ctx);
void sm3_mb_x16_update(sm3_mb_x16_ctx_t *ctx,
                       const uint8_t *msg[SM3_MB_X16], size_t len);
void sm3_mb_x16_final(sm3_mb_x16_ctx_t *ctx,
                      uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE]);
void sm3_mb_x16_hash(const uint8_t *msg[SM3_MB_X16], size_t len,
                     uint8_t digest[SM3_MB_X16][SM3_DIGEST_SIZE]);
#endif /* __AVX512F__ */

/* ---- ARM NEON: 4-way --------------------------------------------- */
#if defined(__aarch64__) || (defined(__ARM_NEON) && defined(__ARM_ARCH_ISA_A64))
#define SM3_MB_X4  4

typedef struct {
    uint32_t state[8 * SM3_MB_X4];
    uint64_t count[SM3_MB_X4];
    uint8_t  buf[SM3_MB_X4][SM3_BLOCK_SIZE];
    uint32_t buf_len[SM3_MB_X4];
} sm3_mb_x4_ctx_t;

void sm3_mb_x4_init(sm3_mb_x4_ctx_t *ctx);
void sm3_mb_x4_update(sm3_mb_x4_ctx_t *ctx,
                      const uint8_t *msg[SM3_MB_X4], size_t len);
void sm3_mb_x4_final(sm3_mb_x4_ctx_t *ctx,
                     uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE]);
void sm3_mb_x4_hash(const uint8_t *msg[SM3_MB_X4], size_t len,
                    uint8_t digest[SM3_MB_X4][SM3_DIGEST_SIZE]);
#endif /* __aarch64__ */

#ifdef __cplusplus
}
#endif

#endif /* SM3_H */
