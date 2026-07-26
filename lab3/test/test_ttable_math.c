#include <stdio.h>
#include <stdint.h>

static inline uint32_t ROTL32(uint32_t x, int n) {
    n = n % 32;
    return (x << n) | (x >> (32 - n));
}

int main() {
    uint8_t sbox[4] = {0xd6, 0x90, 0x00, 0xff};
    for (int si = 0; si < 4; si++) {
        uint32_t s = sbox[si];
        uint32_t ls = s ^ ROTL32(s,2) ^ ROTL32(s,10) ^ ROTL32(s,18) ^ ROTL32(s,24);
        printf("s=0x%02x, L(s)=0x%08x\n", s, ls);
        
        uint32_t s3 = (uint32_t)s << 0;
        uint32_t T3 = s3 ^ ROTL32(s3,2) ^ ROTL32(s3,10) ^ ROTL32(s3,18) ^ ROTL32(s3,24);
        uint32_t s2 = (uint32_t)s << 8;
        uint32_t T2 = s2 ^ ROTL32(s2,2) ^ ROTL32(s2,10) ^ ROTL32(s2,18) ^ ROTL32(s2,24);
        uint32_t s1 = (uint32_t)s << 16;
        uint32_t T1 = s1 ^ ROTL32(s1,2) ^ ROTL32(s1,10) ^ ROTL32(s1,18) ^ ROTL32(s1,24);
        uint32_t s0 = (uint32_t)s << 24;
        uint32_t T0 = s0 ^ ROTL32(s0,2) ^ ROTL32(s0,10) ^ ROTL32(s0,18) ^ ROTL32(s0,24);
        
        printf("  T0(idx768)=0x%08x ROTL24(L(s))=0x%08x match=%d\n", T0, ROTL32(ls,24), T0==ROTL32(ls,24));
        printf("  T1(idx512)=0x%08x ROTL16(L(s))=0x%08x match=%d\n", T1, ROTL32(ls,16), T1==ROTL32(ls,16));
        printf("  T2(idx256)=0x%08x ROTL8(L(s))=0x%08x  match=%d\n",  T2, ROTL32(ls,8),  T2==ROTL32(ls,8));
        printf("  T3(idx0)=0x%08x   L(s)=0x%08x          match=%d\n", T3, ls, T3==ls);
        printf("\n");
    }
    return 0;
}
