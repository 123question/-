#ifndef SM4_AVX512_H
#define SM4_AVX512_H

#include "sm4.h"

#ifdef __cplusplus
extern "C" {
#endif

/* AVX-512 16路并行上下文 */
#define SM4_MB_X16 16

typedef struct {
    uint32_t state[8 * SM4_MB_X16];  /* 8个状态字 × 16路 */
    uint8_t buf[SM4_MB_X16][SM4_BLOCK_SIZE];  /* 缓冲区 */
    uint32_t buf_len[SM4_MB_X16];    /* 各缓冲区长度 */
    uint64_t count[SM4_MB_X16];      /* 已处理字节数 */
} sm4_mb_x16_ctx_t;

/* 密钥扩展 - 16路并行 */
void sm4_mb_x16_set_key(sm4_mb_x16_ctx_t *ctx, const uint8_t *key[SM4_MB_X16]);

/* 加密/解密 - 16路并行 */
void sm4_mb_x16_encrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks);

void sm4_mb_x16_decrypt_blocks(const sm4_mb_x16_ctx_t *ctx,
                                const uint8_t *in[SM4_MB_X16],
                                uint8_t *out[SM4_MB_X16],
                                size_t num_blocks);

/* 单路接口（兼容原有API）*/
void sm4_avx512_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_avx512_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_avx512_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

#ifdef __cplusplus
}
#endif

#endif /* SM4_AVX512_H */
