#include "meteo_analyse.h"

#include <cmath>
#include <cstdio>

#include "astro.h"

std::string descriptionMeteo(int code, int nuages) {
    if (code >= 95) return "Orages";
    if (code == 85 || code == 86) return "Averses de neige";
    if (code >= 80) return "Averses";
    if (code >= 71 && code <= 77) return "Neige";
    if (code == 66 || code == 67) return "Pluie verglaçante";
    if (code >= 61) return "Pluie";
    if (code == 56 || code == 57) return "Bruine verglaçante";
    if (code >= 51) return "Bruine";
    if (code == 45 || code == 48) return "Brouillard";
    if (nuages < 15) return "Ciel dégagé";
    if (nuages < 40) return "Peu nuageux";
    if (nuages < 70) return "Nuageux";
    return "Très nuageux";
}

const char* qualiteSeeing(int s) {
    static const char* const t[] = {"–", "Excellent", "Bon", "Bon", "Moyen", "Moyen", "Médiocre", "Mauvais", "Mauvais"};
    return (s >= 1 && s <= 8) ? t[s] : t[0];
}

const char* qualiteTransparence(int s) {
    static const char* const t[] = {"–", "Excellente", "Excellente", "Bonne", "Moyenne", "Moyenne", "Faible", "Faible", "Très faible"};
    return (s >= 1 && s <= 8) ? t[s] : t[0];
}

const char* arcsecSeeing(int s) {
    static const char* const t[] = {"–", "0,4″", "0,6″", "0,9″", "1,1″", "1,4″", "1,8″", "2,2″", "2,6″"};
    return (s >= 1 && s <= 8) ? t[s] : t[0];
}

const char* directionVent(int d) {
    static const char* const t[] = {"N", "NE", "E", "SE", "S", "SO", "O", "NO"};
    int i = static_cast<int>(std::lround(((d % 360) + 360) % 360 / 45.0)) % 8;
    return t[i];
}

namespace {

int indexAstro(const Meteo& m, time_t utc) {
    if (!m.astroOk) return -1;
    int meilleur = -1;
    long ecart = 2 * 3600 + 1;
    for (int i = 0; i < m.nAstro; ++i) {
        long e = std::labs(static_cast<long>(m.astroInit + m.astroHeure[i] * 3600L - utc));
        if (e < ecart) {
            ecart = e;
            meilleur = i;
        }
    }
    return meilleur;
}

int heureLoc(time_t utc, int dec) { return heureLocale(utc, dec).tm_hour; }

// Gravité d'un code météo, pour choisir le pictogramme dominant d'une période.
int gravite(int code) {
    if (code >= 95) return 9;
    if (code >= 71 && code <= 77) return 8;
    if (code == 85 || code == 86) return 8;
    if (code >= 61 && code <= 67) return 7;
    if (code >= 80) return 6;
    if (code >= 51) return 5;
    if (code == 45 || code == 48) return 4;
    return code;  // 0 à 3
}

}  // namespace

int seeingA(const Meteo& m, time_t utc) {
    int i = indexAstro(m, utc);
    return i < 0 ? 0 : m.seeing[i];
}

int transparenceA(const Meteo& m, time_t utc) {
    int i = indexAstro(m, utc);
    return i < 0 ? 0 : m.transparence[i];
}

ResumeNuit resumerNuit(const Meteo& m, time_t maintenant) {
    ResumeNuit r;
    if (!m.ok || m.n == 0) return r;

    // Fenêtre : de 21 h à 5 h (heure locale). Si on est déjà dans la nuit, on part de maintenant.
    const int dec = m.decalageUtc;
    struct tm lt = heureLocale(maintenant, dec);
    time_t minuitLocal = maintenant - (lt.tm_hour * 3600 + lt.tm_min * 60 + lt.tm_sec);
    time_t debut, fin;
    if (lt.tm_hour < 5) {
        debut = maintenant - 3600;
        fin = minuitLocal + 5 * 3600;
    } else {
        debut = minuitLocal + 21 * 3600;
        fin = minuitLocal + 29 * 3600;
        if (maintenant > debut) debut = maintenant - 3600;
    }

    int n = 0, sommeNuages = 0, codeMax = 0, nuagesMin = 101, heureMin = -1, premiereAverse = -1;
    float pluie = 0, tMin = 99, vMax = 0;
    for (int i = 0; i < m.n; ++i) {
        const Heure& h = m.h[i];
        if (h.t < debut || h.t > fin) continue;
        ++n;
        sommeNuages += h.nuages;
        pluie += h.pluie;
        if (h.temp < tMin) tMin = h.temp;
        if (h.vent > vMax) vMax = h.vent;
        if (gravite(h.code) > gravite(codeMax)) codeMax = h.code;
        if (h.nuages < nuagesMin) {
            nuagesMin = h.nuages;
            heureMin = heureLoc(h.t, dec);
        }
        if (premiereAverse < 0 && h.pluie >= 0.2f) premiereAverse = heureLoc(h.t, dec);
    }
    if (n == 0) return r;

    const int moyNuages = sommeNuages / n;
    r.ok = true;
    r.nuages = moyNuages;
    r.tempMin = static_cast<int>(std::lround(tMin));
    r.ventMax = static_cast<int>(std::lround(vMax));
    r.code = pluie >= 0.5f ? codeMax : (moyNuages < 15 ? 0 : moyNuages < 40 ? 1 : moyNuages < 70 ? 2 : 3);
    r.description = descriptionMeteo(r.code, moyNuages);
    r.meilleureHeure = nuagesMin <= 40 ? heureMin : -1;

    char buf[64];
    if (moyNuages < 25 && pluie < 0.2f) {
        r.detail = "Belle nuit pour observer";
    } else if (moyNuages >= 40 && nuagesMin <= 40) {
        snprintf(buf, sizeof buf, "%sclaircie vers %d h", pluie >= 0.5f ? "Averses, é" : "É", heureMin);
        r.detail = buf;
    } else if (premiereAverse >= 0) {
        snprintf(buf, sizeof buf, "Pluie à partir de %d h", premiereAverse);
        r.detail = buf;
    } else {
        r.detail = "Peu d'éclaircies";
    }

    // Seeing : le meilleur de la nuit, avec l'heure.
    if (m.astroOk) {
        int meilleur = 9;
        for (int i = 0; i < m.nAstro; ++i) {
            time_t t = m.astroInit + m.astroHeure[i] * 3600L;
            if (t < debut || t > fin) continue;
            if (m.seeing[i] >= 1 && m.seeing[i] < meilleur) {
                meilleur = m.seeing[i];
                r.seeing = m.seeing[i];
                r.transparence = m.transparence[i];
                r.seeingHeure = heureLoc(t, dec);
            }
        }
    }
    return r;
}

int choisirColonnes(const Meteo& m, time_t maintenant, int idx[8]) {
    int nb = 0;
    // Première colonne : la prochaine heure pleine.
    int premier = -1;
    for (int i = 0; i < m.n; ++i) {
        if (m.h[i].t >= maintenant - 1800) {
            premier = i;
            break;
        }
    }
    if (premier < 0) return 0;
    for (int i = premier; i < m.n && nb < 8; i += 3) idx[nb++] = i;
    return nb;
}
