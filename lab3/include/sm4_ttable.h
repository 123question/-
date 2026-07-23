#ifndef SM4_TTABLE_H
#define SM4_TTABLE_H

#include "sm4.h"

/* T-table优化实现 */
void sm4_ttable_set_key(SM4_Ctx *ctx, const uint8_t *key);
void sm4_ttable_encrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);
void sm4_ttable_decrypt_block(const SM4_Ctx *ctx, const uint8_t *in, uint8_t *out);

#endif /* SM4_TTABLE_H */
