#pragma once
#include <stdint.h>
#include <stdbool.h>

// ── Initialization ───────────────────────────────────────────────────────────
void ui_init();

// ── Boot screen ──────────────────────────────────────────────────────────────
// Animated Atlantis DeepSea splash; blocks until animation completes (~2.5 s)
void ui_boot();

// ── Button helpers ───────────────────────────────────────────────────────────
// Call once per loop; returns true if BTN1 was short-pressed
bool btn1_short();
// Returns true if BTN1 was long-pressed (held >= LONG_PRESS_MS)
bool btn1_long();
// Short press BTN2
bool btn2_short();
// Long press BTN2
bool btn2_long();
// Both buttons pressed and released simultaneously
bool btns_both();
// Poll buttons (call each loop tick; updates internal state)
void btns_poll();

// ── PIN screen ───────────────────────────────────────────────────────────────
// Interactive PIN entry.  Returns true when user finishes entering 6 digits.
// pin_out must be 7 bytes (6 digits + null).
// Shows "Wrong PIN  N attempts left" if wrong = true.
bool ui_pin_entry(char pin_out[7], bool wrong, int attempts_left, bool locked_out, uint32_t lockout_remaining_ms);

// ── Setup: choose word count ─────────────────────────────────────────────────
// Returns 12 or 24
int  ui_choose_word_count();

// ── Seed entry ───────────────────────────────────────────────────────────────
// Enter one BIP39 word interactively.
// word_num = 1-based index (e.g. 1 of 12).
// total = 12 or 24.
// out must be >= 10 bytes.
void ui_enter_word(int word_num, int total, char out[10]);

// ── Confirm seed ─────────────────────────────────────────────────────────────
// Ask user to re-enter the word at position check_pos (1-based).
// Returns true if they enter the correct word.
bool ui_confirm_word(int check_pos, int total, const char* correct_word);

// ── Main menu ────────────────────────────────────────────────────────────────
enum MenuItem {
    MENU_GET_PASSWORD = 0,
    MENU_SETTINGS,
    MENU_ABOUT,
    MENU_LOCK,
    MENU_COUNT
};
MenuItem ui_main_menu();

// ── Password config ───────────────────────────────────────────────────────────
// Interactive: user picks index (0-9999) and length (10-85).
// Returns false if user cancels (long BTN1 on first field).
bool ui_password_config(uint32_t* index_out, uint8_t* len_out);

// ── Password display ──────────────────────────────────────────────────────────
// Show password string; auto-clears after PWD_SHOW_MS or button press.
// ble_connected: whether BLE keyboard is paired — shows "type" option if true.
// Returns true if user confirmed the "type via BLE" action.
bool ui_show_password(const char* pwd, uint32_t index, uint8_t len, bool ble_connected);

// ── Status / messages ────────────────────────────────────────────────────────
void ui_message(const char* title, const char* body, uint32_t ms);
void ui_confirm_wipe();  // Shows wipe warning; blocks for user confirmation
void ui_about();

// ── Checksum error ────────────────────────────────────────────────────────────
void ui_seed_invalid();
