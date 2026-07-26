#ifndef SM4_BASELINE_H
#define SM4_BASELINE_H

#include "sm4.h"

/* 基础实现 - 作为性能对比基准 */
void sm4_baseline_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_baseline_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_baseline_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

#endif /* SM4_BASELINE_H */
