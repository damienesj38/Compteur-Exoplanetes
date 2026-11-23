// Écran RGB 800 × 480 de la CrowPanel Advance 7" et canevas hors écran en PSRAM.
// Configuration LovyanGFX reprise du projet robominds/Crowpanel-Advance-7.0-HMI-Display
// (licence MIT, voir LICENSE-robominds.txt), elle-même tirée du pilote d'Elecrow.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include <lgfx/v1/platforms/esp32s3/Bus_RGB.hpp>
#include <lgfx/v1/platforms/esp32s3/Panel_RGB.hpp>

class LGFX : public lgfx::LGFX_Device {
  public:
    LGFX();

  private:
    lgfx::Bus_RGB bus_;
    lgfx::Panel_RGB panel_;
};

extern LGFX ecran;
extern lgfx::LGFX_Sprite canevas;

bool initEcran();
void afficher();  // copie le canevas à l'écran
