#include "storage.h"
#include "crypto.h"
#include "config.h"
#include <nvs_flash.h>
#include <esp_partition.h>
#include <string.h>
#include <Arduino.h>

// ── Seed partition ────────────────────────────────────────────────────────────
// Data lives in a dedicated 4 KB flash partition ("seed", subtype 0x99,
// offset 0xC000).  NVS is never used for seed data — this partition is
// invisible to the NVS layer, BLE stack, and esptool.

#define SEED_MAGIC_0  0xAD
#define SEED_MAGIC_1  0xEE
#define SEED_MAGIC_2  0x75
#define SEED_MAGIC_3  0xEA

// Record layout (128 bytes, 4-byte aligned):
//   [0-3]    magic
//   [4]      word_count
//   [5-7]    reserved (0x00)
//   [8-23]   AES-CBC IV  (16 bytes)
//   [24-103] AES256-CBC(AES_KEY, iv, master_key||master_chain)  (80 bytes)
//   [104-127] padding (0x00)
#define RECORD_SIZE   128

// Static AES-256 key — deterministic, no derivation, no variable inputs.
static const uint8_t AES_KEY[32] = {
    0x4A, 0x3B, 0x7C, 0x91, 0xDE, 0x25, 0x6F, 0x08,
    0xB3, 0x5E, 0xA1, 0x74, 0x2D, 0xC8, 0x9F, 0x1B,
    0x62, 0x48, 0xE7, 0x3A, 0x0D, 0x95, 0x57, 0xF2,
    0x81, 0x4C, 0xB6, 0x29, 0xD3, 0x6E, 0xAA, 0x5C
};

static const esp_partition_t* seed_partition() {
    return esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        (esp_partition_subtype_t)0x99,
        "seed");
}

// storage_init() only initialises NVS for the BLE stack.
// Our seed data never touches NVS.
void storage_init() {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

bool storage_is_setup() {
    const esp_partition_t* p = seed_partition();
    if (!p) return false;
    uint8_t magic[4] = {0};
    esp_partition_read(p, 0, magic, sizeof(magic));
    return magic[0] == SEED_MAGIC_0 && magic[1] == SEED_MAGIC_1 &&
           magic[2] == SEED_MAGIC_2 && magic[3] == SEED_MAGIC_3;
}

bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  uint8_t word_count) {
    const esp_partition_t* p = seed_partition();
    if (!p) return false;

    uint8_t plain[64], cipher[80], civ[16];
    crypto_rand(civ, 16);
    memcpy(plain,      master_key,   32);
    memcpy(plain + 32, master_chain, 32);
    size_t clen = aes256_encrypt(AES_KEY, civ, plain, 64, cipher);
    memset(plain, 0, sizeof(plain));
    if (clen == 0) return false;

    // Build 128-byte record
    uint8_t rec[RECORD_SIZE] = {0};
    rec[0] = SEED_MAGIC_0;
    rec[1] = SEED_MAGIC_1;
    rec[2] = SEED_MAGIC_2;
    rec[3] = SEED_MAGIC_3;
    rec[4] = word_count;
    memcpy(rec + 8,  civ,    16);
    memcpy(rec + 24, cipher, 80);

    // Erase sector then write — esp_partition handles alignment
    bool ok = (esp_partition_erase_range(p, 0, p->size) == ESP_OK);
    if (ok) ok = (esp_partition_write(p, 0, rec, RECORD_SIZE) == ESP_OK);

    memset(rec, 0, sizeof(rec));
    memset(civ, 0, sizeof(civ));
    return ok;
}

bool storage_load(uint8_t master_key[32], uint8_t master_chain[32]) {
    const esp_partition_t* p = seed_partition();
    if (!p) return false;

    uint8_t rec[RECORD_SIZE] = {0};
    if (esp_partition_read(p, 0, rec, RECORD_SIZE) != ESP_OK) return false;

    // Verify magic
    if (rec[0] != SEED_MAGIC_0 || rec[1] != SEED_MAGIC_1 ||
        rec[2] != SEED_MAGIC_2 || rec[3] != SEED_MAGIC_3) {
        return false;
    }

    uint8_t plain[80] = {0};
    size_t plen = aes256_decrypt(AES_KEY, rec + 8, rec + 24, 80, plain);
    memset(rec, 0, sizeof(rec));

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
    const esp_partition_t* p = seed_partition();
    if (!p) return 12;
    uint8_t wc = 12;
    esp_partition_read(p, 4, &wc, 1);
    return wc;
}

void storage_wipe() {
    const esp_partition_t* p = seed_partition();
    if (!p) return;
    esp_partition_erase_range(p, 0, p->size);
    // magic is gone → storage_is_setup() returns false
}
