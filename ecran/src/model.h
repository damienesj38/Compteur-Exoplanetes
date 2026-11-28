// Données affichées par l'écran. Aucune dépendance Arduino : ce fichier est aussi
// compilé sur ordinateur pour générer des captures des pages (tools/simulateur).
#pragma once

#include <cstdint>
#include <ctime>
#include <string>

constexpr int NB_LANCEMENTS = 5;
constexpr int NB_HEURES = 48;
constexpr int NB_ASTRO = 24;
constexpr int VALEUR_ABSENTE = -99999;

struct Lancement {
    std::string fusee;
    std::string mission;
    std::string lieu;      // lieu court (ex. « Kourou »)
    std::string statut;    // en français
    time_t net = 0;        // heure UTC
};

// Contenu de data.json (dépôt GitHub du compteur).
struct DonneesCompteur {
    bool ok = false;
    int total = VALEUR_ABSENTE;
    std::string libelle = "Objets extrasolaires confirmés";
    std::string majCatalogue;          // AAAA-MM-JJ
    int nouvelles24h = VALEUR_ABSENTE;
    std::string chiffreValeur;         // format afficheur, ex. « 25,9 »
    std::string chiffreUnite;          // ex. « milliards de km »
    std::string chiffreLegende;        // ex. « Voyager 1 · distance »
    Lancement lancements[NB_LANCEMENTS];
    int nbLancements = 0;
    std::string firmwareVersion;
    std::string firmwareUrl;
    time_t recuLe = 0;
};

struct Heure {
    time_t t = 0;          // UTC
    float temp = 0;        // °C
    int nuages = 0;        // %
    int code = 0;          // code météo OMM
    float pluie = 0;       // mm
    float vent = 0;        // km/h
    int ventDir = 0;       // degrés, d'où vient le vent
    bool jour = false;
};

// Prévisions Météo-France (via Open-Meteo) + seeing et transparence (7Timer!).
struct Meteo {
    bool ok = false;
    int n = 0;
    Heure h[NB_HEURES];
    int decalageUtc = 3600;            // secondes, fourni par Open-Meteo (heure d'été comprise)
    time_t recuLe = 0;

    bool astroOk = false;
    time_t astroInit = 0;              // UTC
    int nAstro = 0;
    int astroHeure[NB_ASTRO] = {};     // heures après astroInit
    int seeing[NB_ASTRO] = {};         // 1 (excellent) à 8 (mauvais)
    int transparence[NB_ASTRO] = {};   // 1 (excellente) à 8 (très faible)
};

struct Reglages {
    std::string ville = "Paris";
    double lat = 48.86;
    double lon = 2.35;
};

// État de l'interface.
struct EtatEcran {
    int page = 0;              // 0 accueil, 1 compteur, 2 météo
    bool alerte = false;       // page 2 en mode « nouvelle exoplanète »
    bool clignote = false;     // phase du clignotement de l'alerte
    bool wifiOk = false;
};
