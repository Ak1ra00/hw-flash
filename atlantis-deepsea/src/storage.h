#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialize NVS storage
void storage_init();

// Returns true if device has been configured (seed stored)
bool storage_is_setup();

// Save the BIP32 master key + chain code, encrypted with a hardware-derived key.
// Wipes any existing data first.
bool storage_save(const uint8_t master_key[32], const uint8_t master_chain[32],
                  uint8_t word_count);

// Load decrypted master key + chain code.
// Returns false if data is missing or corrupt.
bool storage_load(uint8_t master_key[32], uint8_t master_chain[32]);

// Word count stored during setup (12 or 24)
uint8_t storage_word_count();

// Erase all stored data (factory reset)
void storage_wipe();
