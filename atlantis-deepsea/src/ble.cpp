#include "ble.h"
#include <Arduino.h>
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLESecurity.h>
#include <esp_gap_ble_api.h>

// BLE keyboard instance — "Atlantis DeepSea" is the advertised device name.
static BleKeyboard bleKb("Atlantis DeepSea", "Atlantis", 100);

// ── Thread-safe state shared with BLE task ────────────────────────────────────
static volatile uint32_t s_passkey   = 0;
static volatile bool     s_pair_done = false;
static volatile bool     s_pair_ok   = false;

// ── Security callbacks (run on BLE task, not the Arduino loop) ────────────────
class AtlantisSecurity : public BLESecurityCallbacks {
    // Device is display-only (ESP_IO_CAP_OUT): we generate the passkey.
    uint32_t onPassKeyRequest() override { return 0; }

    // Called when the BLE stack generates a passkey for the host to confirm.
    void onPassKeyNotify(uint32_t pass_key) override {
        s_passkey = pass_key;
    }

    bool onSecurityRequest() override { return true; }

    // Called once when pairing/authentication completes — also fires on
    // bonded re-connections that re-establish encryption.
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        s_pair_ok   = (cmpl.success == true);
        s_pair_done = true;
    }

    bool onConfirmPIN(uint32_t) override { return true; }
};

// ── Public API ────────────────────────────────────────────────────────────────

void ble_init() {
    // begin() calls BLEDevice::init() and starts advertising automatically.
    // BLE stays on and advertising at all times — no enable/disable per page.
    bleKb.begin();

    // Secure passkey display with MITM protection and persistent bonding.
    // Bonding info is stored in NVS by the BLE stack and loaded automatically
    // on reconnect — the user will not be asked for the code again.
    BLEDevice::setEncryptionLevel(ESP_BLE_SEC_ENCRYPT_MITM);
    BLEDevice::setSecurityCallbacks(new AtlantisSecurity());

    BLESecurity* sec = new BLESecurity();
    sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
    sec->setCapability(ESP_IO_CAP_OUT);   // display-only: device shows passkey
    sec->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
    sec->setRespEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
}

bool ble_connected() { return bleKb.isConnected(); }

// Type a string via BLE HID.
// Each character is sent individually with a small inter-key delay so the
// HID reports have time to flush through the BLE stack before the next one
// is queued.  A final releaseAll() with extra hold time guarantees the
// host sees a clean key-up for the last character.
void ble_type(const char* str) {
    if (!bleKb.isConnected()) return;
    bleKb.releaseAll();   // clear any lingering state first
    delay(20);
    for (const char* p = str; *p; p++) {
        bleKb.write((uint8_t)*p);
        delay(10);        // one BLE connection event at typical 7.5 ms interval
    }
    delay(80);            // let the last key-up report reach the host
    bleKb.releaseAll();   // explicit all-keys-up
    delay(20);
    bleKb.releaseAll();   // second send for safety
}

// Erase all stored BLE bonding info.  The next connection will require
// re-pairing with a passkey.
void ble_forget_bonds() {
    int num = esp_ble_gap_get_bond_device_num();
    if (num <= 0) return;
    esp_ble_bond_dev_t* list = new esp_ble_bond_dev_t[num];
    esp_ble_gap_get_bond_device_list(&num, list);
    for (int i = 0; i < num; i++)
        esp_ble_gap_remove_bond_device(list[i].bd_addr);
    delete[] list;
}

int ble_bond_count() {
    return esp_ble_gap_get_bond_device_num();
}

uint32_t ble_passkey_pending() { return s_passkey;   }
void     ble_passkey_clear()   { s_passkey = 0;      }

bool ble_pair_done_pending() { return s_pair_done;   }
bool ble_pair_done_success() { return s_pair_ok;     }
void ble_pair_done_clear()   { s_pair_done = false;  }
