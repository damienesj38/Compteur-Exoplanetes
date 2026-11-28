// Écran RGB 800 × 480 de la CrowPanel Advance 7" et canevas hors écran en PSRAM.
// Depuis la version 1.0.4, l'affichage passe par le pilote esp_lcd d'Espressif,
// réglé comme dans les exemples ESP-IDF d'Elecrow : petit tampon intermédiaire en
// mémoire interne (« bounce buffer ») pour que l'image ne décroche plus quand la
// PSRAM est sollicitée, et deux images en alternance pour changer de page sans
// déchirure. LovyanGFX ne sert plus qu'à dessiner dans le canevas.
#pragma once

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

extern lgfx::LGFX_Sprite canevas;

// Réglages du signal vidéo à essayer (horloge pixel, niveau de repos des synchros).
int nombreReglagesAffichage();
const char* nomReglageAffichage(int n);

bool initEcran(int reglage = 0);
void afficher();  // copie le canevas à l'écran, au prochain changement d'image
