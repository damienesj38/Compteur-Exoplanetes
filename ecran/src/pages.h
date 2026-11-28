// Dessin des pages de l'écran sur le canevas 800 × 480.
#pragma once

#include <string>
#include <vector>

#include "model.h"
#include "rendu.h"

void pageAccueil(Canevas& c, const DonneesCompteur& d, const Meteo& m, const Reglages& r, time_t maintenant,
                 const EtatEcran& e);
void pageCompteur(Canevas& c, const DonneesCompteur& d, const Meteo& m, time_t maintenant, const EtatEcran& e);
void pageMeteo(Canevas& c, const Meteo& m, const Reglages& r, time_t maintenant);

void ecranMessage(Canevas& c, const std::string& titre, const std::vector<std::string>& lignes,
                  uint32_t couleurTitre = coul::CYAN);
void ecranConfiguration(Canevas& c, const std::string& nomWifi);
void ecranReinitialisation(Canevas& c);
void ecranMiseAJour(Canevas& c, int pourcent, const std::string& version);
void ecranReglages(Canevas& c, const PrefsUtilisateur& p, const std::string& nomAffichage, bool redemarrage,
                   const std::string& infos1, const std::string& infos2);

// Page Réglages : 6 lignes, boutons − et + (ou bascule), puis 2 boutons en bas.
constexpr int REG_Y0 = 62, REG_PAS = 50, REG_H = 42, REG_LIGNES = 6;
constexpr int REG_MOINS_X = 560, REG_PLUS_X = 650, REG_BTN_L = 70, REG_BASCULE_L = 160;
constexpr int REG_BAS_Y = 372, REG_BAS_H = 48, REG_WIFI_X = 40, REG_WIFI_L = 330, REG_RETOUR_X = 580, REG_RETOUR_L = 180;
enum ZoneReglage { Z_AUCUNE = 0, Z_MOINS = 10, Z_PLUS = 20, Z_ALERTE = 30, Z_WIFI = 40, Z_RETOUR = 41 };
int zoneReglages(int x, int y);  // Z_MOINS + ligne, Z_PLUS + ligne, Z_ALERTE…
// Icône des réglages sur la page d'accueil (centre) et sa zone tactile.
constexpr int ENGRENAGE_X = 545, ENGRENAGE_Y = 457;
inline bool toucheEngrenage(int x, int y) { return x >= 505 && x < 569 && y >= 425 && y < 480; }

// Zones tactiles de l'écran de réinitialisation.
constexpr int BOUTON_OUI_X = 220, BOUTON_NON_X = 420, BOUTON_Y = 320, BOUTON_L = 160, BOUTON_H = 64;
