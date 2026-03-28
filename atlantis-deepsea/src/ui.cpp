#include "ui.h"
#include "config.h"
#include "crypto.h"
#include "wordlist.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <math.h>

// ── Display instance ─────────────────────────────────────────────────────────
static TFT_eSPI tft = TFT_eSPI();

// ── Colour helpers ────────────────────────────────────────────────────────────
#define C_BG        0x0000
#define C_TITLE     tft.color565(0,   180, 255)  // Deep sky blue
#define C_ACCENT    tft.color565(0,   220, 190)  // Teal/aqua
#define C_TEXT      0xFFFF
#define C_DIM       tft.color565(80,  80,  80)
#define C_SEL       tft.color565(0,   255, 160)  // Aquamarine
#define C_PIN       tft.color565(255, 180, 0)    // Amber
#define C_WARN      tft.color565(255, 80,  0)
#define C_GOOD      tft.color565(0,   200, 80)
#define C_BOX       tft.color565(30,  30,  50)   // Dark box fill
#define C_BORDER    tft.color565(0,   100, 150)  // Box border

// ── Button state ──────────────────────────────────────────────────────────────
struct BtnState {
    int      pin;
    bool     raw_prev;
    bool     pressed;       // currently held
    uint32_t press_ts;
    bool     long_fired;
    bool     short_event;
    bool     long_event;
};

static BtnState b1 = {BTN1_PIN, true, false, 0, false, false, false};
static BtnState b2 = {BTN2_PIN, true, false, 0, false, false, false};

// Both-button state
static bool both_active = false;
static bool both_event  = false;

static void update_btn(BtnState& b) {
    bool raw = (digitalRead(b.pin) == LOW);  // active-low
    b.short_event = false;
    b.long_event  = false;

    if (raw && !b.raw_prev) {
        // Falling edge → press start
        b.pressed    = true;
        b.press_ts   = millis();
        b.long_fired = false;
    } else if (!raw && b.raw_prev && b.pressed) {
        // Rising edge → release
        uint32_t held = millis() - b.press_ts;
        if (!b.long_fired && held < LONG_PRESS_MS)
            b.short_event = true;
        b.pressed = false;
    } else if (raw && b.pressed && !b.long_fired) {
        if ((millis() - b.press_ts) >= LONG_PRESS_MS) {
            b.long_event  = true;
            b.long_fired  = true;
        }
    }
    b.raw_prev = raw;
}

void btns_poll() {
    update_btn(b1);
    update_btn(b2);

    both_event = false;

    // Enter both_active when both buttons are held simultaneously
    if (b1.pressed && b2.pressed && !both_active) {
        both_active = true;
    }

    // While both active: suppress all individual events
    if (both_active) {
        b1.short_event = false;
        b2.short_event = false;
        b1.long_event  = false;
        b2.long_event  = false;
    }

    // Fire both_event when both are released
    if (both_active && !b1.pressed && !b2.pressed) {
        both_event  = true;
        both_active = false;
    }
}

bool btn1_short() { return b1.short_event; }
bool btn1_long()  { return b1.long_event;  }
bool btn2_short() { return b2.short_event; }
bool btn2_long()  { return b2.long_event;  }
bool btns_both()  { return both_event;     }

// ── Init ──────────────────────────────────────────────────────────────────────
void ui_init() {
    pinMode(BTN1_PIN, INPUT_PULLUP);
    pinMode(BTN2_PIN, INPUT_PULLUP);
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, HIGH);

    tft.init();
    tft.setRotation(1);       // landscape 240×135
    tft.fillScreen(C_BG);
    tft.setTextDatum(MC_DATUM);
}

// ── Internal drawing helpers ──────────────────────────────────────────────────
static void draw_divider(int y, uint16_t col = 0) {
    if (col == 0) col = C_BORDER;
    tft.drawFastHLine(8, y, DISP_W - 16, col);
}

static void draw_footer(const char* left, const char* right) {
    tft.fillRect(0, DISP_H - 18, DISP_W, 18, C_BOX);
    draw_divider(DISP_H - 19);
    tft.setTextDatum(BL_DATUM);
    tft.setTextColor(C_DIM, C_BOX);
    tft.drawString(left,  6,         DISP_H - 3, 1);
    tft.setTextDatum(BR_DATUM);
    tft.drawString(right, DISP_W-6,  DISP_H - 3, 1);
    tft.setTextDatum(MC_DATUM);
}

static void draw_header(const char* title) {
    tft.fillRect(0, 0, DISP_W, 24, C_BOX);
    tft.setTextDatum(ML_DATUM);
    tft.setTextColor(C_TITLE, C_BOX);
    tft.drawString("ATLANTIS DEEPSEA", 8, 12, 1);
    tft.setTextDatum(MR_DATUM);
    tft.setTextColor(C_DIM, C_BOX);
    tft.drawString(title, DISP_W - 8, 12, 1);
    draw_divider(24);
    tft.setTextDatum(MC_DATUM);
}

// ── Boot screen ───────────────────────────────────────────────────────────────
void ui_boot() {
    tft.fillScreen(C_BG);

    // Animated wave lines (ocean theme)
    for (int y = 0; y < DISP_H; y += 14) {
        for (int x = 0; x < DISP_W; x += 4) {
            int wave = (int)(1.5f * sinf((float)(x + y) * 0.15f));
            tft.drawPixel(x, y + wave,     tft.color565(0, 20, 40));
            tft.drawPixel(x+1, y + wave+1, tft.color565(0, 15, 30));
        }
    }

    // Title — ATLANTIS
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TITLE, C_BG);
    tft.drawString("A T L A N T I S", DISP_W/2, 35, 4);

    // Subtitle — DEEPSEA
    tft.setTextColor(C_ACCENT, C_BG);
    tft.drawString("D E E P S E A", DISP_W/2, 63, 2);

    // Tagline
    tft.setTextColor(C_DIM, C_BG);
    tft.drawString("BIP85 Password Manager", DISP_W/2, 82, 1);

    // Separator
    draw_divider(92, C_BORDER);

    // Loading bar
    int bar_x = 30, bar_y = 100, bar_w = DISP_W - 60, bar_h = 8;
    tft.drawRect(bar_x, bar_y, bar_w, bar_h, C_BORDER);

    for (int i = 0; i <= bar_w - 2; i++) {
        tft.fillRect(bar_x + 1, bar_y + 1, i, bar_h - 2,
                     tft.color565(0, (uint8_t)(100 + i * 80 / bar_w), (uint8_t)(180 + i * 60 / bar_w)));
        if (i % 6 == 0) delay(185);   // ~5.5 s for the bar sweep
    }

    // Version tag
    tft.setTextColor(C_DIM, C_BG);
    tft.drawString("v1.0  |  ak1ra00", DISP_W/2, 122, 1);

    delay(1200);   // hold the finished screen — total splash ~7 s
    tft.fillScreen(C_BG);
}

// ── PIN entry ─────────────────────────────────────────────────────────────────
bool ui_pin_entry(char pin_out[7], bool wrong, int attempts_left, bool locked_out, uint32_t lockout_ms) {
    const int N = PIN_LENGTH;
    int  digits[N] = {0};
    int  pos = 0;            // current digit position (0-5)
    bool done = false;

    auto draw_pin_screen = [&]() {
        tft.fillScreen(C_BG);

        // Header
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(C_TITLE, C_BG);
        tft.drawString("ATLANTIS  DEEPSEA", DISP_W/2, 14, 1);
        draw_divider(24);

        if (locked_out) {
            tft.setTextColor(C_WARN, C_BG);
            tft.drawString("LOCKED OUT", DISP_W/2, 52, 2);
            char buf[40];
            snprintf(buf, sizeof(buf), "Try again in %lus", (unsigned long)(lockout_ms/1000));
            tft.setTextColor(C_DIM, C_BG);
            tft.drawString(buf, DISP_W/2, 75, 1);
            return;
        }

        // Label
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawString(wrong ? "Wrong PIN" : "Enter PIN", DISP_W/2, 40, 2);
        if (wrong) {
            char buf[24];
            snprintf(buf, sizeof(buf), "%d attempts left", attempts_left);
            tft.setTextColor(C_WARN, C_BG);
            tft.drawString(buf, DISP_W/2, 57, 1);
        }

        // 6 digit boxes
        int box_w = 26, box_h = 30, gap = 6;
        int total_w = N * box_w + (N - 1) * gap;
        int start_x = (DISP_W - total_w) / 2;
        int box_y = wrong ? 67 : 58;

        for (int i = 0; i < N; i++) {
            int bx = start_x + i * (box_w + gap);
            uint16_t border_col = (i == pos) ? C_PIN : C_BORDER;
            uint16_t fill_col   = (i == pos) ? tft.color565(40, 25, 0) : C_BOX;
            tft.fillRoundRect(bx, box_y, box_w, box_h, 4, fill_col);
            tft.drawRoundRect(bx, box_y, box_w, box_h, 4, border_col);

            tft.setTextDatum(MC_DATUM);
            if (i < pos) {
                // confirmed digits: show dot
                tft.setTextColor(C_ACCENT, fill_col);
                tft.drawString("*", bx + box_w/2, box_y + box_h/2, 2);
            } else if (i == pos) {
                // current digit: show number
                char d[2] = {(char)('0' + digits[i]), 0};
                tft.setTextColor(C_PIN, fill_col);
                tft.drawString(d, bx + box_w/2, box_y + box_h/2, 2);
            } else {
                // future digits: empty
                tft.setTextColor(C_DIM, fill_col);
                tft.drawString("-", bx + box_w/2, box_y + box_h/2, 2);
            }
        }

        draw_footer("1:up  2:down", "BOTH: confirm digit");
    };

    draw_pin_screen();

    uint32_t last_activity = millis();

    while (!done) {
        btns_poll();

        if (locked_out) {
            uint32_t elapsed = millis() - last_activity;
            if (elapsed > 1000) {
                last_activity = millis();
                if (lockout_ms > 1000) lockout_ms -= 1000; else lockout_ms = 0;
                draw_pin_screen();
                if (lockout_ms == 0) { locked_out = false; draw_pin_screen(); }
            }
            delay(10);
            continue;
        }

        // BTN1 short → digit up
        if (btn1_short()) {
            digits[pos] = (digits[pos] + 1) % 10;
            draw_pin_screen();
        }

        // BTN2 short → digit down
        if (btn2_short()) {
            digits[pos] = (digits[pos] + 9) % 10;   // +9 mod 10 = -1
            draw_pin_screen();
        }

        // BOTH → confirm digit, advance position
        if (btns_both()) {
            pos++;
            if (pos >= N) {
                for (int i = 0; i < N; i++) pin_out[i] = '0' + digits[i];
                pin_out[N] = '\0';
                done = true;
            } else {
                draw_pin_screen();
            }
        }

        // BTN1 long → back one digit
        if (btn1_long() && pos > 0) {
            pos--;
            digits[pos] = 0;
            draw_pin_screen();
        }

        delay(8);
    }

    return true;
}

// ── Choose word count ─────────────────────────────────────────────────────────
int ui_choose_word_count() {
    int choice = 0;  // 0=12, 1=24

    auto draw = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Setup");

        tft.setTextColor(C_TEXT, C_BG);
        tft.setTextDatum(ML_DATUM);
        tft.drawString("Seed phrase length:", 14, 45, 2);

        const char* opts[2] = {"12 words  (BIP39 standard)", "24 words  (maximum security)"};
        for (int i = 0; i < 2; i++) {
            int oy = 72 + i * 22;
            if (i == choice) {
                tft.fillRoundRect(10, oy - 8, DISP_W - 20, 18, 3, tft.color565(0, 40, 60));
                tft.setTextColor(C_SEL, tft.color565(0, 40, 60));
                tft.drawString("> ", 14, oy, 1);
            } else {
                tft.setTextColor(C_DIM, C_BG);
                tft.drawString("  ", 14, oy, 1);
            }
            tft.drawString(opts[i], 26, oy, 1);
        }
        draw_footer("1/2: toggle", "BOTH: confirm");
    };

    draw();
    while (true) {
        btns_poll();
        if (btn1_short() || btn2_short()) { choice = 1 - choice; draw(); }
        if (btns_both()) return (choice == 0) ? 12 : 24;
        delay(8);
    }
}

// ── Seed word entry ────────────────────────────────────────────────────────────
void ui_enter_word(int word_num, int total, char out[10]) {
    char prefix[9] = {0};
    int  prefix_len = 0;
    int  match_indices[8];
    int  match_count  = 0;
    int  sel = 0;       // selected match index in match_indices

    // Letter cycling: a-z then backspace marker
    const char LETTERS[] = "abcdefghijklmnopqrstuvwxyz";
    int cur_letter = 0; // 0-25 = a-z, 26 = backspace

    enum Phase { PHASE_LETTER, PHASE_PICK } phase = PHASE_LETTER;

    auto update_matches = [&]() {
        match_count = 0;
        if (prefix_len > 0)
            match_count = bip39_prefix_match(prefix, match_indices, 8);
        sel = 0;
    };

    auto draw = [&]() {
        tft.fillScreen(C_BG);

        // Header
        char hdr[24];
        snprintf(hdr, sizeof(hdr), "Word %d / %d", word_num, total);
        draw_header(hdr);

        // Input box
        tft.drawRoundRect(8, 28, DISP_W - 16, 20, 3, C_BORDER);
        tft.fillRoundRect(9, 29, DISP_W - 18, 18, 3, C_BOX);

        // Show typed prefix + cursor letter
        char display[12];
        if (phase == PHASE_LETTER) {
            char cl = (cur_letter < 26) ? LETTERS[cur_letter] : '<';
            snprintf(display, sizeof(display), "%s[%c]", prefix, cl);
        } else {
            snprintf(display, sizeof(display), "%s", prefix);
        }
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(C_TEXT, C_BOX);
        tft.drawString(display, 14, 38, 2);

        // Match count badge
        if (match_count > 0) {
            char badge[12];
            snprintf(badge, sizeof(badge), "%d match%s", match_count, match_count == 1 ? "" : "es");
            tft.setTextDatum(MR_DATUM);
            tft.setTextColor(C_DIM, C_BG);
            tft.drawString(badge, DISP_W - 10, 38, 1);
        }

        // Word list
        int max_show = (phase == PHASE_PICK) ? 4 : 3;
        for (int i = 0; i < max_show && i < match_count; i++) {
            int wy = 56 + i * 17;
            bool is_sel = (phase == PHASE_PICK && i == sel);
            if (is_sel) {
                tft.fillRect(8, wy - 6, DISP_W - 16, 16, tft.color565(0, 50, 70));
            }
            tft.setTextDatum(ML_DATUM);
            tft.setTextColor(is_sel ? C_SEL : C_DIM, is_sel ? tft.color565(0, 50, 70) : C_BG);
            if (is_sel) tft.drawString("> ", 10, wy, 1);
            tft.drawString(BIP39_WORDLIST[match_indices[i]], 24, wy, 1);

            // Auto-complete indicator
            if (match_count == 1 && phase == PHASE_LETTER) {
                tft.setTextColor(C_GOOD, C_BG);
                tft.setTextDatum(MR_DATUM);
                tft.drawString("BOTH:accept", DISP_W - 10, wy, 1);
            }
        }

        // Footer
        if (phase == PHASE_LETTER) {
            if (match_count == 1)
                draw_footer("1:next  2:prev", "BOTH: accept word");
            else
                draw_footer("1:next  2:prev", "BOTH: add letter");
        } else {
            draw_footer("1:next  2:prev", "BOTH: pick word");
        }
    };

    // Helper: show inline hint when the auto-complete indicator renders
    // (the draw lambda already handles it via match_count==1 in PHASE_LETTER)

    update_matches();
    draw();

    while (true) {
        btns_poll();

        if (phase == PHASE_LETTER) {
            // BTN1 → next letter (forward)
            if (btn1_short()) {
                cur_letter = (cur_letter + 1) % 27;  // 0-25=letters, 26=backspace
                draw();
            }

            // BTN2 → prev letter (backward)
            if (btn2_short()) {
                cur_letter = (cur_letter - 1 + 27) % 27;
                draw();
            }

            // BTN1 long → quick backspace
            if (btn1_long()) {
                if (prefix_len > 0) {
                    prefix[--prefix_len] = '\0';
                    cur_letter = 0;
                    update_matches();
                    draw();
                }
            }

            // BTN2 long → force switch to pick mode
            if (btn2_long() && match_count > 0) {
                phase = PHASE_PICK;
                draw();
            }

            // BOTH → confirm: add letter, backspace, or accept sole match
            if (btns_both()) {
                if (match_count == 1) {
                    // Accept the only match directly
                    strncpy(out, BIP39_WORDLIST[match_indices[0]], 9);
                    out[9] = '\0';
                    return;
                } else if (cur_letter == 26) {
                    // Backspace position confirmed
                    if (prefix_len > 0) {
                        prefix[--prefix_len] = '\0';
                        cur_letter = 0;
                        update_matches();
                        draw();
                    }
                } else if (prefix_len < 8) {
                    // Add selected letter
                    prefix[prefix_len++] = LETTERS[cur_letter];
                    prefix[prefix_len]   = '\0';
                    cur_letter = 0;
                    update_matches();
                    // Auto-switch to pick mode if ≤4 matches
                    if (match_count > 0 && match_count <= 4) phase = PHASE_PICK;
                    draw();
                }
            }
        } else {
            // PHASE_PICK
            // BTN1 → next match (forward)
            if (btn1_short()) {
                sel = (sel + 1) % match_count;
                draw();
            }

            // BTN2 → prev match (backward)
            if (btn2_short()) {
                sel = (sel - 1 + match_count) % match_count;
                draw();
            }

            // BTN1 long → back to letter phase
            if (btn1_long()) {
                phase = PHASE_LETTER;
                cur_letter = 0;
                draw();
            }

            // BOTH → select highlighted word
            if (btns_both()) {
                strncpy(out, BIP39_WORDLIST[match_indices[sel]], 9);
                out[9] = '\0';
                return;
            }
        }

        delay(8);
    }
}

// ── Confirm word ──────────────────────────────────────────────────────────────
bool ui_confirm_word(int check_pos, int total, const char* correct_word) {
    char entered[10] = {0};

    tft.fillScreen(C_BG);
    char hdr[32];
    snprintf(hdr, sizeof(hdr), "Verify Word %d", check_pos);
    draw_header(hdr);

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TEXT, C_BG);
    tft.drawString("Re-enter this word to", DISP_W/2, 55, 1);
    tft.drawString("confirm your backup:", DISP_W/2, 68, 1);

    delay(1200);

    ui_enter_word(check_pos, total, entered);
    return (strcmp(entered, correct_word) == 0);
}

// ── Main menu ─────────────────────────────────────────────────────────────────
MenuItem ui_main_menu() {
    int sel = 0;
    const char* labels[MENU_COUNT] = {
        "Get Password",
        "Settings",
        "About",
        "Lock Device"
    };

    auto draw = [&]() {
        tft.fillScreen(C_BG);

        // Title bar
        tft.fillRect(0, 0, DISP_W, 20, C_BOX);
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(C_TITLE, C_BOX);
        tft.drawString("ATLANTIS  DEEPSEA", 8, 10, 1);
        tft.setTextDatum(MR_DATUM);
        tft.setTextColor(C_DIM, C_BOX);
        tft.drawString("LOCKED", DISP_W - 8, 10, 1);
        draw_divider(20);

        for (int i = 0; i < MENU_COUNT; i++) {
            int oy = 30 + i * 22;
            if (i == sel) {
                tft.fillRoundRect(8, oy - 7, DISP_W - 16, 18, 3, tft.color565(0, 40, 60));
                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(C_SEL, tft.color565(0, 40, 60));
                tft.drawString("> ", 12, oy, 2);
                tft.drawString(labels[i], 30, oy, 2);
            } else {
                tft.setTextDatum(ML_DATUM);
                tft.setTextColor(C_DIM, C_BG);
                tft.drawString(labels[i], 30, oy, 2);
            }
        }

        draw_footer("1:down  2:up", "BOTH: select");
    };

    draw();
    while (true) {
        btns_poll();
        if (btn1_short()) { sel = (sel + 1) % MENU_COUNT; draw(); }
        if (btn2_short()) { sel = (sel - 1 + MENU_COUNT) % MENU_COUNT; draw(); }
        if (btns_both())   return (MenuItem)sel;
        delay(8);
    }
}

// ── Password config ───────────────────────────────────────────────────────────
bool ui_password_config(uint32_t* index_out) {
    uint32_t idx = 0;

    auto draw = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Get Password");

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("Password Index  (0-9999)", DISP_W/2, 46, 1);

        // Big index box
        tft.fillRoundRect(40, 56, DISP_W - 80, 32, 5, tft.color565(0, 40, 60));
        tft.drawRoundRect(40, 56, DISP_W - 80, 32, 5, C_PIN);
        char buf[8];
        snprintf(buf, sizeof(buf), "%lu", idx);
        tft.setTextColor(C_PIN, tft.color565(0, 40, 60));
        tft.drawString(buf, DISP_W/2, 72, 4);

        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("hold 1:+10  hold 2:cancel", DISP_W/2, 103, 1);

        draw_footer("1:+1  2:-1", "BOTH: generate");
    };

    draw();

    while (true) {
        btns_poll();

        if (btn1_short()) { idx = (idx + 1)              % (PWD_IDX_MAX + 1); draw(); }
        if (btn2_short()) { idx = (idx + PWD_IDX_MAX)    % (PWD_IDX_MAX + 1); draw(); }  // -1

        if (btn1_long())  { idx = (idx + 10)              % (PWD_IDX_MAX + 1); draw(); }
        if (btn2_long())  { return false; }   // cancel — go back to menu

        if (btns_both()) {
            *index_out = idx;
            return true;
        }

        delay(8);
    }
}

// ── Password display ──────────────────────────────────────────────────────────
bool ui_show_password(const char* pwd, uint32_t index, uint8_t len, bool ble_connected) {

    auto draw_pwd_screen = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Password");

        char meta[32];
        snprintf(meta, sizeof(meta), "idx:%lu  len:%d", index, len);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString(meta, DISP_W/2, 33, 1);
        draw_divider(40);

        tft.setTextColor(C_ACCENT, C_BG);
        if (len <= 22) {
            tft.drawString(pwd, DISP_W/2, 72, 2);
        } else {
            int half = len / 2;
            char line1[44], line2[44];
            strncpy(line1, pwd,        half); line1[half] = '\0';
            strncpy(line2, pwd + half, len - half); line2[len - half] = '\0';
            tft.drawString(line1, DISP_W/2, 60, 1);
            tft.drawString(line2, DISP_W/2, 74, 1);
        }

        if (ble_connected)
            draw_footer("BTN1: type via BLE", "BTN2: clear");
        else
            draw_footer("pair BLE to type", "BTN2: clear");
    };

    draw_pwd_screen();

    uint32_t start   = millis();
    uint32_t timeout = PWD_SHOW_MS;

    while (true) {
        btns_poll();

        // BTN2 → clear immediately
        if (btn2_short() || btn2_long()) break;

        // BTN1 → type via BLE (or pair hint)
        if (btn1_short()) {
            if (ble_connected) {
                // "Get ready" confirmation popup
                tft.fillRoundRect(14, 27, DISP_W - 28, 82, 8, C_BOX);
                tft.drawRoundRect(14, 27, DISP_W - 28, 82, 8, tft.color565(0, 150, 200));
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(C_ACCENT, C_BOX);
                tft.drawString("READY TO TYPE?", DISP_W/2, 43, 1);
                draw_divider(52, tft.color565(0, 80, 110));
                tft.setTextColor(C_TEXT, C_BOX);
                tft.drawString("1. Click in password field", DISP_W/2, 65, 1);
                tft.drawString("2. Press confirm to send", DISP_W/2, 79, 1);
                tft.setTextColor(C_DIM, C_BOX);
                tft.drawString("BTN1: cancel   BTN2: send ->", DISP_W/2, 97, 1);

                bool confirmed = false;
                while (true) {
                    btns_poll();
                    if (btn2_short()) { confirmed = true; break; }
                    if (btn1_short() || btn1_long()) { break; }
                    delay(8);
                }
                if (confirmed) return true;
                draw_pwd_screen();   // user cancelled → back to password
            } else {
                // BLE not paired — show pairing hint overlay
                tft.fillRoundRect(14, 38, DISP_W - 28, 58, 8, C_BOX);
                tft.drawRoundRect(14, 38, DISP_W - 28, 58, 8, C_WARN);
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(C_WARN, C_BOX);
                tft.drawString("BLE not paired", DISP_W/2, 53, 1);
                tft.setTextColor(C_DIM, C_BOX);
                tft.drawString("Search Bluetooth for:", DISP_W/2, 68, 1);
                tft.setTextColor(C_ACCENT, C_BOX);
                tft.drawString("\"Atlantis DeepSea\"", DISP_W/2, 81, 1);
                delay(2800);
                draw_pwd_screen();
            }
        }

        uint32_t elapsed = millis() - start;
        if (elapsed >= timeout) break;

        uint32_t remain_s = (timeout - elapsed) / 1000 + 1;

        int bar_w  = DISP_W - 20;
        int filled = (int)(bar_w * (timeout - elapsed) / timeout);
        tft.fillRect(10, 98, bar_w, 6, C_BOX);
        tft.fillRect(10, 98, filled, 6,
                     tft.color565(0, (uint8_t)(150 * filled / bar_w),
                                     (uint8_t)(200 * filled / bar_w)));

        char cd[16];
        snprintf(cd, sizeof(cd), "Clears in %lus", remain_s);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(C_DIM, C_BG);
        tft.fillRect(0, 108, DISP_W, 12, C_BG);
        tft.drawString(cd, DISP_W/2, 114, 1);

        delay(250);
    }

    tft.fillScreen(C_BG);
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Cleared.", DISP_W/2, DISP_H/2, 2);
    delay(800);
    return false;
}

// ── Generic message ───────────────────────────────────────────────────────────
void ui_message(const char* title, const char* body, uint32_t ms) {
    tft.fillScreen(C_BG);
    draw_header(title);
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_TEXT, C_BG);

    // Split body at '\n' and draw up to 3 lines
    char buf[128];
    strncpy(buf, body, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';

    const char* lines[3] = {nullptr, nullptr, nullptr};
    int n = 0;
    char* tok = strtok(buf, "\n");
    while (tok && n < 3) { lines[n++] = tok; tok = strtok(nullptr, "\n"); }

    int start_y = (n == 1) ? DISP_H / 2 : (DISP_H / 2) - (n - 1) * 12;
    for (int i = 0; i < n; i++)
        tft.drawString(lines[i], DISP_W / 2, start_y + i * 22, 2);

    if (ms > 0) {
        delay(ms);
    } else {
        draw_footer("", "BOTH / BTN2: OK");
        while (true) {
            btns_poll();
            if (btn2_short() || btns_both()) break;
            delay(8);
        }
    }
}

// ── Seed invalid ──────────────────────────────────────────────────────────────
void ui_seed_invalid() {
    tft.fillScreen(C_BG);
    draw_header("Error");
    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_WARN, C_BG);
    tft.drawString("Invalid seed phrase!", DISP_W/2, 50, 2);
    tft.setTextColor(C_DIM, C_BG);
    tft.drawString("Checksum mismatch.", DISP_W/2, 72, 1);
    tft.drawString("Please try again.", DISP_W/2, 84, 1);
    draw_footer("", "BOTH / BTN2: retry");
    while (true) { btns_poll(); if (btn2_short() || btns_both()) break; delay(8); }
}

// ── About ─────────────────────────────────────────────────────────────────────
void ui_about() {
    tft.fillScreen(C_BG);
    draw_header("About");

    tft.setTextDatum(ML_DATUM);
    int y = 36;
    struct { const char* text; uint16_t col; } lines[] = {
        {"Atlantis DeepSea  v1.0",   C_ACCENT},
        {"BIP85 Password Manager",   C_ACCENT},
        {"",                         C_BG},
        {"Device: LilyGO T-Display", C_DIM},
        {"Chip:   ESP32 240 MHz",    C_DIM},
        {"Memory: 4 MB Flash",       C_DIM},
        {"",                         C_BG},
        {"github: ak1ra00/hw-flash", C_TEXT},
    };
    for (auto& l : lines) {
        tft.setTextColor(l.col, C_BG);
        tft.drawString(l.text, 12, y, 1);
        y += 11;
    }

    draw_footer("", "BOTH / BTN2: back");
    while (true) { btns_poll(); if (btn2_short() || btns_both()) break; delay(8); }
}

// ── Wipe confirm ──────────────────────────────────────────────────────────────
void ui_confirm_wipe() {
    // Shown from Settings — not yet wired; placeholder
    ui_message("Factory Reset",
               "Hold BTN1 5s to wipe", 0);
}
