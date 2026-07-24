#ifndef SM4_H
#define SM4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ============================================================================
 * SM4 基本常量定义
 * ============================================================================ */

#define SM4_BLOCK_SIZE 16
#define SM4_KEY_SIZE   16
#define SM4_ROUNDS     32

/* ============================================================================
 * SM4 上下文结构
 * ============================================================================ */

typedef struct {
    uint32_t rk[SM4_ROUNDS];     /* 原始轮密钥 */
    uint32_t enc_rk[SM4_ROUNDS]; /* 加密轮密钥 */
    uint32_t dec_rk[SM4_ROUNDS]; /* 解密轮密钥 */
    /* CTR模式 */
    uint8_t iv[SM4_BLOCK_SIZE];
    uint64_t counter;
} SM4_Ctx;

/* ============================================================================
 * 1. 基础实现 (Baseline)
 * ============================================================================ */

void sm4_baseline_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_baseline_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_baseline_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

/* ============================================================================
 * 2. T-table 优化
 * ============================================================================ */

void sm4_ttable_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_ttable_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_ttable_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

/* ============================================================================
 * 3. Shuffle 优化 (比特切片, 8块并行)
 * ============================================================================ */

void sm4_shuffle_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_shuffle_encrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in,
                                 uint8_t *out, size_t num_blocks);
void sm4_shuffle_decrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in,
                                 uint8_t *out, size_t num_blocks);

/* ============================================================================
 * 4. CTR 工作模式
 * ============================================================================ */

void sm4_ctr_init(SM4_Ctx *ctx, const uint8_t *key,
                  const uint8_t *iv, uint64_t counter);
void sm4_ctr_encrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);
void sm4_ctr_decrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);

/* ============================================================================
 * 5. AVX-512 优化 (需要 CPU 支持)
 * ============================================================================ */

#ifdef __AVX512F__

/* 单路接口 - 兼容原有API */
void sm4_avx512_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_avx512_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_avx512_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

/* 16路并行接口 */
#define SM4_MB_X16 16

typedef struct {
    uint32_t state[8 * SM4_MB_X16];          /* 轮密钥存储: 32轮 × 16路 */
    uint8_t buf[SM4_MB_X16][SM4_BLOCK_SIZE]; /* 缓冲区 */
    uint32_t buf_len[SM4_MB_X16];            /* 各缓冲区长度 */
    uint64_t count[SM4_MB_X16];              /* 已处理字节数 */
} sm4_mb_x16_ctx_t;

/* 16路并行密钥扩展 */
void sm4_mb_x16_set_key(sm4_mb_x16_ctx_t *ctx, const uint8_t *key[SM4_MB_X16]);

/* 16路并行加密/解密 */
void sm4_mb_x16_encrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks);

void sm4_mb_x16_decrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks);

#endif /* __AVX512F__ */

/* ============================================================================
 * 6. 性能测试函数 
 * ============================================================================ */

void sm4_perf_test(void);

#endif /* SM4_H */
