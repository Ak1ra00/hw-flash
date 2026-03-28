#include "storage.h"
#include "crypto.h"
#include "config.h"
#include <nvs_flash.h>
#include <esp_partition.h>
#include <string.h>
#include <Arduino.h>

// ── Seed partition ─────────────────────────────────────────────────────────────
// Dedicated 4 KB flash partition ("seed", subtype 0x99, offset 0xC000).
// Never touched by esptool, NVS, or BLE.  Raw reads/writes via esp_partition
// _raw APIs to bypass the SPI flash cache entirely.
//
// Record layout (128 bytes, 4-byte aligned throughout):
//   [0–3]    magic: 0xAD 0xEE 0x75 0xEA  — written LAST so partial writes
//                                           leave storage_is_setup() == false
//   [4]      word_count
//   [5–7]    reserved 0x00
//   [8–23]   AES-CBC IV (16 bytes)
//   [24–103] AES256-CBC(AES_KEY, iv, master_key ∥ master_chain)  (80 bytes)
//   [104–127] padding 0x00

#define SEED_MAGIC_0  0xAD
#define SEED_MAGIC_1  0xEE
#define SEED_MAGIC_2  0x75
#define SEED_MAGIC_3  0xEA
#define RECORD_SIZE   128

static const uint8_t AES_KEY[32] = {
    0x4A, 0x3B, 0x7C, 0x91, 0xDE, 0x25, 0x6F, 0x08,
    0xB3, 0x5E, 0xA1, 0x74, 0x2D, 0xC8, 0x9F, 0x1B,
    0x62, 0x48, 0xE7, 0x3A, 0x0D, 0x95, 0x57, 0xF2,
    0x81, 0x4C, 0xB6, 0x29, 0xD3, 0x6E, 0xAA, 0x5C
};

// Cached at storage_init() — valid for the lifetime of the firmware run.
static const esp_partition_t* s_part = nullptr;

static const esp_partition_t* seed_part() { return s_part; }

// ── Init ───────────────────────────────────────────────────────────────────────

void storage_init() {
    // NVS for BLE stack (unchanged)
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }

    // Locate seed partition once — abort loudly if missing
    s_part = esp_partition_find_first(
                 ESP_PARTITION_TYPE_DATA,
                 (esp_partition_subtype_t)0x99,
                 "seed");

    if (s_part) {
        Serial.printf("[storage] seed part @ 0x%X  size 0x%X\n",
                      s_part->address, s_part->size);
    } else {
        Serial.println("[storage] ERROR: seed partition NOT FOUND");
    }
}

// ── Public API ────────────────────────────────────────────────────────────────

bool storage_is_setup() {
    if (!seed_part()) return false;
    uint8_t magic[4] = {0xFF, 0xFF, 0xFF, 0xFF};
    esp_partition_read_raw(seed_part(), 0, magic, sizeof(magic));
    bool ok = magic[0] == SEED_MAGIC_0 && magic[1] == SEED_MAGIC_1 &&
              magic[2] == SEED_MAGIC_2 && magic[3] == SEED_MAGIC_3;
    Serial.printf("[storage] is_setup: magic=%02X%02X%02X%02X → %s\n",
                  magic[0], magic[1], magic[2], magic[3], ok ? "YES" : "NO");
    return ok;
}

bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  uint8_t word_count) {
    if (!seed_part()) {
        Serial.println("[storage] save: no partition");
        return false;
    }

    // Encrypt
    uint8_t plain[64], cipher[80], civ[16];
    crypto_rand(civ, 16);
    memcpy(plain,      master_key,   32);
    memcpy(plain + 32, master_chain, 32);
    size_t clen = aes256_encrypt(AES_KEY, civ, plain, 64, cipher);
    memset(plain, 0, sizeof(plain));
    if (clen == 0) { Serial.println("[storage] save: encrypt failed"); return false; }

    // Build record — magic slot stays 0x00 until everything else is written
    uint8_t rec[RECORD_SIZE] = {0};
    rec[4] = word_count;
    memcpy(rec + 8,  civ,    16);
    memcpy(rec + 24, cipher, 80);

    // Erase sector
    esp_err_t e = esp_partition_erase_range(seed_part(), 0, seed_part()->size);
    if (e != ESP_OK) {
        Serial.printf("[storage] save: erase failed %d\n", e);
        return false;
    }

    // Write payload (bytes 4–127) first — magic comes last
    e = esp_partition_write_raw(seed_part(), 4, rec + 4, RECORD_SIZE - 4);
    if (e != ESP_OK) {
        Serial.printf("[storage] save: write payload failed %d\n", e);
        return false;
    }

    // Write magic (bytes 0–3) — marks record as valid
    uint8_t magic[4] = {SEED_MAGIC_0, SEED_MAGIC_1, SEED_MAGIC_2, SEED_MAGIC_3};
    e = esp_partition_write_raw(seed_part(), 0, magic, 4);
    if (e != ESP_OK) {
        Serial.printf("[storage] save: write magic failed %d\n", e);
        return false;
    }

    // Read-back verification — confirm data physically persists right now
    uint8_t vfy[4] = {0};
    esp_partition_read_raw(seed_part(), 0, vfy, sizeof(vfy));
    if (vfy[0] != SEED_MAGIC_0 || vfy[1] != SEED_MAGIC_1 ||
        vfy[2] != SEED_MAGIC_2 || vfy[3] != SEED_MAGIC_3) {
        Serial.println("[storage] save: READ-BACK FAILED — write did not persist");
        return false;
    }

    Serial.println("[storage] save: OK");
    memset(rec, 0, sizeof(rec));
    memset(civ, 0, sizeof(civ));
    return true;
}

bool storage_load(uint8_t master_key[32], uint8_t master_chain[32]) {
    if (!seed_part()) {
        Serial.println("[storage] load: no partition");
        return false;
    }

    uint8_t rec[RECORD_SIZE] = {0};
    esp_err_t e = esp_partition_read_raw(seed_part(), 0, rec, RECORD_SIZE);
    if (e != ESP_OK) {
        Serial.printf("[storage] load: read failed %d\n", e);
        return false;
    }

    Serial.printf("[storage] load: magic=%02X%02X%02X%02X\n",
                  rec[0], rec[1], rec[2], rec[3]);

    if (rec[0] != SEED_MAGIC_0 || rec[1] != SEED_MAGIC_1 ||
        rec[2] != SEED_MAGIC_2 || rec[3] != SEED_MAGIC_3) {
        Serial.println("[storage] load: bad magic");
        memset(rec, 0, sizeof(rec));
        return false;
    }

    uint8_t plain[80] = {0};
    size_t plen = aes256_decrypt(AES_KEY, rec + 8, rec + 24, 80, plain);
    memset(rec, 0, sizeof(rec));

    Serial.printf("[storage] load: plen=%u\n", plen);

    if (plen != 64) {
        memset(plain, 0, sizeof(plain));
        return false;
    }

    memcpy(master_key,   plain,      32);
    memcpy(master_chain, plain + 32, 32);
    memset(plain, 0, sizeof(plain));
    Serial.println("[storage] load: OK");
    return true;
}

uint8_t storage_word_count() {
    if (!seed_part()) return 12;
    uint8_t wc = 12;
    esp_partition_read_raw(seed_part(), 4, &wc, 1);
    return wc;
}

void storage_wipe() {
    if (!seed_part()) return;
    esp_partition_erase_range(seed_part(), 0, seed_part()->size);
    Serial.println("[storage] wiped");
}
