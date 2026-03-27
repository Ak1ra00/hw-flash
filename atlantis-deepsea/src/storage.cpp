#include "storage.h"
#include "crypto.h"
#include "config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <Arduino.h>

// NVS keys
#define KEY_SETUP   "setup"     // uint8  — 0x01 when configured
#define KEY_WC      "wc"        // uint8  — word count (12 or 24)
#define KEY_PSALT   "psalt"     // blob 16 — PIN PBKDF2 salt
#define KEY_PHASH   "phash"     // blob 32 — PIN key hash (verify without decrypt)
#define KEY_CSALT   "csalt"     // blob 16 — cipher key derivation salt
#define KEY_CIV     "civ"       // blob 16 — AES-CBC IV
#define KEY_CDATA   "cdata"     // blob 80 — encrypted (master_key || master_chain) = 64B → 80B padded
#define KEY_FAILS   "fails"     // uint8  — consecutive wrong PIN count
#define KEY_LOCKTS  "lockts"    // uint32 — lockout start timestamp (ms)

static nvs_handle_t g_nvs;
static bool g_open = false;

static void open_nvs() {
    if (!g_open) {
        nvs_open(NVS_NAMESPACE, NVS_READWRITE, &g_nvs);
        g_open = true;
    }
}

void storage_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
    open_nvs();
}

bool storage_is_setup() {
    open_nvs();
    uint8_t v = 0;
    nvs_get_u8(g_nvs, KEY_SETUP, &v);
    return v == 0x01;
}

bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  const char pin[7], uint8_t word_count) {
    open_nvs();

    // Random salts + IV
    uint8_t psalt[16], csalt[16], civ[16];
    crypto_rand(psalt, 16);
    crypto_rand(csalt, 16);
    crypto_rand(civ,   16);

    // PIN verification hash
    uint8_t phash[32];
    pin_to_key(pin, psalt, phash);

    // Cipher key (different salt from PIN hash)
    uint8_t ckey[32];
    pin_to_key(pin, csalt, ckey);

    // Encrypt master_key || master_chain  (64 bytes → 80 bytes with PKCS7)
    uint8_t plain[64], cipher[80];
    memcpy(plain,      master_key,   32);
    memcpy(plain + 32, master_chain, 32);
    size_t clen = aes256_encrypt(ckey, civ, plain, 64, cipher);
    memset(plain, 0, 64);
    memset(ckey,  0, 32);

    if (clen == 0) return false;

    nvs_set_u8(g_nvs,    KEY_SETUP, 0x01);
    nvs_set_u8(g_nvs,    KEY_WC,    word_count);
    nvs_set_u8(g_nvs,    KEY_FAILS, 0);
    nvs_set_blob(g_nvs,  KEY_PSALT, psalt, 16);
    nvs_set_blob(g_nvs,  KEY_PHASH, phash, 32);
    nvs_set_blob(g_nvs,  KEY_CSALT, csalt, 16);
    nvs_set_blob(g_nvs,  KEY_CIV,   civ,   16);
    nvs_set_blob(g_nvs,  KEY_CDATA, cipher, clen);
    nvs_commit(g_nvs);

    memset(phash, 0, 32); memset(civ, 0, 16);
    return true;
}

bool storage_is_locked_out() {
    open_nvs();
    uint8_t fails = 0;
    nvs_get_u8(g_nvs, KEY_FAILS, &fails);
    if (fails < PIN_MAX_ATTEMPTS) return false;

    uint32_t ts = 0;
    nvs_get_u32(g_nvs, KEY_LOCKTS, &ts);
    uint32_t now = (uint32_t)(millis() & 0xFFFFFFFF);
    return (now - ts) < PIN_LOCKOUT_MS;
}

int storage_attempts_left() {
    open_nvs();
    uint8_t fails = 0;
    nvs_get_u8(g_nvs, KEY_FAILS, &fails);
    int left = PIN_MAX_ATTEMPTS - (int)fails;
    return left < 0 ? 0 : left;
}

bool storage_load(const char pin[7],
                  uint8_t master_key[32], uint8_t master_chain[32]) {
    open_nvs();

    if (storage_is_locked_out()) return false;

    // Verify PIN hash first (cheap check)
    uint8_t psalt[16], phash_stored[32], phash_calc[32];
    size_t sz = 16;
    if (nvs_get_blob(g_nvs, KEY_PSALT, psalt, &sz) != ESP_OK) return false;
    sz = 32;
    if (nvs_get_blob(g_nvs, KEY_PHASH, phash_stored, &sz) != ESP_OK) return false;

    pin_to_key(pin, psalt, phash_calc);
    bool pin_ok = (memcmp(phash_calc, phash_stored, 32) == 0);
    memset(phash_calc, 0, 32);

    if (!pin_ok) {
        uint8_t fails = 0;
        nvs_get_u8(g_nvs, KEY_FAILS, &fails);
        fails++;
        nvs_set_u8(g_nvs, KEY_FAILS, fails);
        if (fails >= PIN_MAX_ATTEMPTS) {
            uint32_t now = (uint32_t)(millis() & 0xFFFFFFFF);
            nvs_set_u32(g_nvs, KEY_LOCKTS, now);
        }
        nvs_commit(g_nvs);
        return false;
    }

    // Reset fail counter on success
    nvs_set_u8(g_nvs, KEY_FAILS, 0);
    nvs_commit(g_nvs);

    // Decrypt master key
    uint8_t csalt[16], civ[16], cipher[80], plain[80];
    sz = 16; nvs_get_blob(g_nvs, KEY_CSALT, csalt, &sz);
    sz = 16; nvs_get_blob(g_nvs, KEY_CIV,   civ,   &sz);
    sz = 80; nvs_get_blob(g_nvs, KEY_CDATA, cipher, &sz);

    uint8_t ckey[32];
    pin_to_key(pin, csalt, ckey);
    size_t plen = aes256_decrypt(ckey, civ, cipher, sz, plain);
    memset(ckey, 0, 32);

    if (plen != 64) {
        memset(plain, 0, sizeof(plain));
        return false;
    }

    memcpy(master_key,   plain,      32);
    memcpy(master_chain, plain + 32, 32);
    memset(plain, 0, sizeof(plain));
    return true;
}

uint8_t storage_word_count() {
    open_nvs();
    uint8_t wc = 12;
    nvs_get_u8(g_nvs, KEY_WC, &wc);
    return wc;
}

void storage_wipe() {
    open_nvs();
    nvs_erase_all(g_nvs);
    nvs_commit(g_nvs);
}
