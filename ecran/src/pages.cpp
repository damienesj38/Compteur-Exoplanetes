#include "pages.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "astro.h"
#include "meteo_analyse.h"

namespace {

std::string fmt(const char* f, int a) {
    char b[64];
    snprintf(b, sizeof b, f, a);
    return b;
}

// « −3 °C » avec un vrai signe moins.
std::string temperature(float t) {
    int v = static_cast<int>(std::lround(t));
    char b[24];
    if (v < 0) snprintf(b, sizeof b, "\xE2\x88\x92%d °C", -v);
    else snprintf(b, sizeof b, "%d °C", v);
    return b;
}

std::string dateCatalogue(const std::string& iso) {
    if (iso.size() < 10) return iso;
    return iso.substr(8, 2) + "/" + iso.substr(5, 2) + "/" + iso.substr(0, 4);
}

uint32_t couleurSeeing(int s) {
    if (s <= 0) return coul::CASE;
    if (s <= 2) return 0x1F9D6A;
    if (s == 3) return 0x5FA83F;
    if (s == 4) return 0xB8921E;
    if (s == 5) return 0xD06F2F;
    return 0xB03A2E;
}

uint32_t couleurTransparence(int t) {
    if (t <= 0) return coul::CASE;
    if (t <= 3) return 0x2A62C9;
    if (t <= 5) return 0x2F6FA8;
    if (t <= 7) return 0x3A4250;
    return 0x2A2F38;
}

uint32_t couleurQualite(int s) {  // pour les mots « Bon », « Faible »…
    if (s <= 0) return coul::GRIS;
    if (s <= 3) return coul::VERT;
    if (s <= 5) return coul::JAUNE;
    return coul::ORANGE;
}

const char* transparenceCourte(int t) {
    if (t <= 0) return "–";
    if (t <= 2) return "Excell.";
    if (t == 3) return "Bonne";
    if (t <= 5) return "Moy.";
    if (t <= 7) return "Faible";
    return "Très faible";
}

std::string texteLune(const Lune& l) {
    if (l.croissante) {
        if (l.joursAvantPleine == 0) return "PLEINE LUNE AUJOURD'HUI";
        if (l.joursAvantPleine == 1) return "PLEINE LUNE DEMAIN";
        return fmt("PLEINE LUNE DANS %d J", l.joursAvantPleine);
    }
    if (l.joursAvantNouvelle == 0) return "NOUVELLE LUNE AUJOURD'HUI";
    if (l.joursAvantNouvelle == 1) return "NOUVELLE LUNE DEMAIN";
    return fmt("NOUVELLE LUNE DANS %d J", l.joursAvantNouvelle);
}

std::string texteLuneMinuscules(const Lune& l) {
    if (l.croissante) {
        if (l.joursAvantPleine == 0) return "pleine lune aujourd'hui";
        if (l.joursAvantPleine == 1) return "pleine lune demain";
        return fmt("pleine lune dans %d jours", l.joursAvantPleine);
    }
    if (l.joursAvantNouvelle == 0) return "nouvelle lune aujourd'hui";
    if (l.joursAvantNouvelle == 1) return "nouvelle lune demain";
    return fmt("nouvelle lune dans %d jours", l.joursAvantNouvelle);
}

void puce(Canevas& c, int x, int y, int w, int h, const std::string& lib, const std::string& val, uint32_t col,
          const std::string& sous) {
    c.fillSmoothRoundRect(x, y, w, h, 6, C(coul::CASE));
    etiquette(c, lib, x + 9, y + 7, B12, coul::GRIS, 1);
    texte(c, val, x + 9, y + 21, B18, col);
    if (!sous.empty()) texteLimite(c, sous, x + 9, y + 44, w - 14, B12, coul::GRIS_FONCE);
}

void illustrationSysteme(Canevas& c, int x, int y) {
    // Boîte 300 × 112 : étoile, orbite pointillée, planète rocheuse, planète à anneau.
    c.setClipRect(x, y, 300, 112);
    ciel(c, x, y, 300, 112, 9, 7);
    const int ex = x + 44, ey = y + 58;
    for (int i = 0; i < 140; ++i) {
        float a = i * 2 * M_PI / 140;
        if (i % 2) continue;
        c.fillCircle(ex + std::cos(a) * 250, ey + std::sin(a) * 38, 1, C(0x24405F));
    }
    etoile(c, ex, ey, 24, coul::FOND);
    planete(c, x + 138, y + 91, 10, 0x9FF7D8, 0x2F9FD6, 0x173F78);
    planeteAnneau(c, x + 236, y + 50, 22, 0xF3B8FF, 0xA45CF0, 0x4B2387, 0xF2DDFF, coul::FOND);
    c.clearClipRect();
}

const Lancement* prochainLancement(const DonneesCompteur& d, time_t maintenant) {
    for (int i = 0; i < d.nbLancements; ++i)
        if (d.lancements[i].net + 1800 > maintenant) return &d.lancements[i];
    return nullptr;
}

}  // namespace

// ============================================================== page 1 : accueil

void pageAccueil(Canevas& c, const DonneesCompteur& d, const Meteo& m, const Reglages& r, time_t maintenant,
                 const EtatEcran& e) {
    c.fillScreen(C(coul::FOND));
    c.drawFastVLine(229, 0, 480, C(coul::LIGNE));
    c.drawFastVLine(569, 0, 480, C(coul::LIGNE));
    c.drawFastHLine(0, 329, 800, C(coul::LIGNE));
    const int dec = m.decalageUtc;
    struct tm lt = heureLocale(maintenant, dec);

    // --- Ciel cette nuit
    etiquette(c, "CIEL CETTE NUIT", 18, 18, B15, coul::BLEU);
    ResumeNuit rn = resumerNuit(m, maintenant);
    if (!rn.ok) {
        texte(c, "Météo indisponible", 18, 60, S17, coul::GRIS);
        texteMultiligne(c, "Nouvel essai dans quelques minutes.", 18, 86, 194, S14, coul::GRIS_FONCE, 17, 3);
    } else {
        picto(c, 54, 84, 66, rn.code, rn.nuages, false);
        int h = texteMultiligne(c, rn.description, 100, 52, 116, S21, coul::BLANC, 22, 2);
        texteMultiligne(c, rn.detail, 100, 54 + h, 116, S14, coul::GRIS, 16, 3);

        const int x1 = 18, x2 = 119, y1 = 140, y2 = 212, w = 93, hh = 64;
        char b[48];
        if (rn.seeing > 0) snprintf(b, sizeof b, "%s à %d h", arcsecSeeing(rn.seeing), rn.seeingHeure);
        puce(c, x1, y1, w, hh, "SEEING", qualiteSeeing(rn.seeing), couleurQualite(rn.seeing),
             rn.seeing > 0 ? b : "indisponible");
        std::string sousT = rn.transparence > 0 ? fmt("à %d h", rn.seeingHeure) : "indisponible";
        puce(c, x2, y1, w, hh, "TRANSP.", qualiteTransparence(rn.transparence), couleurQualite(rn.transparence), sousT);
        puce(c, x1, y2, w, hh, "TEMP.", temperature(rn.tempMin), coul::BLANC, "minimale");
        const char* vent = rn.ventMax < 10 ? "Faible" : rn.ventMax < 25 ? "Modéré" : rn.ventMax < 40 ? "Fort" : "Très fort";
        puce(c, x2, y2, w, hh, "VENT", vent, rn.ventMax < 25 ? coul::BLANC : coul::ORANGE, fmt("max %d km/h", rn.ventMax));
    }

    // --- Compteur
    std::string lib = majuscules(d.libelle);
    int esp = largeurEtiquette(c, lib, B15, 2) > 270 ? 1 : 2;
    etiquette(c, lib, 248, 18, B15, coul::CYAN, esp);
    iconeWifi(c, 530, 16, e.wifiOk ? coul::VERT : coul::ORANGE);
    illustrationSysteme(c, 250, 48);
    std::string total = d.total == VALEUR_ABSENTE ? "----" : std::to_string(d.total);
    int W = 56, H = 100, E = 12;
    if (total.size() > 4) { W = 46; H = 90; E = 11; }
    int lw = segments(c, total, 0, 0, W, H, E, 0, true, true, false);
    segments(c, total, 400 - lw / 2, 192 + (100 - H) / 2, W, H, E, coul::BLANC_BLEU);

    // --- Lune
    etiquette(c, "PHASE DE LA LUNE", 588, 18, B15, coul::JAUNE);
    Lune l = calculerLune(maintenant, dec);
    lune(c, 685, 104, 52, l.eclairement, l.croissante, r.lat < 0);
    std::string pct = std::to_string(static_cast<int>(std::lround(l.eclairement * 100)));
    int pw = segments(c, pct, 0, 0, 40, 68, 9, 0, true, true, false);
    c.setFont(police(B24));
    int tw = pw + 6 + c.textWidth("%");
    segments(c, pct, 685 - tw / 2, 172, 40, 68, 9, coul::JAUNE);
    texte(c, "%", 685 - tw / 2 + pw + 6, 214, B24, coul::JAUNE);
    etiquette(c, texteLune(l), 685, 254, B15, coul::JAUNE, 1, CENTRE);
    texte(c, nomPhase(l), 685, 278, S14, coul::GRIS, CENTRE);

    // --- Chiffre du jour
    iconeHorloge(c, 18, 346, coul::VIOLET);
    etiquette(c, "CHIFFRE DU JOUR", 42, 347, B15, coul::VIOLET);
    if (d.chiffreValeur.empty()) {
        texte(c, "Indisponible", 18, 392, S17, coul::GRIS);
    } else {
        int cw = 30, ch = 50, ce = 7;
        int largeur = segments(c, d.chiffreValeur, 0, 0, cw, ch, ce, 0, true, true, false);
        if (largeur > 118) {
            float k = 118.0f / largeur;
            cw = std::max(14, static_cast<int>(cw * k));
            ch = std::max(26, static_cast<int>(ch * k));
            ce = std::max(4, static_cast<int>(ce * k));
            largeur = segments(c, d.chiffreValeur, 0, 0, cw, ch, ce, 0, true, true, false);
        }
        segments(c, d.chiffreValeur, 18, 378 + (50 - ch), cw, ch, ce, coul::VIOLET);
        int ux = 18 + largeur + 10;
        texteMultiligne(c, d.chiffreUnite, ux, 394, 212 - ux, S16, 0xE9DCFF, 17, 2);
        texteMultiligne(c, d.chiffreLegende, 18, 438, 194, S14, coul::GRIS, 16, 2);  // 2 lignes depuis 1.0.5
    }

    // --- Heure, date, ville
    char hm[8];
    snprintf(hm, sizeof hm, "%02d:%02d", lt.tm_hour, lt.tm_min);
    int hw = segments(c, hm, 0, 0, 36, 62, 8, 0, true, true, false);
    segments(c, hm, 400 - hw / 2, 346, 36, 62, 8, coul::BLANC);
    char dt[96];
    snprintf(dt, sizeof dt, "%s %d %s %d · %s", JOURS_COURTS[lt.tm_wday], lt.tm_mday, MOIS_COURTS[lt.tm_mon],
             lt.tm_year + 1900, r.ville.c_str());
    std::string ligneDate = majuscules(dt);
    int espD = largeurEtiquette(c, ligneDate, B15, 2) > 320 ? 0 : 2;
    if (largeurEtiquette(c, ligneDate, B15, espD) > 330) {
        snprintf(dt, sizeof dt, "%s %d %s %d", JOURS_COURTS[lt.tm_wday], lt.tm_mday, MOIS_COURTS[lt.tm_mon], lt.tm_year + 1900);
        ligneDate = majuscules(dt);
    }
    etiquette(c, ligneDate, 400, 422, B15, coul::VERT, espD, CENTRE);
    pointsPage(c, 400, 456, 0);
    iconeEngrenage(c, ENGRENAGE_X, ENGRENAGE_Y, coul::GRIS_FONCE, coul::FOND);

    // --- Prochain lancement
    iconeFusee(c, 588, 344, coul::CORAIL);
    etiquette(c, "PROCHAIN LANCEMENT", 612, 347, B15, coul::CORAIL, 1);
    const Lancement* la = prochainLancement(d, maintenant);
    if (!la) {
        texteMultiligne(c, "Aucun lancement connu pour l'instant", 588, 388, 194, S16, coul::GRIS, 18, 2);
    } else {
        long s = static_cast<long>(la->net - maintenant);
        if (s <= 0) {
            etiquette(c, "DÉCOLLAGE !", 588, 382, B24, coul::ORANGE, 2);
        } else {
            char seg[16];
            const char* u1;
            const char* u2;
            if (s < 100 * 3600L) {
                snprintf(seg, sizeof seg, "%ld:%02ld", s / 3600, (s % 3600) / 60);
                u1 = "H";
                u2 = "MIN";
            } else {
                snprintf(seg, sizeof seg, "%ld", s / 86400);
                u1 = "JOURS";
                u2 = "";
            }
            int sw = segments(c, seg, 588, 375, 26, 44, 6, coul::CORAIL);
            etiquette(c, u1, 588 + sw + 10, *u2 ? 388 : 398, B12, 0xFFB8A6, 1);
            if (*u2) etiquette(c, u2, 588 + sw + 10, 402, B12, 0xFFB8A6, 1);
        }
        std::string nom = la->fusee + " · " + la->mission;
        texteLimite(c, nom, 588, 428, 196, S16, coul::BLANC);
        struct tm ln = heureLocale(la->net, dec);
        char lieu[96];
        snprintf(lieu, sizeof lieu, "%s · %s %02d/%02d à %02d:%02d", la->lieu.c_str(), JOURS_COURTS[ln.tm_wday],
                 ln.tm_mday, ln.tm_mon + 1, ln.tm_hour, ln.tm_min);
        texteLimite(c, lieu, 588, 450, 196, S14, coul::GRIS);
    }
}

// ============================================================== page 2 : compteur

void pageCompteur(Canevas& c, const DonneesCompteur& d, const Meteo& m, time_t maintenant, const EtatEcran& e) {
    c.fillScreen(C(coul::FOND));
    c.drawFastHLine(0, 63, 800, C(coul::LIGNE));
    c.drawFastHLine(0, 423, 800, C(coul::LIGNE));

    if (e.alerte) {
        uint32_t col = e.clignote ? coul::ORANGE : melange(coul::FOND, coul::ORANGE, 0.35f);
        etiquette(c, "NOUVELLE EXOPLANÈTE !", 24, 18, B24, col, 3);
    } else {
        etiquette(c, "COMPTEUR D'EXOPLANÈTES", 24, 18, B24, coul::CYAN, 3);
    }
    c.setFont(police(S16));
    int sw = c.textWidth("exoplanet.eu");
    texte(c, "exoplanet.eu", 776, 22, S16, coul::GRIS, DROITE);
    c.fillSmoothCircle(776 - sw - 14, 31, 6, C(e.alerte ? coul::ORANGE : coul::VERT));

    // Illustration
    const int px = 157, py = 244;
    ciel(c, 20, 80, 270, 330, 14, 3);
    for (int i = 0; i < 8; ++i) {
        float a = i * M_PI / 4;
        c.drawWideLine(px + std::cos(a) * 112, py + std::sin(a) * 112, px + std::cos(a) * 128, py + std::sin(a) * 128,
                       1.6f, C(e.alerte ? coul::ORANGE : melange(coul::FOND, coul::ORANGE, 0.45f)));
    }
    for (int i = 0; i < 10; ++i) c.fillCircle(px, py, 96 - i * 4, C(melange(coul::FOND, coul::CYAN, 0.02f + i * 0.012f)));
    planeteAnneau(c, px, py, 56, 0xBFF4FF, 0x3AA6E8, 0x16306E, 0xFFE6C4, coul::FOND);

    // Variation depuis hier
    const int x0 = 326;
    std::string delta = d.nouvelles24h == VALEUR_ABSENTE ? "--" : ((d.nouvelles24h >= 0 ? "+" : "-") + std::to_string(std::abs(d.nouvelles24h)));
    int dw = segments(c, delta, x0, 112, 34, 56, 8, coul::VERT);
    etiquette(c, d.nouvelles24h == VALEUR_ABSENTE ? "HISTORIQUE EN COURS" : "DEPUIS HIER", x0 + dw + 16, 131, B18,
              coul::VERT, 2);

    std::string total = d.total == VALEUR_ABSENTE ? "----" : std::to_string(d.total);
    int W = total.size() > 4 ? 64 : 72;
    segments(c, total, x0, 194, W, 130, 15, e.alerte ? coul::ORANGE : coul::BLANC_BLEU);
    etiquette(c, majuscules(d.libelle), x0, 350, B18, coul::CYAN, 2);

    // Pied de page
    if (!d.majCatalogue.empty())
        etiquette(c, "CATALOGUE MIS À JOUR LE " + dateCatalogue(d.majCatalogue), 24, 444, B15, coul::GRIS, 1);
    pointsPage(c, 400, 452, 1);
    struct tm lt = heureLocale(maintenant, m.decalageUtc);
    char hm[8];
    snprintf(hm, sizeof hm, "%02d:%02d", lt.tm_hour, lt.tm_min);
    texte(c, hm, 776, 441, B18, coul::VERT, DROITE);
}

// ============================================================== page 3 : météo

void pageMeteo(Canevas& c, const Meteo& m, const Reglages& r, time_t maintenant) {
    c.fillScreen(C(coul::FOND));
    c.drawFastHLine(0, 53, 800, C(coul::LIGNE));
    c.drawFastHLine(0, 423, 800, C(coul::LIGNE));
    soleil(c, 30, 22, 6);
    nuage(c, 38, 30, 24, coul::NUAGE);
    std::string titre = "MÉTÉO · " + majuscules(r.ville);
    etiquette(c, titre, 58, 15, B24, coul::BLEU, 2);
    pointsPage(c, 748, 440, 2);
    texte(c, "Météo-France (AROME) via Open-Meteo · seeing 7Timer!", 782, 456, B12, coul::GRIS_FONCE, DROITE);

    int idx[8];
    int nb = m.ok ? choisirColonnes(m, maintenant, idx) : 0;
    if (nb == 0) {
        texte(c, "Météo indisponible pour le moment", 400, 200, S21, coul::GRIS, CENTRE);
        texte(c, "Nouvel essai dans quelques minutes.", 400, 232, S16, coul::GRIS_FONCE, CENTRE);
        return;
    }
    const int dec = m.decalageUtc;

    // Dates couvertes
    struct tm a = heureLocale(m.h[idx[0]].t, dec), b = heureLocale(m.h[idx[nb - 1]].t, dec);
    char per[96];
    if (a.tm_mday == b.tm_mday)
        snprintf(per, sizeof per, "%s %d %s", JOURS_COURTS[a.tm_wday], a.tm_mday, MOIS_LONGS[a.tm_mon]);
    else
        snprintf(per, sizeof per, "du %s %d au %s %d %s", JOURS_COURTS[a.tm_wday], a.tm_mday, JOURS_COURTS[b.tm_wday],
                 b.tm_mday, MOIS_LONGS[b.tm_mon]);
    std::string ps = per;
    ps[0] = static_cast<char>(ps[0] - 32);
    texte(c, ps, 782, 17, S16, coul::GRIS, DROITE);

    // Grille
    const float x0 = 18 + 104 + 6, gap = 6, cw = (782 - x0 - 7 * gap) / 8.0f;
    const int rowsY[6] = {90, 144, 201, 258, 315, 372};
    const int rh = 51;
    const char* lib[6] = {"Ciel", "Température", "Nuages", "Vent", "Seeing", "Transparence"};
    const char* sous[6] = {"", "", "couverture", "direction", "la nuit", "la nuit"};
    for (int i = 0; i < 6; ++i) {
        int h = i == 0 ? 48 : rh;
        bool deux = sous[i][0] != 0;
        texte(c, lib[i], 18, rowsY[i] + h / 2 - (deux ? 17 : 9), S16, coul::NUAGE);
        if (deux) texte(c, sous[i], 18, rowsY[i] + h / 2 + 2, B12, coul::GRIS_FONCE);
    }

    for (int j = 0; j < nb; ++j) {
        const Heure& h = m.h[idx[j]];
        const int x = static_cast<int>(x0 + j * (cw + gap)), w = static_cast<int>(cw), cx = x + w / 2;
        struct tm t = heureLocale(h.t, dec);
        // En-tête : soleil ou lune + heure
        std::string hs = fmt("%02d h", t.tm_hour);
        c.setFont(police(B15));
        int tw = 14 + 4 + c.textWidth(hs.c_str());
        if (h.jour) soleil(c, cx - tw / 2 + 6, 73, 3);
        else petiteLune(c, cx - tw / 2 + 7, 73, 6, coul::JAUNE, coul::FOND);
        texte(c, hs, cx - tw / 2 + 18, 65, B15, coul::GRIS);

        const uint32_t fondCase = h.jour ? coul::CASE_JOUR : coul::CASE_NUIT;
        // Ciel
        if (h.jour) c.fillSmoothRoundRect(x, rowsY[0], w, 48, 6, C(coul::CASE_JOUR));
        picto(c, cx, rowsY[0] + 24, 40, h.code, h.nuages, h.jour);
        // Température
        c.fillSmoothRoundRect(x, rowsY[1], w, rh, 6, C(fondCase));
        texte(c, temperature(h.temp), cx, rowsY[1] + 16, B18, coul::BLANC, CENTRE);
        // Nuages
        uint32_t fn = melange(0x15305A, coul::NUAGE, h.nuages / 100.0f);
        c.fillSmoothRoundRect(x, rowsY[2], w, rh, 6, C(fn));
        texte(c, fmt("%d %%", h.nuages), cx, rowsY[2] + 16, B18, h.nuages >= 55 ? coul::FOND : coul::BLANC, CENTRE);
        // Vent
        c.fillSmoothRoundRect(x, rowsY[3], w, rh, 6, C(fondCase));
        const char* dir = directionVent(h.ventDir);
        c.setFont(police(B15));
        int dw = 12 + 4 + c.textWidth(dir);
        fleche(c, cx - dw / 2 + 6, rowsY[3] + 16, 12, h.ventDir + 180.0f, coul::VERT);
        texte(c, dir, cx - dw / 2 + 16, rowsY[3] + 7, B15, coul::NUAGE);
        texte(c, fmt("%d km/h", static_cast<int>(std::lround(h.vent))), cx, rowsY[3] + 29, B12, coul::GRIS, CENTRE);
        // Seeing et transparence (la nuit seulement)
        if (h.jour) {
            for (int k : {4, 5}) {
                c.fillSmoothRoundRect(x, rowsY[k], w, rh, 6, C(coul::CASE_JOUR));
                texte(c, "–", cx, rowsY[k] + 16, B18, 0x4A5566, CENTRE);
            }
        } else {
            int s = seeingA(m, h.t), tr = transparenceA(m, h.t);
            c.fillSmoothRoundRect(x, rowsY[4], w, rh, 6, C(couleurSeeing(s)));
            texte(c, arcsecSeeing(s), cx, rowsY[4] + 16, B18, coul::BLANC, CENTRE);
            c.fillSmoothRoundRect(x, rowsY[5], w, rh, 6, C(couleurTransparence(tr)));
            texte(c, transparenceCourte(tr), cx, rowsY[5] + 17, B15, coul::BLANC, CENTRE);
        }
    }

    // Pied de page : meilleur créneau et Lune
    ResumeNuit rn = resumerNuit(m, maintenant);
    if (rn.ok && rn.meilleureHeure >= 0)
        etiquette(c, fmt("MEILLEUR CRÉNEAU : VERS %d H", rn.meilleureHeure), 22, 436, B15, coul::VERT, 1);
    else
        etiquette(c, "PAS DE CRÉNEAU DÉGAGÉ CETTE NUIT", 22, 436, B15, coul::ORANGE, 1);
    Lune l = calculerLune(maintenant, dec);
    std::string lc = "Lune " + std::to_string(static_cast<int>(std::lround(l.eclairement * 100))) + " % · " + texteLuneMinuscules(l);
    texte(c, lc, 22, 457, S14, coul::GRIS);
}

// ============================================================== écrans de service

void ecranMessage(Canevas& c, const std::string& titre, const std::vector<std::string>& lignes, uint32_t couleurTitre) {
    c.fillScreen(C(coul::FOND));
    ciel(c, 0, 0, 800, 100, 14, 11);
    ciel(c, 0, 400, 800, 80, 10, 12);
    ciel(c, 0, 100, 120, 300, 8, 13);
    ciel(c, 680, 140, 120, 260, 8, 14);
    planeteAnneau(c, 690, 90, 34, 0xF3B8FF, 0xA45CF0, 0x4B2387, 0xF2DDFF, coul::FOND);
    planete(c, 110, 400, 16, 0x9FF7D8, 0x2F9FD6, 0x173F78);
    texte(c, titre, 400, 120, B28, couleurTitre, CENTRE);
    int y = 180;
    for (auto& l : lignes) {
        y += texteMultiligne(c, l, 140, y, 520, S17, coul::BLANC, 24, 3) + 10;
    }
}

void ecranConfiguration(Canevas& c, const std::string& nomWifi) {
    ecranMessage(c, "Bienvenue !",
                 {"1. Sur ton téléphone, connecte-toi au réseau Wi-Fi « " + nomWifi + " ».",
                  "2. Une page s'ouvre toute seule. Si ce n'est pas le cas, ouvre ton navigateur à l'adresse 192.168.4.1.",
                  "3. Choisis ton Wi-Fi, tape son mot de passe et le nom de ta ville, puis enregistre.",
                  "L'écran redémarre ensuite tout seul."});
}

void ecranReinitialisation(Canevas& c) {
    ecranMessage(c, "Changer de Wi-Fi ou de ville ?",
                 {"L'écran va oublier ton Wi-Fi et ta ville, puis rouvrir la page de configuration."}, coul::ORANGE);
    c.fillSmoothRoundRect(BOUTON_OUI_X, BOUTON_Y, BOUTON_L, BOUTON_H, 10, C(0x7A3A12));
    texte(c, "Oui", BOUTON_OUI_X + BOUTON_L / 2, BOUTON_Y + 18, B24, coul::BLANC, CENTRE);
    c.fillSmoothRoundRect(BOUTON_NON_X, BOUTON_Y, BOUTON_L, BOUTON_H, 10, C(0x1F3A5C));
    texte(c, "Non", BOUTON_NON_X + BOUTON_L / 2, BOUTON_Y + 18, B24, coul::BLANC, CENTRE);
}

void ecranMiseAJour(Canevas& c, int pourcent, const std::string& version) {
    ecranMessage(c, "Mise à jour en cours",
                 {"Nouvelle version " + version + ". Ne débranche pas l'écran, il redémarrera tout seul."});
    c.fillSmoothRoundRect(200, 320, 400, 18, 9, C(coul::CASE));
    if (pourcent > 0) c.fillSmoothRoundRect(200, 320, std::max(18, 4 * std::min(100, pourcent)), 18, 9, C(coul::CYAN));
    texte(c, std::to_string(pourcent) + " %", 400, 350, S17, coul::GRIS, CENTRE);
}

namespace {
void boutonReglage(Canevas& c, int x, int y, int l, int h, const std::string& lib, uint32_t fond, Police p = B24) {
    c.fillSmoothRoundRect(x, y, l, h, 8, C(fond));
    texte(c, lib, x + l / 2, y + (h - (p == B24 ? 24 : 18)) / 2 - 1, p, coul::BLANC, CENTRE);
}
}  // namespace

int zoneReglages(int x, int y) {
    for (int i = 0; i < REG_LIGNES; ++i) {
        const int y0 = REG_Y0 + i * REG_PAS;
        if (y < y0 - 4 || y > y0 + REG_H + 4) continue;
        if (i == 4) return (x >= REG_MOINS_X - 6 && x <= REG_MOINS_X + REG_BASCULE_L + 6) ? Z_ALERTE : Z_AUCUNE;
        if (x >= REG_MOINS_X - 8 && x <= REG_MOINS_X + REG_BTN_L + 4) return Z_MOINS + i;
        if (x >= REG_PLUS_X - 4 && x <= REG_PLUS_X + REG_BTN_L + 8) return Z_PLUS + i;
        return Z_AUCUNE;
    }
    if (y >= REG_BAS_Y - 4 && y <= REG_BAS_Y + REG_BAS_H + 4) {
        if (x >= REG_WIFI_X && x <= REG_WIFI_X + REG_WIFI_L) return Z_WIFI;
        if (x >= REG_RETOUR_X && x <= REG_RETOUR_X + REG_RETOUR_L) return Z_RETOUR;
    }
    return Z_AUCUNE;
}

void ecranReglages(Canevas& c, const PrefsUtilisateur& p, const std::string& nomAffichage, bool redemarrage,
                   const std::string& infos1, const std::string& infos2) {
    c.fillScreen(C(coul::FOND));
    etiquette(c, "RÉGLAGES", 40, 20, B18, coul::CYAN, 3);
    iconeEngrenage(c, 22, 30, coul::CYAN, coul::FOND);
    const bool sansNuit = p.nuitDebut == p.nuitFin;
    auto pct = [](int v) { return std::to_string(v) + " %"; };
    const std::string lignes[REG_LIGNES][2] = {
        {"Luminosité le jour", pct(p.lumJour)},
        {"Luminosité la nuit", p.lumNuit == 0 ? std::string("Éteint") : pct(p.lumNuit)},
        {"Début du mode nuit", sansNuit ? std::string("Aucun") : std::to_string(p.nuitDebut) + " h"},
        {"Fin du mode nuit", sansNuit ? std::string("Aucune") : std::to_string(p.nuitFin) + " h"},
        {"Alerte « nouvelle exoplanète »", ""},
        {"Affichage (si l'image saute)", nomAffichage},
    };
    for (int i = 0; i < REG_LIGNES; ++i) {
        const int y = REG_Y0 + i * REG_PAS;
        c.fillSmoothRoundRect(24, y, 752, REG_H, 8, C(coul::CASE));
        texte(c, lignes[i][0], 40, y + 11, S17, coul::BLANC);
        if (i == 4) {
            boutonReglage(c, REG_MOINS_X, y + 3, REG_BASCULE_L, REG_H - 6, p.alerte ? "Activée" : "Désactivée",
                          p.alerte ? 0x1E6B45 : 0x5A2A2A, S17);
            continue;
        }
        texte(c, lignes[i][1], REG_MOINS_X - 20, y + 9, B24, coul::CYAN, DROITE);
        boutonReglage(c, REG_MOINS_X, y + 3, REG_BTN_L, REG_H - 6, "−", 0x1F3A5C);
        boutonReglage(c, REG_PLUS_X, y + 3, REG_BTN_L, REG_H - 6, "+", 0x1F3A5C);
    }
    if (redemarrage)
        texte(c, "L'écran redémarrera en quittant les réglages.", 40, REG_Y0 + 5 * REG_PAS + REG_H + 2, S14, coul::ORANGE);
    boutonReglage(c, REG_WIFI_X, REG_BAS_Y, REG_WIFI_L, REG_BAS_H, "Changer de Wi-Fi ou de ville", 0x5A3417, S17);
    boutonReglage(c, REG_RETOUR_X, REG_BAS_Y, REG_RETOUR_L, REG_BAS_H, "Retour", 0x1E6B45, S17);
    texteLimite(c, infos1, 40, 434, 720, S14, coul::GRIS);
    texteLimite(c, infos2, 40, 454, 720, S14, coul::GRIS_FONCE);
}
