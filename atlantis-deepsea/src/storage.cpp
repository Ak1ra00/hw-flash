#include "storage.h"
#include "crypto.h"
#include "config.h"
#include <nvs_flash.h>
#include <nvs.h>
#include <string.h>
#include <Arduino.h>

// NVS keys
#define KEY_SETUP   "setup"   // uint8  — 0x01 when configured (written LAST)
#define KEY_WC      "wc"      // uint8  — word count (12 or 24)
#define KEY_CIV     "civ"     // blob 16 — AES-CBC IV
#define KEY_CDATA   "cdata"   // blob 80 — AES256-CBC(AES_KEY, iv, master_key||master_chain)

// Static AES-256 key embedded in firmware.
// No PBKDF2, no hw_pin, no variable derivation — guarantees deterministic
// encrypt/decrypt across every boot and every firmware flash.
static const uint8_t AES_KEY[32] = {
    0x4A, 0x3B, 0x7C, 0x91, 0xDE, 0x25, 0x6F, 0x08,
    0xB3, 0x5E, 0xA1, 0x74, 0x2D, 0xC8, 0x9F, 0x1B,
    0x62, 0x48, 0xE7, 0x3A, 0x0D, 0x95, 0x57, 0xF2,
    0x81, 0x4C, 0xB6, 0x29, 0xD3, 0x6E, 0xAA, 0x5C
};

static bool nvs_open_rw(nvs_handle_t* h) {
    return nvs_open(NVS_NAMESPACE, NVS_READWRITE, h) == ESP_OK;
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
    uint8_t civ[16], plain[64], cipher[80];

    crypto_rand(civ, 16);
    memcpy(plain,      master_key,   32);
    memcpy(plain + 32, master_chain, 32);
    size_t clen = aes256_encrypt(AES_KEY, civ, plain, 64, cipher);
    memset(plain, 0, 64);

    if (clen == 0) return false;

    nvs_handle_t h;
    if (!nvs_open_rw(&h)) return false;

    // Erase stale data first — clean slate before every save
    nvs_erase_all(h);
    nvs_commit(h);

    // Write data first; KEY_SETUP last — so storage_is_setup() is only
    // true when all entries are present and committed.
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

    size_t plen = aes256_decrypt(AES_KEY, civ, cipher, sz, plain);

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
