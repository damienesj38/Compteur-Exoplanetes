// Contrôleur tactile GT911, interrogé toutes les 20 ms au plus.
// Adapté de robominds/Crowpanel-Advance-7.0-HMI-Display (licence MIT), sans LVGL.
#pragma once

#include <cstdint>

bool initTactile();
void lireTactile();  // à appeler à chaque tour de boucle
bool tactileAppuye();
int16_t tactileX();
int16_t tactileY();
