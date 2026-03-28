#pragma once

// ── Hardware ────────────────────────────────────────────────────────────────
#define BTN1_PIN        35      // Top button (increment)
#define BTN2_PIN         0      // Bottom button / BOOT (confirm)
#define TFT_BL_PIN       4      // Backlight

// Display orientation: landscape 240×135
#define DISP_W          240
#define DISP_H          135

// ── Button timing ────────────────────────────────────────────────────────────
#define DEBOUNCE_MS      50
#define LONG_PRESS_MS   700

// ── Security ─────────────────────────────────────────────────────────────────
#define PIN_LENGTH        6
#define PIN_MAX_ATTEMPTS  5
#define PIN_LOCKOUT_MS   30000UL   // 30 s lockout after 5 wrong tries
#define AUTO_LOCK_MS    120000UL   // 2 min idle → lock

// ── Password derivation ───────────────────────────────────────────────────────
#define PWD_LEN_FIXED    20        // Fixed output length — index is the only user input
#define PWD_LEN_MIN      10        // kept for BIP85 API compatibility
#define PWD_LEN_MAX      85
#define PWD_IDX_MAX    9999
#define PWD_SHOW_MS    30000UL   // auto-clear after 30 s

// ── Storage ───────────────────────────────────────────────────────────────────
#define NVS_NAMESPACE   "atlantis"

// ── Theme colours (RGB565 constants, computed at compile time) ────────────────
#define CLR_BG          0x0000   // Black
#define CLR_TITLE       0x055F   // Deep sky blue  #0057FF approx → 0x055F
#define CLR_ACCENT      0x07F9   // Teal           #00CED1 → 0x0679
#define CLR_TEXT        0xFFFF   // White
#define CLR_DIM         0x4208   // Dark grey
#define CLR_SELECT      0x07E8   // Aquamarine     #00FFB0 approx
#define CLR_PIN         0xFD20   // Amber/gold
#define CLR_WARN        0xF820   // Orange-red
#define CLR_GOOD        0x07E0   // Green
