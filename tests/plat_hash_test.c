/* runtime/plat_hash.h against OpenSSL: random inputs of 0..300 bytes fed in random pieces, the digest read in between
 * and at the end, SHA-1 and MD5.   clang -O2 -I runtime tests/plat_hash_test.c -lcrypto -o build/plat_hash_test */
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include "plat_hash.h"

int main(void) {
    uint8_t data[300], want[20], got[20];
    int bad = 0, cases = 0;
    srand(1);
    for (int i = 0; i < 20000; i++) {
        int sha1 = i & 1, n = rand() % (int)sizeof data;
        for (int j = 0; j < n; j++) data[j] = (uint8_t)rand();
        PlatHash h;
        ph_init(&h, sha1);
        for (int at = 0; at < n;) {
            int k = 1 + rand() % 80;
            if (k > n - at) k = n - at;
            ph_update(&h, data + at, (size_t)k);
            at += k;
            if (rand() % 4 == 0) ph_final(&h, got);   /* reading the digest early changes nothing */
        }
        unsigned len = ph_final(&h, got), wl = 0;
        EVP_Digest(data, (size_t)n, want, &wl, sha1 ? EVP_sha1() : EVP_md5(), NULL);
        cases++;
        if (len != wl || memcmp(got, want, len)) { if (bad++ < 5) printf("FAIL %s, %d bytes\n", sha1 ? "sha1" : "md5", n); }
    }
    printf("plat_hash: %d cases, %d failures\n", cases, bad);
    return bad != 0;
}
