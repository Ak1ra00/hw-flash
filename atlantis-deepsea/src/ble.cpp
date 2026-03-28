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

// Remote device address, captured on every successful authentication so we can
// force-disconnect when leaving the password page.
static esp_bd_addr_t s_remote_addr  = {0};
static volatile bool s_has_remote   = false;

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
        if (cmpl.success) {
            memcpy(s_remote_addr, cmpl.bd_addr, sizeof(esp_bd_addr_t));
            s_has_remote = true;
        }
        s_pair_ok   = (cmpl.success == true);
        s_pair_done = true;
    }

    bool onConfirmPIN(uint32_t) override { return true; }
};

// ── Public API ────────────────────────────────────────────────────────────────

void ble_init() {
    // begin() calls BLEDevice::init() and starts advertising automatically.
    // We stop advertising immediately — BLE is only active on the password page.
    bleKb.begin();
    BLEDevice::getAdvertising()->stop();

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

// Start advertising — call when entering the password display page.
void ble_enable() {
    BLEDevice::getAdvertising()->start();
}

// Stop advertising and drop any active connection — call when leaving the
// password display page so the host's on-screen keyboard is restored.
void ble_disable() {
    BLEDevice::getAdvertising()->stop();
    if (s_has_remote && bleKb.isConnected()) {
        esp_ble_gap_disconnect(s_remote_addr);
        s_has_remote = false;
    }
}

bool ble_connected() { return bleKb.isConnected(); }

// Type a string via BLE HID and release all keys so the last character
// does not repeat.
void ble_type(const char* str) {
    if (!bleKb.isConnected()) return;
    bleKb.print(str);
    delay(20);          // let the HID report flush
    bleKb.releaseAll(); // explicit key-up clears the "last char repeating" bug
}

uint32_t ble_passkey_pending() { return s_passkey;   }
void     ble_passkey_clear()   { s_passkey = 0;      }

bool ble_pair_done_pending() { return s_pair_done;   }
bool ble_pair_done_success() { return s_pair_ok;     }
void ble_pair_done_clear()   { s_pair_done = false;  }
