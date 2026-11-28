#include "ecran.h"

#include <Arduino.h>
#include <esp_lcd_panel_ops.h>
#include <esp_lcd_panel_rgb.h>
#include <freertos/semphr.h>

#include "board_pins.h"

lgfx::LGFX_Sprite canevas;

namespace {

constexpr int L = board::LCD_WIDTH;
constexpr int H = board::LCD_HEIGHT;
int lignesTampon = 20;  // tampon intermédiaire (Elecrow : 10 lignes), ramené à 10 si la mémoire manque

esp_lcd_panel_handle_t panneau = nullptr;
uint16_t* images[2] = {nullptr, nullptr};
int imageAffichee = 0;
SemaphoreHandle_t semVsync = nullptr;

bool surVsync(esp_lcd_panel_handle_t, const esp_lcd_rgb_panel_event_data_t*, void*) {
    BaseType_t reveil = pdFALSE;
    xSemaphoreGiveFromISR(semVsync, &reveil);
    return reveil == pdTRUE;
}

struct ReglageAffichage {
    uint32_t hz;
    bool synchroReposBas;
    const char* nom;
};
#if CROWPANEL_ADVANCE_REV >= 130
// Essais sur une V1.5 (28/11/2026) : 13, 14 et 16 MHz font sauter le bas de
// l'image, 18 MHz va mieux. La dalle semble vouloir une fréquence d'image plus
// élevée : on teste donc au-dessus de 18 MHz.
const ReglageAffichage REGLAGES[] = {
    {18000000, false, "18 MHz"},
    {21000000, false, "21 MHz"},
    {24000000, false, "24 MHz"},
    {27000000, false, "27 MHz"},
};
#else
const ReglageAffichage REGLAGES[] = {
    {board::LCD_PCLK_HZ, true, "21 MHz, synchro B"},
};
#endif
constexpr int NB_REGLAGES = sizeof(REGLAGES) / sizeof(REGLAGES[0]);

const ReglageAffichage& reglage(int n) { return REGLAGES[(n % NB_REGLAGES + NB_REGLAGES) % NB_REGLAGES]; }

}  // namespace

int nombreReglagesAffichage() { return NB_REGLAGES; }
const char* nomReglageAffichage(int n) { return reglage(n).nom; }

bool initEcran(int n) {
    const ReglageAffichage& r = reglage(n);
    semVsync = xSemaphoreCreateBinary();

    esp_lcd_rgb_panel_config_t cfg = {};
    cfg.clk_src = LCD_CLK_SRC_DEFAULT;
    cfg.timings.pclk_hz = r.hz;
    cfg.timings.h_res = L;
    cfg.timings.v_res = H;
    cfg.timings.hsync_pulse_width = board::LCD_HSYNC_PULSE_WIDTH;
    cfg.timings.hsync_back_porch = board::LCD_HSYNC_BACK_PORCH;
    cfg.timings.hsync_front_porch = board::LCD_HSYNC_FRONT_PORCH;
    cfg.timings.vsync_pulse_width = board::LCD_VSYNC_PULSE_WIDTH;
    cfg.timings.vsync_back_porch = board::LCD_VSYNC_BACK_PORCH;
    cfg.timings.vsync_front_porch = board::LCD_VSYNC_FRONT_PORCH;
    cfg.timings.flags.hsync_idle_low = r.synchroReposBas;
    cfg.timings.flags.vsync_idle_low = r.synchroReposBas;
    cfg.timings.flags.pclk_active_neg = 1;
    cfg.data_width = 16;
    cfg.bits_per_pixel = 16;
    cfg.num_fbs = 2;
    cfg.bounce_buffer_size_px = L * lignesTampon;
    cfg.dma_burst_size = 64;
    cfg.hsync_gpio_num = board::LCD_HSYNC;
    cfg.vsync_gpio_num = board::LCD_VSYNC;
    cfg.de_gpio_num = board::LCD_DE;
    cfg.pclk_gpio_num = board::LCD_PCLK;
    cfg.disp_gpio_num = -1;
    const int donnees[16] = {board::LCD_B0, board::LCD_B1, board::LCD_B2, board::LCD_B3, board::LCD_B4,
                             board::LCD_G0, board::LCD_G1, board::LCD_G2, board::LCD_G3, board::LCD_G4,
                             board::LCD_G5, board::LCD_R0, board::LCD_R1, board::LCD_R2, board::LCD_R3,
                             board::LCD_R4};
    for (int i = 0; i < 16; ++i) cfg.data_gpio_nums[i] = donnees[i];
    cfg.flags.fb_in_psram = 1;

    if (esp_lcd_new_rgb_panel(&cfg, &panneau) != ESP_OK) {
        lignesTampon = 10;
        cfg.bounce_buffer_size_px = L * lignesTampon;
        if (esp_lcd_new_rgb_panel(&cfg, &panneau) != ESP_OK) {
            Serial.println("ecran: esp_lcd_new_rgb_panel a échoué");
            return false;
        }
    }
    esp_lcd_rgb_panel_event_callbacks_t rappels = {};
    rappels.on_vsync = surVsync;
    esp_lcd_rgb_panel_register_event_callbacks(panneau, &rappels, nullptr);
    esp_lcd_panel_reset(panneau);
    esp_lcd_panel_init(panneau);
    void* fb0 = nullptr;
    void* fb1 = nullptr;
    if (esp_lcd_rgb_panel_get_frame_buffer(panneau, 2, &fb0, &fb1) != ESP_OK) {
        Serial.println("ecran: images introuvables");
        return false;
    }
    images[0] = static_cast<uint16_t*>(fb0);
    images[1] = static_cast<uint16_t*>(fb1);
    memset(images[0], 0, L * H * 2);
    memset(images[1], 0, L * H * 2);
    imageAffichee = 0;

    canevas.setPsram(true);
    canevas.setColorDepth(16);
    if (!canevas.createSprite(L, H)) {
        Serial.println("ecran: pas assez de PSRAM pour le canevas (vérifier « OPI PSRAM »)");
        return false;
    }
    Serial.printf("ecran: %dx%d, %s, tampon %d lignes\n", L, H, r.nom, lignesTampon);
    return true;
}

void afficher() {
    if (!panneau) return;
    const int arriere = 1 - imageAffichee;
    // Le canevas LovyanGFX range ses pixels RGB565 octets inversés : on les remet
    // dans l'ordre attendu par l'écran en recopiant dans l'image cachée.
    const uint32_t* src = static_cast<const uint32_t*>(canevas.getBuffer());
    uint32_t* dst = reinterpret_cast<uint32_t*>(images[arriere]);
    for (int i = 0; i < L * H / 2; ++i) {
        const uint32_t x = src[i];
        dst[i] = ((x & 0x00FF00FFu) << 8) | ((x >> 8) & 0x00FF00FFu);
    }
    // Bascule vers cette image au début de la prochaine trame.
    xSemaphoreTake(semVsync, 0);
    esp_lcd_panel_draw_bitmap(panneau, 0, 0, L, H, images[arriere]);
    // Attendre que la bascule ait eu lieu avant de réécrire dans l'autre image.
    xSemaphoreTake(semVsync, pdMS_TO_TICKS(100));
    xSemaphoreTake(semVsync, pdMS_TO_TICKS(100));
    imageAffichee = arriere;
}
