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

// Zones tactiles de l'écran de réinitialisation.
constexpr int BOUTON_OUI_X = 220, BOUTON_NON_X = 420, BOUTON_Y = 320, BOUTON_L = 160, BOUTON_H = 64;
