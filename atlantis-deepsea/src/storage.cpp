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
#define KEY_CDATA   "cdata"     // blob 80 — encrypted (master_key || master_chain)
#define KEY_FAILS   "fails"     // uint8  — consecutive wrong PIN count
#define KEY_LOCKTS  "lockts"    // uint32 — lockout start timestamp (ms, 0 = none)
#define KEY_PINLEN  "pinlen"    // uint8  — PIN_LENGTH when storage was written

// Always open a fresh handle — never cache; BLE stack may reinitialise NVS
// between calls, so a stale handle would silently return ESP_ERR_NVS_INVALID_HANDLE.
static bool nvs_open_rw(nvs_handle_t* h) {
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, h) == ESP_OK;
}

void storage_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // millis() resets on every boot — clear the in-session lockout state.
    // Also reset the failure counter so a legitimate reboot gives a fresh
    // set of attempts (the 5-attempt wipe is a per-session protection).
    nvs_handle_t h;
    if (nvs_open_rw(&h)) {
        nvs_set_u32(h, KEY_LOCKTS, 0);
        nvs_set_u8 (h, KEY_FAILS,  0);
        nvs_commit(h);
        nvs_close(h);
    }
}

bool storage_is_setup() {
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;
    uint8_t v = 0;
    nvs_get_u8(h, KEY_SETUP, &v);
    nvs_close(h);
    return v == 0x01;
}

bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  const char pin[7], uint8_t word_count) {
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

    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    nvs_set_u8  (h, KEY_SETUP,  0x01);
    nvs_set_u8  (h, KEY_PINLEN, (uint8_t)PIN_LENGTH);
    nvs_set_u8  (h, KEY_WC,     word_count);
    nvs_set_u8  (h, KEY_FAILS,  0);
    nvs_set_u32 (h, KEY_LOCKTS, 0);
    nvs_set_blob(h, KEY_PSALT, psalt, 16);
    nvs_set_blob(h, KEY_PHASH, phash, 32);
    nvs_set_blob(h, KEY_CSALT, csalt, 16);
    nvs_set_blob(h, KEY_CIV,   civ,   16);
    nvs_set_blob(h, KEY_CDATA, cipher, clen);
    nvs_commit(h);
    nvs_close(h);

    memset(phash, 0, 32);
    memset(civ,   0, 16);
    return true;
}

bool storage_is_locked_out() {
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    uint8_t fails = 0;
    nvs_get_u8(h, KEY_FAILS, &fails);

    if (fails < PIN_MAX_ATTEMPTS) {
        nvs_close(h);
        return false;
    }

    uint32_t ts = 0;
    nvs_get_u32(h, KEY_LOCKTS, &ts);

    if (ts == 0) {
        // Max fails reached but no in-session timestamp yet (e.g. first check
        // after reboot).  Start a fresh lockout timer now.
        ts = millis();
        nvs_set_u32(h, KEY_LOCKTS, ts);
        nvs_commit(h);
    }
    nvs_close(h);

    return (millis() - ts) < PIN_LOCKOUT_MS;
}

int storage_attempts_left() {
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return PIN_MAX_ATTEMPTS;
    uint8_t fails = 0;
    nvs_get_u8(h, KEY_FAILS, &fails);
    nvs_close(h);
    int left = PIN_MAX_ATTEMPTS - (int)fails;
    return left < 0 ? 0 : left;
}

bool storage_load(const char pin[7],
                  uint8_t master_key[32], uint8_t master_chain[32]) {
    if (storage_is_locked_out()) return false;

    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    // Verify PIN hash
    uint8_t psalt[16], phash_stored[32], phash_calc[32];
    size_t sz = 16;
    bool read_ok = (nvs_get_blob(h, KEY_PSALT, psalt, &sz) == ESP_OK);
    sz = 32;
    read_ok = read_ok && (nvs_get_blob(h, KEY_PHASH, phash_stored, &sz) == ESP_OK);

    if (!read_ok) {
        // NVS unreadable — treat as wrong PIN and count the attempt
        uint8_t fails = 0;
        nvs_get_u8(h, KEY_FAILS, &fails);
        fails++;
        nvs_set_u8(h, KEY_FAILS, fails);
        if (fails >= PIN_MAX_ATTEMPTS) {
            nvs_set_u32(h, KEY_LOCKTS, millis());
        }
        nvs_commit(h);
        nvs_close(h);
        return false;
    }

    pin_to_key(pin, psalt, phash_calc);
    bool pin_ok = (memcmp(phash_calc, phash_stored, 32) == 0);
    memset(phash_calc, 0, 32);

    if (!pin_ok) {
        uint8_t fails = 0;
        nvs_get_u8(h, KEY_FAILS, &fails);
        fails++;
        nvs_set_u8(h, KEY_FAILS, fails);
        if (fails >= PIN_MAX_ATTEMPTS) {
            nvs_set_u32(h, KEY_LOCKTS, millis());
        }
        nvs_commit(h);
        nvs_close(h);
        return false;
    }

    // PIN correct — reset fail counter
    nvs_set_u8(h, KEY_FAILS, 0);
    nvs_commit(h);

    // Decrypt master key
    uint8_t csalt[16], civ[16], cipher[80], plain[80];
    sz = 16; nvs_get_blob(h, KEY_CSALT, csalt,  &sz);
    sz = 16; nvs_get_blob(h, KEY_CIV,   civ,    &sz);
    sz = 80; nvs_get_blob(h, KEY_CDATA, cipher, &sz);
    nvs_close(h);

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
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return 12;
    uint8_t wc = 12;
    nvs_get_u8(h, KEY_WC, &wc);
    nvs_close(h);
    return wc;
}

void storage_wipe() {
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
}
