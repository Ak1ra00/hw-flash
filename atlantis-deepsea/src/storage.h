#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialize NVS storage
void storage_init();

// Returns true if device has been configured (seed + PIN stored)
bool storage_is_setup();

// Save the BIP32 master key + chain code, encrypted with the user's PIN.
// Also saves PIN verification hash.
// Wipes any existing data first.
bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  const char pin[7], uint8_t word_count);

// Verify PIN and load decrypted master key + chain code.
// Returns false if PIN is wrong or data is corrupt.
// Increments fail counter; locks out after PIN_MAX_ATTEMPTS.
bool storage_load(const char pin[7],
                  uint8_t master_key[32], uint8_t master_chain[32]);

// Number of remaining PIN attempts before lockout
int  storage_attempts_left();

// True if currently locked out (too many wrong PINs)
bool storage_is_locked_out();

// Word count stored during setup (12 or 24)
uint8_t storage_word_count();

// Erase all stored data (factory reset)
void storage_wipe();
