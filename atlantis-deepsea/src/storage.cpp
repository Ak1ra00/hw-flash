#include "storage.h"
#include "crypto.h"
#include "config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <Arduino.h>

// NVS keys — only 4 entries now; no phash to fail verification
#define KEY_SETUP   "setup"   // uint8  — 0x01 when configured (written LAST)
#define KEY_WC      "wc"      // uint8  — word count (12 or 24)
#define KEY_CIV     "civ"     // blob 16 — AES-CBC IV
#define KEY_CDATA   "cdata"   // blob 80 — AES256-CBC(ckey, iv, master_key||master_chain)

// Fixed salt for cipher key derivation.  Device-specific key comes from
// hw_pin(); this salt makes the derived key format-specific.
// Never change this value — it would invalidate all stored seeds.
static const uint8_t CIPHER_SALT[16] = {
    0xA7, 0x3D, 0x5C, 0x82, 0x1F, 0xE4, 0x09, 0xB6,
    0x7E, 0x2A, 0xC1, 0x58, 0xD3, 0x4F, 0x96, 0x0E
};

static bool nvs_open_rw(nvs_handle_t* h) {
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, h) == ESP_OK;
}

static void hw_pin(char pin[20]) {
    uint64_t mac = ESP.getEfuseMac();
    snprintf(pin, 20, "%016llX", (unsigned long long)mac);
}

// Derive the AES cipher key deterministically from the device eFuse MAC.
// Same device → same key every time.  No random salt, no stored comparison.
static void derive_ckey(uint8_t ckey[32]) {
    char pin[20];
    hw_pin(pin);
    pin_to_key(pin, CIPHER_SALT, ckey);
}

void storage_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
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
                  uint8_t word_count) {
    uint8_t ckey[32], civ[16], plain[64], cipher[80];

    derive_ckey(ckey);
    crypto_rand(civ, 16);

    memcpy(plain,      master_key,   32);
    memcpy(plain + 32, master_chain, 32);
    size_t clen = aes256_encrypt(ckey, civ, plain, 64, cipher);
    memset(plain, 0, 64);
    memset(ckey,  0, 32);

    if (clen == 0) return false;

    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    // Erase any stale data (old format entries, partial writes, etc.)
    nvs_erase_all(h);
    nvs_commit(h);

    // Write data blobs first; KEY_SETUP written LAST so storage_is_setup()
    // is only true when all entries are present and committed.
    bool ok = true;
    ok = ok && (nvs_set_u8  (h, KEY_WC,    word_count)   == ESP_OK);
    ok = ok && (nvs_set_blob(h, KEY_CIV,   civ,   16)    == ESP_OK);
    ok = ok && (nvs_set_blob(h, KEY_CDATA, cipher, clen) == ESP_OK);
    if (ok) ok = (nvs_set_u8(h, KEY_SETUP, 0x01)         == ESP_OK);

    if (ok) {
        nvs_commit(h);
    } else {
        nvs_erase_all(h);
        nvs_commit(h);
    }

    nvs_close(h);
    memset(civ, 0, 16);
    return ok;
}

bool storage_load(uint8_t master_key[32], uint8_t master_chain[32]) {
    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    uint8_t civ[16], cipher[80], plain[80];
    size_t sz;

    sz = sizeof(civ);
    bool ok = (nvs_get_blob(h, KEY_CIV,   civ,    &sz) == ESP_OK);
    sz = sizeof(cipher);
    ok = ok && (nvs_get_blob(h, KEY_CDATA, cipher, &sz) == ESP_OK);
    nvs_close(h);

    if (!ok) return false;

    uint8_t ckey[32];
    derive_ckey(ckey);
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
