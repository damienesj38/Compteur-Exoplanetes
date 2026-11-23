// Interprétation des prévisions : résumé de la nuit, créneaux de la page météo.
// Sans dépendance Arduino (compilé aussi par le simulateur).
#pragma once

#include <string>

#include "model.h"

struct ResumeNuit {
    bool ok = false;
    int code = 3;              // code météo dominant (OMM)
    int nuages = 0;            // couverture moyenne (%)
    std::string description;   // « Très nuageux »
    std::string detail;        // « Éclaircie vers 22 h »
    int seeing = 0;            // meilleur seeing de la nuit (1-8), 0 si inconnu
    int seeingHeure = -1;      // heure locale de ce meilleur seeing
    int transparence = 0;      // à la même heure (1-8), 0 si inconnu
    int tempMin = 0;
    int ventMax = 0;           // km/h
    int meilleureHeure = -1;   // heure locale du créneau le plus dégagé, -1 si aucun
};

std::string descriptionMeteo(int code, int nuages);
const char* qualiteSeeing(int s);        // « Bon »…
const char* qualiteTransparence(int t);  // « Faible »…
const char* arcsecSeeing(int s);         // « 0,6″ »
const char* directionVent(int degres);   // « NO »

// Indice 7Timer le plus proche de l'instant donné (à 2 h près), 0 sinon.
int seeingA(const Meteo& m, time_t utc);
int transparenceA(const Meteo& m, time_t utc);

ResumeNuit resumerNuit(const Meteo& m, time_t maintenant);

// Remplit jusqu'à 8 indices d'heures (dans m.h) espacés de 3 h à partir de maintenant.
int choisirColonnes(const Meteo& m, time_t maintenant, int idx[8]);
