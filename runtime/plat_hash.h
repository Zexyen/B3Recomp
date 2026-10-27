/* [witchtime] SHA-1 and MD5 for the CryptoAPI subset of plat_posix.c (the game hashes with nn::crypto's Sha1Generator
 * and Md5Generator, nnmisc.c), so the runtime needs no libcrypto. Incremental, and the digest can be read while hashing
 * goes on (CryptGetHashParam may be asked more than once). Checked against OpenSSL by tests/plat_hash_test.c. */
#ifndef PLAT_HASH_H
#define PLAT_HASH_H
#include <stdint.h>
#include <string.h>

typedef struct {
    int sha1;                                  /* 1 = SHA-1 (20 bytes, big-endian), 0 = MD5 (16 bytes, little-endian) */
    uint32_t s[5];
    uint64_t len;                              /* bytes hashed */
    uint8_t buf[64];
} PlatHash;

static uint32_t ph_rol(uint32_t v, int n) { return v << n | v >> (32 - n); }

static void ph_sha1_block(uint32_t *s, const uint8_t *p) {
    uint32_t w[80], a = s[0], b = s[1], c = s[2], d = s[3], e = s[4];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = ph_rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5a827999u; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ed9eba1u; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8f1bbcdcu; }
        else { f = b ^ c ^ d; k = 0xca62c1d6u; }
        uint32_t t = ph_rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = ph_rol(b, 30); b = a; a = t;
    }
    s[0] += a; s[1] += b; s[2] += c; s[3] += d; s[4] += e;
}

static void ph_md5_block(uint32_t *s, const uint8_t *p) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[16] = {7, 12, 17, 22, 5, 9, 14, 20, 4, 11, 16, 23, 6, 10, 15, 21};
    uint32_t m[16], a = s[0], b = s[1], c = s[2], d = s[3];
    for (int i = 0; i < 16; i++) m[i] = (uint32_t)p[4 * i] | (uint32_t)p[4 * i + 1] << 8 | (uint32_t)p[4 * i + 2] << 16 | (uint32_t)p[4 * i + 3] << 24;
    for (int i = 0; i < 64; i++) {
        uint32_t f; int g;
        if (i < 16) { f = (b & c) | (~b & d); g = i; }
        else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) & 15; }
        else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) & 15; }
        else { f = c ^ (b | ~d); g = (7 * i) & 15; }
        uint32_t t = d;
        d = c; c = b;
        b = b + ph_rol(a + f + K[i] + m[g], R[(i >> 4) * 4 + (i & 3)]);
        a = t;
    }
    s[0] += a; s[1] += b; s[2] += c; s[3] += d;
}

static void ph_init(PlatHash *h, int sha1) {
    static const uint32_t init[5] = {0x67452301u, 0xefcdab89u, 0x98badcfeu, 0x10325476u, 0xc3d2e1f0u};
    memset(h, 0, sizeof *h);
    h->sha1 = sha1;
    memcpy(h->s, init, sizeof init);
}

static void ph_update(PlatHash *h, const uint8_t *p, size_t n) {
    while (n) {
        size_t at = (size_t)(h->len & 63), take = 64 - at < n ? 64 - at : n;
        memcpy(h->buf + at, p, take);
        h->len += take; p += take; n -= take;
        if (!(h->len & 63)) (h->sha1 ? ph_sha1_block : ph_md5_block)(h->s, h->buf);
    }
}

/* the digest of what was hashed so far (h itself is not changed); returns its size */
static unsigned ph_final(const PlatHash *h, uint8_t *out) {
    PlatHash c = *h;
    uint64_t bits = c.len * 8;
    uint8_t pad[72] = {0x80}, tail[8];
    for (int i = 0; i < 8; i++) tail[i] = (uint8_t)(c.sha1 ? bits >> (56 - 8 * i) : bits >> (8 * i));
    ph_update(&c, pad, (size_t)((119 - (c.len & 63)) & 63) + 1);
    ph_update(&c, tail, 8);
    unsigned words = c.sha1 ? 5 : 4;
    for (unsigned i = 0; i < words; i++)
        for (int j = 0; j < 4; j++) out[4 * i + j] = (uint8_t)(c.sha1 ? c.s[i] >> (24 - 8 * j) : c.s[i] >> (8 * j));
    return words * 4;
}
#endif
