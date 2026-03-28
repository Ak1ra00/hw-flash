#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── BLE keyboard with secure passkey pairing + bonding ───────────────────────
// BLE advertises from boot and stays on permanently.
// Call ble_init() once in setup(), before storage_init().

void     ble_init();
bool     ble_connected();
void     ble_type(const char* str);  // prints + releaseAll to prevent key repeat

// Remove all stored bonded devices (user must re-pair with passkey).
void     ble_forget_bonds();

// Passkey notification: set by BLE security callback when the host requests
// passkey-based pairing.  Check with ble_passkey_pending(); call
// ble_passkey_clear() after displaying it.
uint32_t ble_passkey_pending();
void     ble_passkey_clear();

// Pairing completion: fires once when authentication completes (success or fail).
// Cleared by calling ble_pair_done_clear().
bool     ble_pair_done_pending();
bool     ble_pair_done_success();
void     ble_pair_done_clear();
