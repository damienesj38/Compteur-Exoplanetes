// Réglages de compilation du compteur d'exoplanètes.
#pragma once

#include "board_pins.h"

#define FW_VERSION "1.0.0"

// Fichier publié chaque heure par le dépôt GitHub (compteur, chiffre du jour, lancements).
#define URL_DONNEES "https://damienesj38.github.io/Compteur-Exoplanetes/data.json"

// Nom du réseau Wi-Fi temporaire ouvert par l'écran pour sa configuration.
#define NOM_WIFI_CONFIG "Compteur-Exoplanetes"

#if CROWPANEL_ADVANCE_REV >= 130
#define VERSION_CARTE "v13"
#else
#define VERSION_CARTE "v12"
#endif

// Intervalles (millisecondes)
#define INTERVALLE_DONNEES (10UL * 60 * 1000)
#define INTERVALLE_METEO (30UL * 60 * 1000)
#define INTERVALLE_ASTRO (3UL * 60 * 60 * 1000)
#define NOUVEL_ESSAI (5UL * 60 * 1000)
#define DUREE_ALERTE (60UL * 1000)
#define RETOUR_ACCUEIL (3UL * 60 * 1000)

// Heures calmes : pas d'alerte, luminosité réduite.
#define HEURE_DEBUT_NUIT 22
#define HEURE_FIN_NUIT 7
#define LUMINOSITE_JOUR 100
#define LUMINOSITE_NUIT 35
