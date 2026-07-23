#ifndef SM4_H
#define SM4_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* 常量定义 */
#define SM4_BLOCK_SIZE 16
#define SM4_KEY_SIZE   16
#define SM4_ROUNDS     32

/* SM4上下文结构 */
typedef struct {
    uint32_t rk[SM4_ROUNDS];     /* 原始轮密钥 */
    uint32_t enc_rk[SM4_ROUNDS]; /* 加密轮密钥 */
    uint32_t dec_rk[SM4_ROUNDS]; /* 解密轮密钥 */
    /* CTR模式 */
    uint8_t iv[SM4_BLOCK_SIZE];
    uint64_t counter;
} SM4_Ctx;

/* 基础实现接口 */
void sm4_baseline_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_baseline_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_baseline_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

/* T-table优化接口 */
void sm4_ttable_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_ttable_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_ttable_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

/* Shuffle优化接口 (多块并行) */
void sm4_shuffle_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_shuffle_encrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in, 
                                 uint8_t *out, size_t num_blocks);
void sm4_shuffle_decrypt_blocks(const SM4_Ctx *ctx, const uint8_t *in, 
                                 uint8_t *out, size_t num_blocks);

/* CTR模式接口 */
void sm4_ctr_init(SM4_Ctx *ctx, const uint8_t *key, 
                  const uint8_t *iv, uint64_t counter);
void sm4_ctr_encrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);
void sm4_ctr_decrypt(SM4_Ctx *ctx, const uint8_t *in, uint8_t *out, size_t length);

/* 性能测试函数 */
void sm4_perf_test(void);

#endif /* SM4_H */
