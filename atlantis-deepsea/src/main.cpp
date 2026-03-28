#include <Arduino.h>
#include "config.h"
#include "ui.h"
#include "crypto.h"
#include "storage.h"

// ── App state machine ─────────────────────────────────────────────────────────
enum AppState {
    STATE_BOOT,
    STATE_PIN_ENTRY,
    STATE_SETUP_WORD_COUNT,
    STATE_SEED_ENTRY,
    STATE_SEED_CONFIRM,
    STATE_MAIN_MENU,
    STATE_GET_PASSWORD,
    STATE_SETTINGS,
    STATE_ABOUT,
    STATE_LOCKED,
};

static AppState       state          = STATE_BOOT;
static uint8_t        master_key[32] = {0};
static uint8_t        master_chain[32]= {0};
static bool           key_loaded     = false;
static uint32_t       last_activity  = 0;

// Scratch space for seed entry
static char           seed_words[24][10];
static int            word_count     = 12;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void clear_keys() {
    memset(master_key,   0, 32);
    memset(master_chain, 0, 32);
    key_loaded = false;
}

static void touch_activity() {
    last_activity = millis();
}

static bool idle_timeout() {
    return key_loaded && ((millis() - last_activity) > AUTO_LOCK_MS);
}

// ── Setup: gather mnemonic and store ─────────────────────────────────────────
static void run_setup() {
    // 1. Choose word count
    state = STATE_SETUP_WORD_COUNT;
    word_count = ui_choose_word_count();

    // 2. Enter seed words
    state = STATE_SEED_ENTRY;
    for (int i = 0; i < word_count; i++) {
        ui_enter_word(i + 1, word_count, seed_words[i]);
    }

    // 3. Validate checksum
    const char* ptrs[24];
    for (int i = 0; i < word_count; i++) ptrs[i] = seed_words[i];

    while (!bip39_validate(ptrs, word_count)) {
        ui_seed_invalid();
        // Re-enter all words
        for (int i = 0; i < word_count; i++)
            ui_enter_word(i + 1, word_count, seed_words[i]);
    }

    // 4. Confirm 3 random words (positions spread evenly)
    state = STATE_SEED_CONFIRM;
    int check_pos[3] = {
        1,
        word_count / 2,
        word_count
    };
    for (int i = 0; i < 3; i++) {
        int pos = check_pos[i];
        while (!ui_confirm_word(pos, word_count, seed_words[pos - 1])) {
            ui_message("Mismatch", "Word didn't match.\nTry again.", 1500);
        }
    }

    // 5. Derive BIP32 master key
    uint8_t seed[64];
    bip39_to_seed(ptrs, word_count, seed);
    bip32_master(seed, 64, master_key, master_chain);
    memset(seed, 0, 64);

    // 6. Set PIN
    ui_message("Set PIN", "Choose a 6-digit PIN\nto protect this device.", 2000);
    char pin[7] = {0};
    char pin2[7] = {0};
    bool pin_ok = false;
    while (!pin_ok) {
        ui_pin_entry(pin,  false, PIN_MAX_ATTEMPTS, false, 0);
        ui_message("Confirm PIN", "Re-enter your PIN.", 1200);
        ui_pin_entry(pin2, false, PIN_MAX_ATTEMPTS, false, 0);
        if (strcmp(pin, pin2) == 0) {
            pin_ok = true;
        } else {
            memset(pin, 0, 7); memset(pin2, 0, 7);
            ui_message("Mismatch", "PINs didn't match.\nTry again.", 1500);
        }
    }

    // 7. Save to storage
    if (!storage_save(master_key, master_chain, pin, (uint8_t)word_count)) {
        ui_message("Error", "Failed to save.\nDevice may need reset.", 3000);
        clear_keys();
        ESP.restart();
    }
    memset(pin, 0, 7); memset(pin2, 0, 7);

    // Clear seed words from RAM
    for (int i = 0; i < 24; i++) memset(seed_words[i], 0, 10);

    key_loaded = true;
    touch_activity();

    ui_message("Success!", "Wallet stored securely.\nEntering vault...", 2000);
    state = STATE_MAIN_MENU;
}

// ── PIN unlock ────────────────────────────────────────────────────────────────
static void run_pin_entry() {
    bool wrong = false;
    int  attempts_left = storage_attempts_left();

    while (true) {
        char pin[7] = {0};
        ui_pin_entry(pin, wrong, attempts_left, false, 0);

        if (storage_load(pin, master_key, master_chain)) {
            memset(pin, 0, 7);
            key_loaded = true;
            touch_activity();
            state = STATE_MAIN_MENU;
            return;
        }

        memset(pin, 0, 7);
        wrong = true;
        attempts_left = storage_attempts_left();

        if (attempts_left == 0) {
            // Security wipe — erase seed and all data, restart as new device
            ui_message("SECURITY WIPE",
                       "5 wrong PINs.\nErasing all data...", 3000);
            storage_wipe();
            ESP.restart();
        }
    }
}

// ── Settings sub-menu ─────────────────────────────────────────────────────────
static void run_settings() {
    ui_message("Settings",
               "Hold BTN1 at boot to\ndo factory reset.", 0);
}

// ── Main ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    ui_init();
    storage_init();
    state = STATE_BOOT;
}

void loop() {
    btns_poll();

    // Global idle auto-lock
    if (idle_timeout()) {
        clear_keys();
        state = STATE_LOCKED;
    }

    switch (state) {

    case STATE_BOOT:
        ui_boot();
        if (storage_is_setup())
            state = STATE_PIN_ENTRY;
        else
            run_setup();
        break;

    case STATE_PIN_ENTRY:
    case STATE_LOCKED:
        run_pin_entry();
        break;

    case STATE_MAIN_MENU: {
        touch_activity();
        MenuItem choice = ui_main_menu();
        touch_activity();
        switch (choice) {
            case MENU_GET_PASSWORD: state = STATE_GET_PASSWORD; break;
            case MENU_SETTINGS:     run_settings(); break;
            case MENU_ABOUT:        ui_about(); break;
            case MENU_LOCK:
                clear_keys();
                state = STATE_LOCKED;
                break;
            default: break;
        }
        break;
    }

    case STATE_GET_PASSWORD: {
        touch_activity();
        uint32_t idx = 0;

        if (!ui_password_config(&idx)) {
            state = STATE_MAIN_MENU;
            break;
        }

        touch_activity();

        ui_message("Deriving...", "Computing password...", 1200);

        char pwd[87] = {0};
        bool ok = bip85_password(master_key, master_chain, PWD_LEN_FIXED, idx, pwd);

        if (!ok) {
            ui_message("Error", "Derivation failed.\nTry a different index.", 2500);
        } else {
            ui_show_password(pwd, idx, PWD_LEN_FIXED);
            memset(pwd, 0, sizeof(pwd));
        }

        touch_activity();
        state = STATE_MAIN_MENU;
        break;
    }

    case STATE_SETTINGS:
        run_settings();
        state = STATE_MAIN_MENU;
        break;

    case STATE_ABOUT:
        ui_about();
        state = STATE_MAIN_MENU;
        break;

    default:
        state = STATE_MAIN_MENU;
        break;
    }

    delay(5);
}
