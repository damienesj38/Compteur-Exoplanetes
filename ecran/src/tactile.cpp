#include "tactile.h"

#include <Arduino.h>
#include <Wire.h>

#include "board_pins.h"
#include "panel_mcu.h"

namespace {

constexpr uint16_t REG_ETAT = 0x814E;

uint8_t g_adresse = 0;
bool g_appuye = false;
int16_t g_x = 0, g_y = 0;
uint32_t g_dernier = 0;

bool lire(uint16_t reg, uint8_t* out, size_t n) {
    Wire.beginTransmission(g_adresse);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom(static_cast<int>(g_adresse), static_cast<int>(n)) != static_cast<int>(n)) return false;
    for (size_t i = 0; i < n; ++i) out[i] = Wire.read();
    return true;
}

bool ecrire(uint16_t reg, uint8_t v) {
    Wire.beginTransmission(g_adresse);
    Wire.write(static_cast<uint8_t>(reg >> 8));
    Wire.write(static_cast<uint8_t>(reg & 0xFF));
    Wire.write(v);
    return Wire.endTransmission() == 0;
}

bool present(uint8_t a) {
    Wire.beginTransmission(a);
    return Wire.endTransmission() == 0;
}

}  // namespace

bool initTactile() {
    // INT maintenu bas pendant la remise à zéro : le GT911 prend l'adresse 0x5D.
    pinMode(board::TP_INT, OUTPUT);
    digitalWrite(board::TP_INT, LOW);
    panel_mcu::activateTouch();
    delay(120);
    pinMode(board::TP_INT, INPUT);
    delay(100);
    if (present(board::GT911_ADDR_PRIMARY)) g_adresse = board::GT911_ADDR_PRIMARY;
    else if (present(board::GT911_ADDR_BACKUP)) g_adresse = board::GT911_ADDR_BACKUP;
    else {
        Serial.println("tactile: aucun GT911 trouvé");
        return false;
    }
    Serial.printf("tactile: GT911 à 0x%02X\n", g_adresse);
    return true;
}

void lireTactile() {
    if (g_adresse == 0) return;
    const uint32_t t = millis();
    if (t - g_dernier < board::TOUCH_POLL_MS) return;
    g_dernier = t;
    uint8_t b[6];
    if (!lire(REG_ETAT, b, sizeof b)) return;
    if ((b[0] & 0x80) == 0) return;  // pas de nouvelle mesure
    if ((b[0] & 0x0F) > 0) {
        int16_t x = static_cast<int16_t>(b[2] | (b[3] << 8));
        int16_t y = static_cast<int16_t>(b[4] | (b[5] << 8));
        g_x = x < 0 ? 0 : (x >= board::LCD_WIDTH ? board::LCD_WIDTH - 1 : x);
        g_y = y < 0 ? 0 : (y >= board::LCD_HEIGHT ? board::LCD_HEIGHT - 1 : y);
        g_appuye = true;
    } else {
        g_appuye = false;
    }
    ecrire(REG_ETAT, 0);
}

bool tactileAppuye() { return g_appuye; }
int16_t tactileX() { return g_x; }
int16_t tactileY() { return g_y; }
