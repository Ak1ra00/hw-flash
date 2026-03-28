#include <Arduino.h>
#include "config.h"
#include "ble.h"
#include "ui.h"
#include "crypto.h"
#include "storage.h"

// ── App state machine ─────────────────────────────────────────────────────────
enum AppState {
    STATE_BOOT,
    STATE_SETUP_WORD_COUNT,
    STATE_SEED_ENTRY,
    STATE_SEED_CONFIRM,
    STATE_MAIN_MENU,
    STATE_GET_PASSWORD,
    STATE_SETTINGS,
    STATE_ABOUT,
};

static AppState       state          = STATE_BOOT;
static uint8_t        master_key[32] = {0};
static uint8_t        master_chain[32]= {0};
static bool           key_loaded     = false;

// Scratch space for seed entry
static char           seed_words[24][10];
static int            word_count     = 12;

// ── Helpers ───────────────────────────────────────────────────────────────────
static void clear_keys() {
    memset(master_key,   0, 32);
    memset(master_chain, 0, 32);
    key_loaded = false;
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

    // 6. Save to storage (hardware-bound key, no user PIN required)
    if (!storage_save(master_key, master_chain, (uint8_t)word_count)) {
        ui_message("Error", "Failed to save.\nDevice may need reset.", 3000);
        clear_keys();
        ESP.restart();
    }

    // Clear seed words from RAM
    for (int i = 0; i < 24; i++) memset(seed_words[i], 0, 10);

    key_loaded = true;

    ui_message("Success!", "Wallet stored securely.\nEntering vault...", 2000);
    state = STATE_MAIN_MENU;
}

// ── Settings sub-menu ─────────────────────────────────────────────────────────
static void run_settings() {
    SettingsItem choice = ui_settings_menu();
    if (choice == SETTINGS_BLUETOOTH) {
        ui_ble_settings();
    } else if (choice == SETTINGS_FACTORY_RESET) {
        // Confirm then wipe
        ui_message("Factory Reset",
                   "Hold BTN1 3s to wipe\nall data.", 0);
        uint32_t start = millis();
        while (millis() - start < 10000UL) {
            btns_poll();
            if (btn1_long()) {
                storage_wipe();
                ui_message("Wiped", "All data erased.\nRestarting...", 2000);
                ESP.restart();
            }
            if (btn2_short() || btns_both()) break;
            delay(8);
        }
    }
    // SETTINGS_COUNT (back) or unknown — return to menu
}

// ── Main ──────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    ui_init();
    storage_init();   // Init NVS first — if BLE sees it already initialized
    ble_init();       // it gets INVALID_STATE and skips nvs_flash_erase()
    state = STATE_BOOT;
}

void loop() {
    btns_poll();

    switch (state) {

    case STATE_BOOT:
        ui_boot();
        if (storage_is_setup()) {
            // Load keys directly — no PIN required
            if (storage_load(master_key, master_chain)) {
                key_loaded = true;
                state = STATE_MAIN_MENU;
            } else {
                // Stored data is unreadable (likely saved by an older firmware
                // version with a different encryption scheme).  Wipe and re-enter.
                ui_message("Re-enter Seed",
                           "Seed needs re-entry\n(firmware updated).\nPlease re-enter now.", 3000);
                storage_wipe();
                run_setup();
            }
        } else {
            run_setup();
        }
        break;

    case STATE_MAIN_MENU: {
        MenuItem choice = ui_main_menu();
        switch (choice) {
            case MENU_GET_PASSWORD: state = STATE_GET_PASSWORD; break;
            case MENU_SETTINGS:     run_settings(); break;
            case MENU_ABOUT:        ui_about(); break;
            default: break;
        }
        break;
    }

    case STATE_GET_PASSWORD: {
        if (!key_loaded) {
            ui_message("No Seed", "No seed loaded.\nUse Settings >\nFactory Reset.", 3000);
            state = STATE_MAIN_MENU;
            break;
        }

        uint32_t idx = 0;

        if (!ui_password_config(&idx)) {
            state = STATE_MAIN_MENU;
            break;
        }

        ui_message("Deriving...", "Computing password...", 1200);

        char pwd[87] = {0};
        bool ok = bip85_password(master_key, master_chain, PWD_LEN_FIXED, idx, pwd);

        if (!ok) {
            ui_message("Error", "Derivation failed.\nTry a different index.", 2500);
        } else {
            ui_show_password(pwd, idx, PWD_LEN_FIXED);
            memset(pwd, 0, sizeof(pwd));
        }

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
