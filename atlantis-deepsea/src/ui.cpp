#include "ui.h"
#include "ble.h"
#include "config.h"
#include "crypto.h"
#include "wordlist.h"
#include <TFT_eSPI.h>
#include <Arduino.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <esp_gap_ble_api.h>

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
    tft.drawString(FW_VERSION "  |  ak1ra00", DISP_W/2, 122, 1);

    delay(1200);   // hold the finished screen — total splash ~7 s
    tft.fillScreen(C_BG);
}

// ── Emoji combo authentication ────────────────────────────────────────────────
// 10 symbols: each is a printable ASCII char drawn large in a unique colour.
// The secret combo is 3 symbols (10^3 = 1000 combos), wiped after 5 failures.

static const char  EMOJI_SYM[10]  = {'~','@','*','X','#','+','^','$','%','!'};
static const char* EMOJI_NAME[10] = {
    "WAVE","VORTEX","STAR","CROSS","GRID",
    "PLUS","BOLT","COIN","BUBBL","FLAME"
};

static uint16_t emoji_fg(int id) {
    switch (id) {
        case 0: return tft.color565(  0, 180, 255);  // blue
        case 1: return tft.color565(  0, 220, 190);  // teal
        case 2: return tft.color565(255, 220,   0);  // yellow
        case 3: return tft.color565(  0, 255, 220);  // cyan
        case 4: return tft.color565(255, 120,   0);  // orange
        case 5: return tft.color565(255, 200,  50);  // gold
        case 6: return tft.color565(100, 255,   0);  // lime
        case 7: return tft.color565(255,  50, 200);  // magenta
        case 8: return tft.color565(255, 150, 200);  // pink
        default:return tft.color565(255,  60,  60);  // red
    }
}

bool ui_pin_entry(char pin_out[7], bool wrong, int attempts_left,
                  bool /*locked_out*/, uint32_t /*lockout_ms*/) {
    const int N     = 3;    // 3-symbol combo
    const int BOX_W = 62, BOX_H = 52, GAP = 8;
    const int BOX_Y = 26;
    const int START_X = (DISP_W - (N * BOX_W + (N - 1) * GAP)) / 2;

    int combo[N]  = {0, 0, 0};
    int active    = 0;

    // x-centre of slot i
    auto cx = [&](int i) { return START_X + i * (BOX_W + GAP) + BOX_W / 2; };
    auto bx = [&](int i) { return START_X + i * (BOX_W + GAP); };

    auto draw_slot = [&](int i) {
        int eid = combo[i];
        bool is_active = (i == active);
        bool is_locked = (i < active);

        uint16_t bg = is_active ? tft.color565(0, 35, 55)
                    : is_locked ? tft.color565(0, 28,  0)
                    :             C_BOX;
        uint16_t border = is_active ? C_PIN
                        : is_locked ? C_GOOD
                        :             C_DIM;

        tft.fillRoundRect(bx(i), BOX_Y, BOX_W, BOX_H, 6, bg);
        tft.drawRoundRect(bx(i), BOX_Y, BOX_W, BOX_H, 6, border);
        if (is_active)  // double border on active slot
            tft.drawRoundRect(bx(i)+1, BOX_Y+1, BOX_W-2, BOX_H-2, 5, border);

        char sym[2] = {EMOJI_SYM[eid], 0};
        tft.setTextDatum(MC_DATUM);
        uint16_t fg = (is_active || is_locked) ? emoji_fg(eid) : C_DIM;
        tft.setTextColor(fg, bg);
        tft.drawString(sym, cx(i), BOX_Y + BOX_H / 2, 4);

        // label below each slot
        tft.setTextColor(is_active ? C_ACCENT : (is_locked ? C_GOOD : C_DIM), C_BG);
        tft.drawString(EMOJI_NAME[eid], cx(i), BOX_Y + BOX_H + 8, 1);
    };

    auto draw = [&]() {
        tft.fillScreen(C_BG);

        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(C_TITLE, C_BG);
        tft.drawString("ATLANTIS  DEEPSEA", DISP_W/2, 12, 1);
        draw_divider(22);

        for (int i = 0; i < N; i++) draw_slot(i);

        // Wrong combo notice
        if (wrong) {
            char buf[36];
            snprintf(buf, sizeof(buf), "Wrong combo!  %d attempt%s left",
                     attempts_left, attempts_left == 1 ? "" : "s");
            tft.setTextColor(C_WARN, C_BG);
            tft.drawString(buf, DISP_W/2, 104, 1);
        }

        draw_footer("1:next  2:prev", "BOTH: confirm slot");
    };

    draw();

    while (true) {
        btns_poll();

        if (btn1_short()) { combo[active] = (combo[active] + 1) % 10;     draw(); }
        if (btn2_short()) { combo[active] = (combo[active] + 9) % 10;     draw(); }

        if (btn1_long() && active > 0) { active--; draw(); }  // back

        if (btns_both()) {
            active++;
            if (active >= N) {
                for (int i = 0; i < N; i++) pin_out[i] = '0' + combo[i];
                pin_out[N] = '\0';
                return true;
            }
            draw();
        }

        delay(8);
    }
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

        // Handle BLE passkey pairing requests from any page
        if (ble_passkey_pending()) {
            uint32_t pk = ble_passkey_pending();
            ble_passkey_clear();
            ui_show_passkey(pk);
            draw();
        }

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

// ── BLE passkey display ───────────────────────────────────────────────────────
void ui_show_passkey(uint32_t code) {
    tft.fillScreen(C_BG);
    draw_header("BLE Pairing");

    tft.setTextDatum(MC_DATUM);
    tft.setTextColor(C_DIM, C_BG);
    tft.drawString("Enter this code on", DISP_W/2, 38, 1);
    tft.drawString("your computer:", DISP_W/2, 50, 1);

    char buf[8];
    snprintf(buf, sizeof(buf), "%06lu", (unsigned long)code);
    tft.setTextColor(C_PIN, C_BG);
    tft.drawString(buf, DISP_W/2, 85, 4);

    draw_footer("", "any button: dismiss");

    // Stay on screen until the user explicitly dismisses.
    // When pairing completes the status updates in-place but the code
    // stays visible — user reads it at their own pace then presses a button.
    while (true) {
        btns_poll();

        if (ble_pair_done_pending()) {
            bool ok = ble_pair_done_success();
            ble_pair_done_clear();
            // Update status line only — passkey stays readable
            tft.fillRect(0, 108, DISP_W, 10, C_BG);
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(ok ? C_GOOD : C_WARN, C_BG);
            tft.drawString(ok ? "Paired!" : "Pairing failed", DISP_W/2, 114, 1);
        }

        if (btn1_short() || btn2_short() || btn1_long() || btn2_long() || btns_both())
            return;

        delay(10);
    }
}

// ── Password display ──────────────────────────────────────────────────────────
void ui_show_password(const char* pwd, uint32_t index, uint8_t len) {
    auto redraw_screen = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Password");

        char meta[32];
        snprintf(meta, sizeof(meta), "index: %lu", (unsigned long)index);
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
    };

    auto redraw_footer = [&]() {
        if (ble_connected())
            draw_footer("any btn: clear", "BOTH: type BLE");
        else
            draw_footer("", "any button: clear");
    };

    redraw_screen();
    redraw_footer();

    bool prev_conn = ble_connected();

    while (true) {
        btns_poll();

        // Show passkey popup if a BLE pairing request arrives
        uint32_t pk = ble_passkey_pending();
        if (pk) {
            ble_passkey_clear();
            ui_show_passkey(pk);
            redraw_screen();
            redraw_footer();
            prev_conn = ble_connected();
        }

        // Update footer when connection state changes
        bool curr_conn = ble_connected();
        if (curr_conn != prev_conn) {
            redraw_footer();
            prev_conn = curr_conn;
        }

        if (btns_both()) {
            if (ble_connected()) {
                // Type the password via BLE keyboard
                ble_type(pwd);
                // Show brief overlay
                tft.fillRoundRect(20, 88, DISP_W - 40, 22, 4, tft.color565(0, 50, 0));
                tft.setTextDatum(MC_DATUM);
                tft.setTextColor(C_GOOD, tft.color565(0, 50, 0));
                tft.drawString("Sent via BLE!", DISP_W/2, 99, 1);
                delay(1200);
                redraw_screen();
                redraw_footer();
                prev_conn = ble_connected();
            } else {
                break; // not connected — treat BOTH as clear
            }
        }

        if (btn1_short() || btn2_short() || btn1_long() || btn2_long()) break;

        delay(10);
    }

    tft.fillScreen(C_BG);
    tft.setTextColor(C_DIM, C_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("Cleared.", DISP_W/2, DISP_H/2, 2);
    delay(800);
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
        {"Atlantis DeepSea  " FW_VERSION, C_ACCENT},
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

// ── Settings menu ─────────────────────────────────────────────────────────────
SettingsItem ui_settings_menu() {
    int sel = 0;
    const char* labels[SETTINGS_COUNT] = {
        "Bluetooth",
        "Factory Reset",
    };

    auto draw = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Settings");

        for (int i = 0; i < SETTINGS_COUNT; i++) {
            int oy = 36 + i * 26;
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
        if (btn1_short()) { sel = (sel + 1) % SETTINGS_COUNT; draw(); }
        if (btn2_short()) { sel = (sel - 1 + SETTINGS_COUNT) % SETTINGS_COUNT; draw(); }
        if (btns_both())   return (SettingsItem)sel;
        if (btn1_long())   return SETTINGS_COUNT; // back (out of range — caller ignores)
        delay(8);
    }
}

// ── Bluetooth settings ─────────────────────────────────────────────────────────
void ui_ble_settings() {
    auto draw = [&]() {
        tft.fillScreen(C_BG);
        draw_header("Bluetooth");

        // Connection status
        bool conn = ble_connected();
        tft.setTextDatum(ML_DATUM);
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("Status:", 12, 36, 1);
        tft.setTextColor(conn ? C_GOOD : C_ACCENT, C_BG);
        tft.drawString(conn ? "Connected" : "Advertising...", 70, 36, 1);

        // Bonded device count
        int bonds = esp_ble_gap_get_bond_device_num();
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("Bonded devices:", 12, 54, 1);
        char buf[8];
        snprintf(buf, sizeof(buf), "%d", bonds);
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawString(buf, 126, 54, 1);

        // Info line
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("BLE always on.", 12, 72, 1);
        tft.drawString("Device: Atlantis DeepSea", 12, 84, 1);

        draw_footer("1: forget devices", "BOTH / 2: back");
    };

    draw();

    while (true) {
        btns_poll();

        // BTN1 short = forget all bonds (with confirmation)
        if (btn1_short()) {
            tft.fillScreen(C_BG);
            draw_header("Forget Devices?");
            tft.setTextDatum(MC_DATUM);
            tft.setTextColor(C_WARN, C_BG);
            tft.drawString("Remove all paired", DISP_W/2, 50, 2);
            tft.drawString("devices?", DISP_W/2, 72, 2);
            draw_footer("1: confirm", "2 / BOTH: cancel");

            bool confirmed = false;
            while (true) {
                btns_poll();
                if (btn1_short()) { confirmed = true; break; }
                if (btn2_short() || btns_both()) break;
                delay(8);
            }

            if (confirmed) {
                ble_forget_bonds();
                ui_message("Done", "All bonds removed.\nRe-pair next connect.", 2000);
            }
            draw();
        }

        if (btn2_short() || btns_both()) return;
        delay(8);
    }
}

// ── Wipe confirm ──────────────────────────────────────────────────────────────
void ui_confirm_wipe() {
    // Shown from Settings — not yet wired; placeholder
    ui_message("Factory Reset",
               "Hold BTN1 5s to wipe", 0);
}
