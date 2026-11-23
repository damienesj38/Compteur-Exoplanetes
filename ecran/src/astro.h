// Calculs locaux : phase de la Lune (formules simplifiées de Meeus, précision de
// l'ordre du pour cent) et petits utilitaires de date en français.
#pragma once

#include <ctime>
#include <string>

struct Lune {
    double eclairement = 0;   // 0 à 1
    bool croissante = true;   // vrai entre nouvelle et pleine lune
    int joursAvantPleine = 0; // en jours calendaires locaux
    int joursAvantNouvelle = 0;
};

Lune calculerLune(time_t utc, int decalageUtc);
std::string nomPhase(const Lune& l);

// Date/heure locale décomposée (sans dépendre du fuseau du système).
struct tm heureLocale(time_t utc, int decalageUtc);

extern const char* const JOURS_COURTS[7];   // « dim. », « lun. »…
extern const char* const MOIS_COURTS[12];   // « janv. »…
extern const char* const MOIS_LONGS[12];    // « janvier »…
