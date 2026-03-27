#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ── BIP39 ────────────────────────────────────────────────────────────────────
// Return index of word in wordlist, or -1 if not found
int  bip39_word_index(const char* word);

// Fill indices_out with indices of words matching prefix; returns count found (max max_out)
int  bip39_prefix_match(const char* prefix, int* indices_out, int max_out);

// Validate mnemonic checksum (words[] must be null-terminated strings)
bool bip39_validate(const char* const* words, int count);

// Convert mnemonic to 64-byte BIP39 seed (PBKDF2-SHA512, 2048 iters)
void bip39_to_seed(const char* const* words, int count, uint8_t seed[64]);

// ── BIP32 ────────────────────────────────────────────────────────────────────
// Derive master key/chain from seed bytes
void bip32_master(const uint8_t* seed, size_t seed_len,
                  uint8_t key[32], uint8_t chain[32]);

// Derive hardened child key  (adds 0x80000000 internally)
// Returns false if derived key is invalid (retry with next index)
bool bip32_child_hard(const uint8_t key[32], const uint8_t chain[32],
                      uint32_t index,
                      uint8_t child_key[32], uint8_t child_chain[32]);

// ── BIP85 ────────────────────────────────────────────────────────────────────
// Derive a deterministic base85 password
// Path: m/83696968'/707764'/pwd_len'/index'
// out must be at least pwd_len+1 bytes
bool bip85_password(const uint8_t master_key[32], const uint8_t master_chain[32],
                    uint8_t pwd_len, uint32_t index, char* out);

// ── Symmetric crypto ─────────────────────────────────────────────────────────
// Fill buf with cryptographically random bytes
void crypto_rand(uint8_t* buf, size_t len);

// Derive 32-byte AES key from 6-digit PIN string + 16-byte salt (PBKDF2-SHA256, 10000 iters)
void pin_to_key(const char pin[7], const uint8_t salt[16], uint8_t key[32]);

// AES-256-CBC encrypt/decrypt in-place (padded to 16-byte boundary with PKCS7)
// cipher_out must be at least ((plain_len+15)/16)*16 bytes
// Returns ciphertext length, or 0 on error
size_t aes256_encrypt(const uint8_t key[32], const uint8_t iv[16],
                      const uint8_t* plain, size_t plain_len,
                      uint8_t* cipher_out);

// Returns plaintext length, or 0 on error
size_t aes256_decrypt(const uint8_t key[32], const uint8_t iv[16],
                      const uint8_t* cipher, size_t cipher_len,
                      uint8_t* plain_out);
