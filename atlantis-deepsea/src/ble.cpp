#include "ble.h"
#include <BleKeyboard.h>
#include <BLEDevice.h>
#include <BLESecurity.h>

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

    // Called once when pairing/authentication completes.
    void onAuthenticationComplete(esp_ble_auth_cmpl_t cmpl) override {
        s_pair_ok   = (cmpl.success == true);
        s_pair_done = true;
    }

    bool onConfirmPIN(uint32_t) override { return true; }
};

// ── Public API ────────────────────────────────────────────────────────────────
void ble_init() {
    // Begin starts BLEDevice::init() internally — must happen before we set
    // security so the BLE stack is already up.
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

void ble_type(const char* str) {
    if (bleKb.isConnected()) bleKb.print(str);
}

uint32_t ble_passkey_pending() { return s_passkey;   }
void     ble_passkey_clear()   { s_passkey = 0;      }

bool ble_pair_done_pending() { return s_pair_done;   }
bool ble_pair_done_success() { return s_pair_ok;     }
void ble_pair_done_clear()   { s_pair_done = false;  }
