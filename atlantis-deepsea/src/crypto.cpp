#include "crypto.h"
#include "wordlist.h"
#include <string.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <mbedtls/bignum.h>
#include <mbedtls/aes.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>

// ── secp256k1 group order ────────────────────────────────────────────────────
static const uint8_t SECP256K1_N[32] = {
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFE,
    0xBA,0xAE,0xDC,0xE6,0xAF,0x48,0xA0,0x3B,
    0xBF,0xD2,0x5E,0x8C,0xD0,0x36,0x41,0x41
};

// BIP85 base-85 charset (85 printable ASCII chars — official spec)
static const char BASE85[86] =
    "0123456789"
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "!#$%&()*+-;<=>?@^_`{|}~";  // 10+26+26+23 = 85

// ── Internal helpers ─────────────────────────────────────────────────────────

static void hmac_sha512(const uint8_t* key, size_t klen,
                        const uint8_t* data, size_t dlen,
                        uint8_t out[64]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), 1);
    mbedtls_md_hmac_starts(&ctx, key, klen);
    mbedtls_md_hmac_update(&ctx, data, dlen);
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

// ── BIP39 ────────────────────────────────────────────────────────────────────

int bip39_word_index(const char* word) {
    // Binary search (list is sorted)
    int lo = 0, hi = 2047;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        int c = strcmp(BIP39_WORDLIST[mid], word);
        if (c == 0) return mid;
        if (c < 0)  lo = mid + 1;
        else        hi = mid - 1;
    }
    return -1;
}

int bip39_prefix_match(const char* prefix, int* out, int max_out) {
    size_t plen = strlen(prefix);
    int found = 0;
    for (int i = 0; i < 2048 && found < max_out; i++) {
        if (strncmp(BIP39_WORDLIST[i], prefix, plen) == 0)
            out[found++] = i;
        // Early exit: wordlist is sorted, once we pass prefix we're done
        else if (found > 0)
            break;
    }
    return found;
}

bool bip39_validate(const char* const* words, int count) {
    if (count != 12 && count != 24) return false;

    // Pack word indices into bit array
    uint8_t bits[33] = {0};   // 264 bits max
    int bit_pos = 0;
    for (int w = 0; w < count; w++) {
        int idx = bip39_word_index(words[w]);
        if (idx < 0) return false;
        // Each word = 11 bits, MSB first
        for (int b = 10; b >= 0; b--) {
            if (idx & (1 << b))
                bits[bit_pos >> 3] |= (0x80 >> (bit_pos & 7));
            bit_pos++;
        }
    }

    // Entropy bytes + checksum
    int ent_bytes = (count == 12) ? 16 : 32;
    int cs_bits   = ent_bytes / 4;   // 4 or 8

    uint8_t hash[32];
    mbedtls_sha256(bits, ent_bytes, hash, 0);

    uint8_t stored   = bits[ent_bytes] >> (8 - cs_bits);
    uint8_t computed = hash[0]         >> (8 - cs_bits);
    return stored == computed;
}

void bip39_to_seed(const char* const* words, int count, uint8_t seed[64]) {
    // Build space-joined mnemonic string
    char mnemonic[24 * 10] = {0};
    for (int i = 0; i < count; i++) {
        if (i) strncat(mnemonic, " ", sizeof(mnemonic) - strlen(mnemonic) - 1);
        strncat(mnemonic, words[i], sizeof(mnemonic) - strlen(mnemonic) - 1);
    }

    const char salt[] = "mnemonic";

    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA512), 1);
    mbedtls_pkcs5_pbkdf2_hmac(&ctx,
        (const uint8_t*)mnemonic, strlen(mnemonic),
        (const uint8_t*)salt, strlen(salt),
        2048, 64, seed);
    mbedtls_md_free(&ctx);

    // Clear sensitive data
    memset(mnemonic, 0, sizeof(mnemonic));
}

// ── BIP32 ────────────────────────────────────────────────────────────────────

void bip32_master(const uint8_t* seed, size_t seed_len,
                  uint8_t key[32], uint8_t chain[32]) {
    uint8_t I[64];
    static const uint8_t SEED_KEY[] = "Bitcoin seed";
    hmac_sha512(SEED_KEY, 12, seed, seed_len, I);
    memcpy(key,   I,    32);
    memcpy(chain, I+32, 32);
    memset(I, 0, 64);
}

bool bip32_child_hard(const uint8_t key[32], const uint8_t chain[32],
                      uint32_t index,
                      uint8_t child_key[32], uint8_t child_chain[32]) {
    uint32_t idx = index | 0x80000000u;

    uint8_t data[37];
    data[0] = 0x00;
    memcpy(data + 1, key, 32);
    data[33] = (idx >> 24) & 0xFF;
    data[34] = (idx >> 16) & 0xFF;
    data[35] = (idx >>  8) & 0xFF;
    data[36] =  idx        & 0xFF;

    uint8_t I[64];
    hmac_sha512(chain, 32, data, 37, I);
    memset(data, 0, sizeof(data));

    // child_key = (IL + parent_key) mod n  [256-bit arithmetic]
    mbedtls_mpi IL, pk, n, res;
    mbedtls_mpi_init(&IL); mbedtls_mpi_init(&pk);
    mbedtls_mpi_init(&n);  mbedtls_mpi_init(&res);

    mbedtls_mpi_read_binary(&IL, I,          32);
    mbedtls_mpi_read_binary(&pk, key,        32);
    mbedtls_mpi_read_binary(&n,  SECP256K1_N, 32);

    bool ok = (mbedtls_mpi_cmp_mpi(&IL, &n) < 0);
    if (ok) {
        mbedtls_mpi_add_mpi(&res, &IL, &pk);
        mbedtls_mpi_mod_mpi(&res, &res, &n);
        ok = (mbedtls_mpi_cmp_int(&res, 0) != 0);
    }
    if (ok) {
        mbedtls_mpi_write_binary(&res, child_key, 32);
        memcpy(child_chain, I + 32, 32);
    }

    mbedtls_mpi_free(&IL); mbedtls_mpi_free(&pk);
    mbedtls_mpi_free(&n);  mbedtls_mpi_free(&res);
    memset(I, 0, 64);
    return ok;
}

// ── BIP85 ────────────────────────────────────────────────────────────────────

bool bip85_password(const uint8_t master_key[32], const uint8_t master_chain[32],
                    uint8_t pwd_len, uint32_t index, char* out) {
    uint8_t key[32], chain[32], k[32], c[32];
    memcpy(key,   master_key,   32);
    memcpy(chain, master_chain, 32);

    // m/83696968'/707764'/pwd_len'/index'
    const uint32_t path[4] = {83696968u, 707764u, (uint32_t)pwd_len, index};
    for (int i = 0; i < 4; i++) {
        if (!bip32_child_hard(key, chain, path[i], k, c)) {
            memset(key, 0, 32); memset(chain, 0, 32);
            return false;
        }
        memcpy(key, k, 32); memcpy(chain, c, 32);
    }

    // entropy = HMAC-SHA512("bip-entropy-from-k", child_key)
    uint8_t entropy[64];
    static const uint8_t ENT_KEY[] = "bip-entropy-from-k";
    hmac_sha512(ENT_KEY, 18, key, 32, entropy);
    memset(key, 0, 32); memset(chain, 0, 32);

    // Big-integer base-85 encoding (matches BIP85 Python reference)
    mbedtls_mpi E, d85, rem;
    mbedtls_mpi_init(&E); mbedtls_mpi_init(&d85); mbedtls_mpi_init(&rem);
    mbedtls_mpi_read_binary(&E, entropy, 64);
    mbedtls_mpi_lset(&d85, 85);

    for (uint8_t i = 0; i < pwd_len; i++) {
        mbedtls_mpi_mod_mpi(&rem, &E, &d85);
        mbedtls_mpi_div_mpi(&E, NULL, &E, &d85);
        uint8_t r = 0;
        mbedtls_mpi_write_binary(&rem, &r, 1);
        out[pwd_len - 1 - i] = BASE85[r];  // reversed, per spec
    }
    out[pwd_len] = '\0';

    mbedtls_mpi_free(&E); mbedtls_mpi_free(&d85); mbedtls_mpi_free(&rem);
    memset(entropy, 0, 64);
    return true;
}

// ── Symmetric crypto ─────────────────────────────────────────────────────────

void crypto_rand(uint8_t* buf, size_t len) {
    for (size_t i = 0; i < len; i += 4) {
        uint32_t r = esp_random();
        size_t chunk = (len - i < 4) ? (len - i) : 4;
        memcpy(buf + i, &r, chunk);
    }
}

void pin_to_key(const char pin[7], const uint8_t salt[16], uint8_t key[32]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    mbedtls_pkcs5_pbkdf2_hmac(&ctx,
        (const uint8_t*)pin, strlen(pin),
        salt, 16,
        10000, 32, key);
    mbedtls_md_free(&ctx);
}

size_t aes256_encrypt(const uint8_t key[32], const uint8_t iv[16],
                      const uint8_t* plain, size_t plain_len,
                      uint8_t* cipher_out) {
    // PKCS7 padding
    size_t padded = ((plain_len + 15) / 16) * 16;
    uint8_t pad_byte = (uint8_t)(padded - plain_len);
    uint8_t buf[padded];
    memcpy(buf, plain, plain_len);
    memset(buf + plain_len, pad_byte, padded - plain_len);

    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_enc(&ctx, key, 256);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_ENCRYPT, padded, iv_copy, buf, cipher_out);
    mbedtls_aes_free(&ctx);
    memset(buf, 0, padded);
    return padded;
}

size_t aes256_decrypt(const uint8_t key[32], const uint8_t iv[16],
                      const uint8_t* cipher, size_t cipher_len,
                      uint8_t* plain_out) {
    if (cipher_len == 0 || cipher_len % 16 != 0) return 0;

    uint8_t iv_copy[16];
    memcpy(iv_copy, iv, 16);

    mbedtls_aes_context ctx;
    mbedtls_aes_init(&ctx);
    mbedtls_aes_setkey_dec(&ctx, key, 256);
    mbedtls_aes_crypt_cbc(&ctx, MBEDTLS_AES_DECRYPT, cipher_len, iv_copy, cipher, plain_out);
    mbedtls_aes_free(&ctx);

    // Remove PKCS7 padding
    uint8_t pad = plain_out[cipher_len - 1];
    if (pad == 0 || pad > 16) return 0;
    return cipher_len - pad;
}
