#include "astro.h"

#include <cmath>

const char* const JOURS_COURTS[7] = {"dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."};
const char* const MOIS_COURTS[12] = {"janv.", "févr.", "mars", "avr.", "mai", "juin",
                                     "juil.", "août", "sept.", "oct.", "nov.", "déc."};
const char* const MOIS_LONGS[12] = {"janvier", "février", "mars", "avril", "mai", "juin",
                                    "juillet", "août", "septembre", "octobre", "novembre", "décembre"};

namespace {

constexpr double DEG = M_PI / 180.0;

double norm360(double x) {
    x = std::fmod(x, 360.0);
    return x < 0 ? x + 360.0 : x;
}

// Angle de phase i (degrés) et élongation D, d'après Meeus, chap. 48 (version courte).
void phase(time_t utc, double& eclairement, bool& croissante) {
    const double jd = utc / 86400.0 + 2440587.5;
    const double T = (jd - 2451545.0) / 36525.0;
    const double D = norm360(297.8501921 + 445267.1114034 * T);
    const double M = norm360(357.5291092 + 35999.0502909 * T);
    const double Mp = norm360(134.9633964 + 477198.8675055 * T);
    const double i = 180.0 - D
        - 6.289 * std::sin(Mp * DEG)
        + 2.100 * std::sin(M * DEG)
        - 1.274 * std::sin((2 * D - Mp) * DEG)
        - 0.658 * std::sin(2 * D * DEG)
        - 0.214 * std::sin(2 * Mp * DEG)
        - 0.110 * std::sin(D * DEG);
    eclairement = (1.0 + std::cos(i * DEG)) / 2.0;
    croissante = D < 180.0;
}

long jourLocal(time_t utc, int decalage) {
    return static_cast<long>(std::floor((utc + decalage) / 86400.0));
}

}  // namespace

struct tm heureLocale(time_t utc, int decalageUtc) {
    time_t t = utc + decalageUtc;
    struct tm r {};
    gmtime_r(&t, &r);
    return r;
}

Lune calculerLune(time_t utc, int decalageUtc) {
    Lune l;
    phase(utc, l.eclairement, l.croissante);

    // Recherche heure par heure du prochain maximum et du prochain minimum d'éclairement.
    double prec = l.eclairement;
    bool montant = l.croissante;
    bool pleineTrouvee = false, nouvelleTrouvee = false;
    for (int h = 1; h <= 24 * 31 && !(pleineTrouvee && nouvelleTrouvee); ++h) {
        double k;
        bool c;
        phase(utc + h * 3600, k, c);
        if (montant && k < prec) {  // on vient de passer le maximum
            if (!pleineTrouvee) {
                l.joursAvantPleine = static_cast<int>(jourLocal(utc + (h - 1) * 3600, decalageUtc) - jourLocal(utc, decalageUtc));
                pleineTrouvee = true;
            }
            montant = false;
        } else if (!montant && k > prec) {  // on vient de passer le minimum
            if (!nouvelleTrouvee) {
                l.joursAvantNouvelle = static_cast<int>(jourLocal(utc + (h - 1) * 3600, decalageUtc) - jourLocal(utc, decalageUtc));
                nouvelleTrouvee = true;
            }
            montant = true;
        }
        prec = k;
    }
    return l;
}

std::string nomPhase(const Lune& l) {
    const double k = l.eclairement;
    // La veille de la pleine lune, l'éclairement dépasse déjà 97 % : on attend le jour J.
    if (k < 0.03 && (l.croissante || l.joursAvantNouvelle == 0 || k < 0.005)) return "Nouvelle lune";
    if (k > 0.97 && (!l.croissante || l.joursAvantPleine == 0 || k > 0.995)) return "Pleine lune";
    if (l.croissante) {
        if (k < 0.45) return "Premier croissant";
        if (k < 0.55) return "Premier quartier";
        return "Gibbeuse croissante";
    }
    if (k < 0.45) return "Dernier croissant";
    if (k < 0.55) return "Dernier quartier";
    return "Gibbeuse décroissante";
}
