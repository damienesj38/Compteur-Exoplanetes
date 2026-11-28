#include "ecran.h"

#include <Arduino.h>

#include "board_pins.h"

LGFX ecran;
lgfx::LGFX_Sprite canevas(&ecran);

LGFX::LGFX() {
    {
        auto cfg = bus_.config();
        cfg.panel = &panel_;
        cfg.pin_d0 = board::LCD_B0;
        cfg.pin_d1 = board::LCD_B1;
        cfg.pin_d2 = board::LCD_B2;
        cfg.pin_d3 = board::LCD_B3;
        cfg.pin_d4 = board::LCD_B4;
        cfg.pin_d5 = board::LCD_G0;
        cfg.pin_d6 = board::LCD_G1;
        cfg.pin_d7 = board::LCD_G2;
        cfg.pin_d8 = board::LCD_G3;
        cfg.pin_d9 = board::LCD_G4;
        cfg.pin_d10 = board::LCD_G5;
        cfg.pin_d11 = board::LCD_R0;
        cfg.pin_d12 = board::LCD_R1;
        cfg.pin_d13 = board::LCD_R2;
        cfg.pin_d14 = board::LCD_R3;
        cfg.pin_d15 = board::LCD_R4;
        cfg.pin_henable = board::LCD_DE;
        cfg.pin_vsync = board::LCD_VSYNC;
        cfg.pin_hsync = board::LCD_HSYNC;
        cfg.pin_pclk = board::LCD_PCLK;
        cfg.freq_write = board::LCD_PCLK_HZ;
        cfg.hsync_polarity = board::LCD_HSYNC_POLARITY;
        cfg.hsync_front_porch = board::LCD_HSYNC_FRONT_PORCH;
        cfg.hsync_pulse_width = board::LCD_HSYNC_PULSE_WIDTH;
        cfg.hsync_back_porch = board::LCD_HSYNC_BACK_PORCH;
        cfg.vsync_polarity = board::LCD_VSYNC_POLARITY;
        cfg.vsync_front_porch = board::LCD_VSYNC_FRONT_PORCH;
        cfg.vsync_pulse_width = board::LCD_VSYNC_PULSE_WIDTH;
        cfg.vsync_back_porch = board::LCD_VSYNC_BACK_PORCH;
        cfg.pclk_idle_high = board::LCD_PCLK_IDLE_HIGH;
        bus_.config(cfg);
    }
    {
        auto cfg = panel_.config();
        cfg.memory_width = board::LCD_WIDTH;
        cfg.memory_height = board::LCD_HEIGHT;
        cfg.panel_width = board::LCD_WIDTH;
        cfg.panel_height = board::LCD_HEIGHT;
        cfg.offset_x = 0;
        cfg.offset_y = 0;
        panel_.config(cfg);
    }
    panel_.setBus(&bus_);
    setPanel(&panel_);
}

void LGFX::reglerSignal(uint32_t horlogeHz, int polarite) {
    auto cfg = bus_.config();
    cfg.freq_write = horlogeHz;
    cfg.hsync_polarity = polarite;
    cfg.vsync_polarity = polarite;
    bus_.config(cfg);
}

namespace {
struct ReglageAffichage {
    uint32_t hz;
    int polarite;
    const char* nom;
};
#if CROWPANEL_ADVANCE_REV >= 130
// n°1 = exemple PlatformIO d'Elecrow pour les V1.3 à V1.5.
const ReglageAffichage REGLAGES[] = {
    {18000000, 1, "18 MHz, synchro 1"},
    {16000000, 0, "16 MHz, synchro 0"},
    {18000000, 0, "18 MHz, synchro 0"},
    {16000000, 1, "16 MHz, synchro 1"},
    {21000000, 1, "21 MHz, synchro 1"},
    {21000000, 0, "21 MHz, synchro 0"},
};
#else
const ReglageAffichage REGLAGES[] = {
    {board::LCD_PCLK_HZ, 0, "21 MHz, synchro 0"},
};
#endif
constexpr int NB_REGLAGES = sizeof(REGLAGES) / sizeof(REGLAGES[0]);
}  // namespace

int nombreReglagesAffichage() { return NB_REGLAGES; }
const char* nomReglageAffichage(int n) { return REGLAGES[(n % NB_REGLAGES + NB_REGLAGES) % NB_REGLAGES].nom; }

bool initEcran(int reglage) {
    const ReglageAffichage& r = REGLAGES[(reglage % NB_REGLAGES + NB_REGLAGES) % NB_REGLAGES];
    ecran.reglerSignal(r.hz, r.polarite);
    if (!ecran.init()) {
        Serial.println("ecran: init() a échoué");
        return false;
    }
    ecran.setColorDepth(16);
    ecran.fillScreen(TFT_BLACK);
    canevas.setPsram(true);
    canevas.setColorDepth(16);
    if (!canevas.createSprite(board::LCD_WIDTH, board::LCD_HEIGHT)) {
        Serial.println("ecran: pas assez de PSRAM pour le canevas (vérifier « OPI PSRAM »)");
        return false;
    }
    Serial.printf("ecran: %dx%d, %s\n", board::LCD_WIDTH, board::LCD_HEIGHT, r.nom);
    return true;
}

void afficher() { canevas.pushSprite(0, 0); }
