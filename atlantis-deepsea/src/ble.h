#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── BLE keyboard with secure passkey pairing + bonding ───────────────────────
// Call ble_init() once in setup(), before storage_init().
// BLE does NOT advertise at boot — call ble_enable() / ble_disable() to
// control advertising from the password display page only.

void     ble_init();

// Start / stop BLE advertising and active connection.
// ble_enable()  — enter password page
// ble_disable() — leave password page (disconnects host so soft keyboard returns)
void     ble_enable();
void     ble_disable();

bool     ble_connected();
void     ble_type(const char* str);  // prints + releaseAll to prevent key repeat

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
