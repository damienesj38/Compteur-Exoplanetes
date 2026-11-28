// Simulateur : dessine les pages de l'écran sur ordinateur et les enregistre en
// images brutes RGB565 (converties en PNG par sim.py). Sert à vérifier la mise en
// page sans la carte.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>

#include <cmath>
#include <cstdio>
#include <string>

#include "../../src/model.h"
#include "../../src/pages.h"
#include "../../src/polices.h"

static void enregistrer(lgfx::LGFX_Sprite& s, const char* nom) {
    std::string chemin = std::string("out/") + nom + ".rgb565";
    FILE* f = fopen(chemin.c_str(), "wb");
    for (int y = 0; y < 480; ++y)
        for (int x = 0; x < 800; ++x) {
            uint16_t p = s.readPixel(x, y);
            fwrite(&p, 2, 1, f);
        }
    fclose(f);
}

int main() {
    initPolices();
    lgfx::LGFX_Sprite c;
    c.setColorDepth(16);
    c.createSprite(800, 480);

    const time_t maintenant = 1795424100;  // 23/11/2026 08:55 UTC (09:55 à Paris)

    // data.json réel du 23/11/2026 (compteur, chiffre du jour, lancements)
    DonneesCompteur d;
    d.ok = true;
    d.total = 8540;
    d.majCatalogue = "2026-11-21";
    d.nouvelles24h = 1;
    d.chiffreValeur = "99,86";
    d.chiffreUnite = "%";
    d.chiffreLegende = "Part de la masse du Système solaire dans le Soleil";
    auto L = [&](int i, const char* f, const char* m, const char* lieu, const char* st, time_t t) {
        d.lancements[i] = {f, m, lieu, st, t};
    };
    L(0, "Soyuz 2.1b", "Progress MS-36 (97P)", "Baïkonour", "À confirmer", 1795515180);
    L(1, "Ariane 64", "Amazon Leo (LE-04)", "Kourou", "À confirmer", 1795518540);
    L(2, "Falcon 9", "USSF-266", "Vandenberg", "Feu vert", 1795535700);
    d.nbLancements = 3;

    Reglages r;
    r.ville = "Lyon";

    // Météo : seeing/transparence réels de 7Timer (prévision du 23/11 00 h UTC) ;
    // valeurs Météo-France illustratives (non récupérables depuis l'atelier).
    Meteo m;
    m.ok = true;
    m.decalageUtc = 3600;
    m.n = 48;
    const time_t t0 = 1795392000;  // 23/11/2026 00:00 UTC
    const int nuagesTp[] = {97, 97, 97, 97, 88, 75, 62, 62, 88, 97, 97};  // tous les 3 h depuis 00 UTC
    for (int i = 0; i < 48; ++i) {
        Heure& h = m.h[i];
        h.t = t0 + i * 3600;
        int loc = (i + 1) % 24;
        h.jour = loc >= 8 && loc < 17;
        float k = i / 3.0f;
        int a = std::min(10, static_cast<int>(k)), b = std::min(10, a + 1);
        h.nuages = static_cast<int>(nuagesTp[a] + (nuagesTp[b] - nuagesTp[a]) * (k - a));
        h.temp = 4.0f + 4.5f * std::sin((loc - 9) * M_PI / 12.0f) - (i > 24 ? 1.0f : 0.0f);
        h.pluie = (i >= 9 && i <= 17) || (i >= 23 && i <= 25) ? 0.6f : 0.0f;
        h.code = h.pluie > 0 ? 61 : (h.nuages > 85 ? 3 : 2);
        h.vent = 8 + (i % 7);
        h.ventDir = i < 15 ? 330 : 100;
    }
    m.astroOk = true;
    m.astroInit = t0;
    const int tp[] = {3, 6, 9, 12, 15, 18, 21, 24, 27, 30}, see[] = {4, 5, 2, 2, 3, 5, 2, 2, 3, 4},
              tr[] = {4, 4, 4, 3, 4, 8, 8, 4, 3, 3};
    m.nAstro = 10;
    for (int i = 0; i < 10; ++i) {
        m.astroHeure[i] = tp[i];
        m.seeing[i] = see[i];
        m.transparence[i] = tr[i];
    }

    EtatEcran e;
    e.wifiOk = true;

    pageAccueil(c, d, m, r, maintenant, e);
    enregistrer(c, "page1_accueil");
    pageCompteur(c, d, m, maintenant, e);
    enregistrer(c, "page2_compteur");
    e.alerte = true;
    e.clignote = true;
    pageCompteur(c, d, m, maintenant, e);
    enregistrer(c, "page2_alerte");
    pageMeteo(c, m, r, maintenant);
    enregistrer(c, "page3_meteo");
    ecranConfiguration(c, "Compteur-Exoplanetes");
    enregistrer(c, "ecran_configuration");
    ecranReinitialisation(c);
    enregistrer(c, "reinit_tmp");
    {
        PrefsUtilisateur pu;
        ecranReglages(c, pu, "n°1 · 21 MHz", false,
                      "Version 1.1.0 · carte V1.3 à V1.5 · Wi-Fi « Livebox-1234 » · Lyon",
                      "Données reçues il y a 4 min · mises à jour automatiques du programme activées");
        enregistrer(c, "reglages");
        pu.lumNuit = 0; pu.alerte = false; pu.nuitDebut = pu.nuitFin = 7;
        ecranReglages(c, pu, "n°2 · 24 MHz", true, "Version 1.1.0", "Données pas encore reçues");
        enregistrer(c, "reglages2");
    }
    ecranReinitialisation(c);
    enregistrer(c, "ecran_reinitialisation");
    ecranMiseAJour(c, 42, "1.0.1");
    enregistrer(c, "ecran_mise_a_jour");
    // Cas limites : données absentes
    DonneesCompteur vide;
    Meteo sansMeteo;
    e.alerte = false;
    pageAccueil(c, vide, sansMeteo, r, maintenant, e);
    enregistrer(c, "page1_sans_donnees");
    puts("ok");
    return 0;
}
