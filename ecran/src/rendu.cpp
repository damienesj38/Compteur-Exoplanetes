#include "rendu.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

lgfx::rgb888_t C(uint32_t hex) { return lgfx::rgb888_t(hex); }

uint32_t melange(uint32_t a, uint32_t b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto canal = [&](int dec) {
        int x = (a >> dec) & 0xFF, y = (b >> dec) & 0xFF;
        return static_cast<uint32_t>(std::lround(x + (y - x) * t)) << dec;
    };
    return canal(16) | canal(8) | canal(0);
}

// ---------------------------------------------------------------- texte

namespace {

// Découpe une chaîne UTF-8 en caractères.
std::vector<std::string> caracteres(const std::string& s) {
    std::vector<std::string> v;
    for (size_t i = 0; i < s.size();) {
        unsigned char c = s[i];
        size_t n = c < 0x80 ? 1 : (c >> 5) == 6 ? 2 : (c >> 4) == 14 ? 3 : 4;
        v.push_back(s.substr(i, n));
        i += n;
    }
    return v;
}

}  // namespace

int texte(Canevas& c, const std::string& s, int x, int y, Police p, uint32_t couleur, Aligne a) {
    c.setFont(police(p));
    c.setTextColor(C(couleur));
    c.setTextDatum(a == GAUCHE ? textdatum_t::top_left : a == CENTRE ? textdatum_t::top_center : textdatum_t::top_right);
    c.drawString(s.c_str(), x, y);
    return c.textWidth(s.c_str());
}

int largeurEtiquette(Canevas& c, const std::string& s, Police p, int esp) {
    c.setFont(police(p));
    int w = 0;
    auto cs = caracteres(s);
    for (auto& ch : cs) w += c.textWidth(ch.c_str()) + esp;
    return cs.empty() ? 0 : w - esp;
}

int etiquette(Canevas& c, const std::string& s, int x, int y, Police p, uint32_t couleur, int esp, Aligne a) {
    int w = largeurEtiquette(c, s, p, esp);
    int cx = a == GAUCHE ? x : a == CENTRE ? x - w / 2 : x - w;
    c.setFont(police(p));
    c.setTextColor(C(couleur));
    c.setTextDatum(textdatum_t::top_left);
    for (auto& ch : caracteres(s)) {
        c.drawString(ch.c_str(), cx, y);
        cx += c.textWidth(ch.c_str()) + esp;
    }
    return w;
}

void texteLimite(Canevas& c, const std::string& s, int x, int y, int largeur, Police p, uint32_t couleur) {
    c.setFont(police(p));
    if (c.textWidth(s.c_str()) <= largeur) {
        texte(c, s, x, y, p, couleur);
        return;
    }
    auto cs = caracteres(s);
    std::string r;
    for (auto& ch : cs) {
        if (c.textWidth((r + ch + "…").c_str()) > largeur) break;
        r += ch;
    }
    while (!r.empty() && r.back() == ' ') r.pop_back();
    texte(c, r + "…", x, y, p, couleur);
}

int texteMultiligne(Canevas& c, const std::string& s, int x, int y, int largeur, Police p, uint32_t couleur,
                    int interligne, int maxLignes) {
    c.setFont(police(p));
    std::vector<std::string> mots;
    size_t i = 0;
    while (i < s.size()) {
        size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        mots.push_back(s.substr(i, j - i));
        i = j + 1;
    }
    std::vector<std::string> lignes;
    std::string ligne;
    for (auto& m : mots) {
        std::string essai = ligne.empty() ? m : ligne + " " + m;
        if (c.textWidth(essai.c_str()) > largeur && !ligne.empty()) {
            lignes.push_back(ligne);
            ligne = m;
        } else {
            ligne = essai;
        }
    }
    if (!ligne.empty()) lignes.push_back(ligne);
    int n = std::min<int>(lignes.size(), maxLignes);
    for (int k = 0; k < n; ++k) {
        if (k == n - 1 && static_cast<int>(lignes.size()) > n) {
            std::string reste;
            for (size_t q = k; q < lignes.size(); ++q) reste += (q > static_cast<size_t>(k) ? " " : "") + lignes[q];
            texteLimite(c, reste, x, y + k * interligne, largeur, p, couleur);
        } else {
            texte(c, lignes[k], x, y + k * interligne, p, couleur);
        }
    }
    return n * interligne;
}

std::string majuscules(const std::string& s) {
    std::string r;
    for (size_t i = 0; i < s.size(); ++i) {
        unsigned char c = s[i];
        if (c >= 'a' && c <= 'z') {
            r += static_cast<char>(c - 32);
        } else if (c == 0xC3 && i + 1 < s.size()) {
            unsigned char d = s[i + 1];
            // à-þ (U+00E0-U+00FE, sauf ÷) → À-Þ
            if (d >= 0xA0 && d <= 0xBE && d != 0xB7) d -= 0x20;
            r += static_cast<char>(c);
            r += static_cast<char>(d);
            ++i;
        } else if (c == 0xC5 && i + 1 < s.size() && static_cast<unsigned char>(s[i + 1]) == 0x93) {
            r += "\xC5\x92";  // œ → Œ
            ++i;
        } else {
            r += static_cast<char>(c);
        }
    }
    return r;
}

// ---------------------------------------------------------------- segments

namespace {

constexpr float INCLINAISON = 0.105f;  // tan(6°)

struct Pt {
    float x, y;
};

void polygone(Canevas& c, const Pt* p, int n, float ox, float oy, float baseY, uint32_t couleur) {
    auto X = [&](const Pt& q) { return static_cast<int32_t>(std::lround(ox + q.x + (baseY - q.y) * INCLINAISON)); };
    auto Y = [&](const Pt& q) { return static_cast<int32_t>(std::lround(oy + q.y)); };
    for (int i = 1; i < n - 1; ++i)
        c.fillTriangle(X(p[0]), Y(p[0]), X(p[i]), Y(p[i]), X(p[i + 1]), Y(p[i + 1]), C(couleur));
}

void segH(Canevas& c, float x, float y, float L, float E, float ox, float oy, float base, uint32_t col) {
    Pt p[6] = {{x, y + E / 2}, {x + E / 2, y}, {x + L - E / 2, y}, {x + L, y + E / 2}, {x + L - E / 2, y + E}, {x + E / 2, y + E}};
    polygone(c, p, 6, ox, oy, base, col);
}

void segV(Canevas& c, float x, float y, float L, float E, float ox, float oy, float base, uint32_t col) {
    Pt p[6] = {{x + E / 2, y}, {x + E, y + E / 2}, {x + E, y + L - E / 2}, {x + E / 2, y + L}, {x, y + L - E / 2}, {x, y + E / 2}};
    polygone(c, p, 6, ox, oy, base, col);
}

const char* motif(char ch) {
    switch (ch) {
        case '0': return "abcdef";
        case '1': return "bc";
        case '2': return "abged";
        case '3': return "abgcd";
        case '4': return "fgbc";
        case '5': return "afgcd";
        case '6': return "afgedc";
        case '7': return "abc";
        case '8': return "abcdefg";
        case '9': return "abcdfg";
        case '-': return "g";
        default: return "";
    }
}

}  // namespace

int segments(Canevas& c, const std::string& s, int x0, int y0, int W, int H, int E, uint32_t allume, bool fantomes,
             bool halo, bool dessiner) {
    const float g = std::max(1.0f, E * 0.16f);
    const float espace = std::round(W * 0.26f);
    const uint32_t eteint = melange(coul::FOND, allume, 0.07f);
    const uint32_t lueur = melange(coul::FOND, allume, 0.13f);
    float x = 0;
    const float base = static_cast<float>(H);

    for (int passe = (halo ? 0 : 1); passe < 2; ++passe) {
        x = 0;
        for (char ch : s) {
            if (ch == ':') {
                if (dessiner && passe == 1) {
                    for (float yy : {H * 0.28f, H * 0.64f}) {
                        Pt p[4] = {{x, yy}, {x + E, yy}, {x + E, yy + E}, {x, yy + E}};
                        polygone(c, p, 4, x0, y0, base, allume);
                    }
                }
                x += E + espace;
                continue;
            }
            if (ch == ',' || ch == '.') {
                if (dessiner && passe == 1) {
                    float xx = x - espace * 0.4f;
                    const float fh = static_cast<float>(H), fe = static_cast<float>(E);
                    Pt p[4] = {{xx, fh - fe}, {xx + fe, fh - fe}, {xx + fe * 0.7f, fh + fe * 0.5f}, {xx, fh}};
                    polygone(c, p, 4, x0, y0, base, allume);
                }
                x += E + espace * 0.3f;
                continue;
            }
            if (ch == '+') {
                float P = W * 0.7f;
                if (dessiner) {
                    uint32_t col = passe == 0 ? lueur : allume;
                    float d = passe == 0 ? g * 1.6f : 0;
                    segH(c, x - d, H / 2.0f - E / 2 - d, P + 2 * d, E + 2 * d, x0, y0, base, col);
                    segV(c, x + P / 2 - E / 2 - d, H / 2.0f - P / 2 - d, P + 2 * d, E + 2 * d, x0, y0, base, col);
                }
                x += P + espace;
                continue;
            }
            if (ch == ' ') {
                x += W * 0.5f + espace;
                continue;
            }
            if (dessiner) {
                const char* m = motif(ch);
                const float hl = W - E - 2 * g, vl = H / 2.0f - E / 2 - 2 * g;
                struct S {
                    char id;
                    bool h;
                    float sx, sy;
                } segs[7] = {{'a', true, x + E / 2 + g, 0},          {'g', true, x + E / 2 + g, H / 2.0f - E / 2},
                             {'d', true, x + E / 2 + g, static_cast<float>(H - E)},      {'f', false, x, E / 2 + g},
                             {'b', false, x + W - E, E / 2 + g},     {'e', false, x, H / 2.0f + g},
                             {'c', false, x + W - E, H / 2.0f + g}};
                for (auto& sg : segs) {
                    bool on = std::strchr(m, sg.id) != nullptr;
                    if (passe == 0) {
                        if (!on) continue;
                        float d = g * 1.2f;
                        if (sg.h) segH(c, sg.sx - d, sg.sy - d, hl + 2 * d, E + 2 * d, x0, y0, base, lueur);
                        else segV(c, sg.sx - d, sg.sy - d, vl + 2 * d, E + 2 * d, x0, y0, base, lueur);
                    } else {
                        if (!on && !fantomes) continue;
                        uint32_t col = on ? allume : eteint;
                        if (sg.h) segH(c, sg.sx, sg.sy, hl, E, x0, y0, base, col);
                        else segV(c, sg.sx, sg.sy, vl, E, x0, y0, base, col);
                    }
                }
            }
            x += W + espace;
        }
        if (!dessiner) break;
    }
    return static_cast<int>(std::lround(x - espace));
}

// ---------------------------------------------------------------- pictogrammes

void soleil(Canevas& c, int cx, int cy, int r, uint32_t col) {
    for (int i = 0; i < 8; ++i) {
        float a = i * M_PI / 4;
        c.drawWideLine(cx + std::cos(a) * r * 1.45f, cy + std::sin(a) * r * 1.45f, cx + std::cos(a) * r * 1.95f,
                       cy + std::sin(a) * r * 1.95f, std::max(1.0f, r * 0.14f), C(col));
    }
    c.fillSmoothCircle(cx, cy, r, C(col));
}

void petiteLune(Canevas& c, int cx, int cy, int r, uint32_t col, uint32_t fond) {
    c.fillSmoothCircle(cx, cy, r, C(col));
    c.fillSmoothCircle(cx + r * 0.55f, cy - r * 0.35f, r * 0.85f, C(fond));
}

void nuage(Canevas& c, int cx, int cy, int w, uint32_t col) {
    c.fillSmoothCircle(cx - w * 0.22f, cy + w * 0.02f, w * 0.2f, C(col));
    c.fillSmoothCircle(cx + w * 0.02f, cy - w * 0.1f, w * 0.27f, C(col));
    c.fillSmoothCircle(cx + w * 0.26f, cy + w * 0.04f, w * 0.18f, C(col));
    c.fillSmoothRoundRect(cx - w * 0.44f, cy + w * 0.02f, w * 0.88f, w * 0.2f, w * 0.1f, C(col));
}

void picto(Canevas& c, int cx, int cy, int t, int code, int nuages, bool jour) {
    auto astre = [&](int x, int y, int r) {
        if (jour) soleil(c, x, y, r);
        else petiteLune(c, x, y, r, coul::JAUNE, coul::FOND);
    };
    const bool pluie = (code >= 51 && code <= 67) || (code >= 80 && code <= 82);
    const bool neige = (code >= 71 && code <= 77) || code == 85 || code == 86;
    const bool orage = code >= 95;
    const bool brouillard = code == 45 || code == 48;
    const int w = t;

    if (brouillard) {
        for (int i = 0; i < 3; ++i)
            c.drawWideLine(cx - w * 0.4f + i * 3, cy - w * 0.15f + i * w * 0.18f, cx + w * 0.4f - i * 2,
                           cy - w * 0.15f + i * w * 0.18f, w * 0.05f, C(coul::NUAGE));
        return;
    }
    if (code == 0 || (code < 4 && nuages < 15)) {
        if (jour) soleil(c, cx, cy, w * 0.22f);
        else petiteLune(c, cx, cy, w * 0.3f, coul::JAUNE, coul::FOND);
        return;
    }
    if (code <= 2 && !pluie) {
        astre(cx - w * 0.14f, cy - w * 0.14f, w * 0.18f);
        nuage(c, cx + w * 0.06f, cy + w * 0.08f, w * 0.72f, coul::NUAGE);
        return;
    }
    if (code >= 80 && jour && !neige) {  // averses : soleil derrière
        soleil(c, cx - w * 0.2f, cy - w * 0.2f, w * 0.14f);
    }
    nuage(c, cx + w * 0.1f, cy - w * 0.16f, w * 0.6f, coul::NUAGE_SOMBRE);
    nuage(c, cx - w * 0.02f, cy - w * 0.02f, w * 0.78f, coul::NUAGE);
    const float yb = cy + w * 0.26f;
    if (pluie || orage) {
        for (int i = -1; i <= 1; ++i)
            c.drawWideLine(cx + i * w * 0.2f + w * 0.03f, yb, cx + i * w * 0.2f - w * 0.04f, yb + w * 0.14f, w * 0.035f,
                           C(coul::CYAN));
    }
    if (neige) {
        for (int i = -1; i <= 1; ++i) c.fillSmoothCircle(cx + i * w * 0.2f, yb + w * 0.08f, w * 0.045f, C(coul::BLANC));
    }
    if (orage) {
        const float x = cx + w * 0.05f, y = cy + w * 0.12f;
        c.fillTriangle(x, y, x - w * 0.12f, y + w * 0.2f, x + w * 0.02f, y + w * 0.17f, C(0xFFC24A));
        c.fillTriangle(x + w * 0.02f, y + w * 0.14f, x + w * 0.1f, y + w * 0.12f, x - w * 0.06f, y + w * 0.38f, C(0xFFC24A));
    }
}

void lune(Canevas& c, int cx, int cy, int r, double k, bool croissante, bool sud) {
    const uint32_t clair = 0xF4EDCF, sombre = 0x151A24, cratere = 0xD2C79F;
    c.fillSmoothCircle(cx, cy, r, C(clair));
    // Quelques cratères, dessinés avant l'ombre.
    const float cr[4][3] = {{0.3f, -0.3f, 0.12f}, {0.12f, 0.35f, 0.16f}, {0.55f, 0.15f, 0.08f}, {-0.25f, -0.15f, 0.1f}};
    for (auto& q : cr) c.fillSmoothCircle(cx + q[0] * r, cy + q[1] * r, q[2] * r, C(cratere));
    // Partie dans l'ombre, ligne par ligne. Hémisphère nord : croissante = éclairée à droite.
    bool eclaireeADroite = croissante != sud;
    for (int dy = -r; dy <= r; ++dy) {
        float w = std::sqrt(std::max(0.0f, static_cast<float>(r * r - dy * dy)));
        float xt = w * (1 - 2 * static_cast<float>(k));  // terminateur
        int x1, x2;
        if (eclaireeADroite) {
            x1 = static_cast<int>(std::lround(cx - w));
            x2 = static_cast<int>(std::lround(cx + xt));
        } else {
            x1 = static_cast<int>(std::lround(cx - xt));
            x2 = static_cast<int>(std::lround(cx + w));
        }
        if (x2 > x1) c.drawFastHLine(x1, cy + dy, x2 - x1, C(sombre));
    }
}

void planete(Canevas& c, int cx, int cy, int r, uint32_t clair, uint32_t moyen, uint32_t sombre) {
    // Dégradé radial décentré vers le haut à gauche.
    for (int i = 0; i <= r; ++i) {
        float t = static_cast<float>(i) / r;
        uint32_t col = t < 0.6f ? melange(sombre, moyen, t / 0.6f) : melange(moyen, clair, (t - 0.6f) / 0.4f);
        float off = r * 0.32f * t;
        if (i == 0) c.fillSmoothCircle(cx, cy, r, C(col));
        else c.fillCircle(cx - off, cy - off, r - i, C(col));
    }
}

void planeteAnneau(Canevas& c, int cx, int cy, int r, uint32_t clair, uint32_t moyen, uint32_t sombre, uint32_t anneau,
                   uint32_t fond) {
    const int rx = r * 1.85f, ry = r * 0.42f, ep = std::max(2, r / 9);
    const uint32_t anneauArriere = melange(fond, anneau, 0.55f);
    c.fillEllipseArc(cx, cy, rx - ep, rx, ry - ep, ry, 180, 360, C(anneauArriere));
    planete(c, cx, cy, r, clair, moyen, sombre);
    c.fillEllipseArc(cx, cy, rx - ep, rx, ry - ep, ry, 0, 180, C(anneau));
}

void etoile(Canevas& c, int cx, int cy, int r, uint32_t fond) {
    for (int i = 0; i < 10; ++i) {
        float t = i / 10.0f;
        c.fillCircle(cx, cy, r * (2.2f - 1.2f * t), C(melange(fond, 0xFFB347, 0.05f + 0.25f * t)));
    }
    for (int i = 0; i <= r; ++i) {
        float t = static_cast<float>(i) / r;
        uint32_t col = t < 0.5f ? melange(0xFF7A1A, 0xFFC24A, t / 0.5f) : melange(0xFFC24A, 0xFFF6C8, (t - 0.5f) / 0.5f);
        if (i == 0) c.fillSmoothCircle(cx, cy, r, C(col));
        else c.fillCircle(cx, cy, r - i, C(col));
    }
}

void ciel(Canevas& c, int x, int y, int w, int h, int nb, unsigned graine) {
    unsigned s = graine;
    auto alea = [&]() {
        s = s * 1103515245u + 12345u;
        return (s >> 8) & 0xFFFF;
    };
    for (int i = 0; i < nb; ++i) {
        int px = x + alea() % w, py = y + alea() % h;
        c.fillSmoothCircle(px, py, 1, C(melange(coul::FOND, 0xCFD8E6, 0.5f + (alea() % 50) / 100.0f)));
    }
}

void fleche(Canevas& c, int cx, int cy, int L, float a, uint32_t col) {
    // a : direction vers laquelle souffle le vent (degrés, 0 = vers le nord / le haut).
    float rad = a * M_PI / 180.0f;
    float dx = std::sin(rad), dy = -std::cos(rad);
    float x0 = cx - dx * L / 2, y0 = cy - dy * L / 2, x1 = cx + dx * L / 2, y1 = cy + dy * L / 2;
    c.drawWideLine(x0, y0, x1, y1, 1.0f, C(col));
    float px = -dy, py = dx;
    c.fillTriangle(x1 + dx * 2, y1 + dy * 2, x1 - dx * 4 + px * 4, y1 - dy * 4 + py * 4, x1 - dx * 4 - px * 4,
                   y1 - dy * 4 - py * 4, C(col));
}

void pointsPage(Canevas& c, int cx, int cy, int page) {
    for (int i = 0; i < 3; ++i)
        c.fillSmoothCircle(cx + (i - 1) * 16, cy, 4, C(i == page ? coul::BLANC : coul::GRIS_TRES_FONCE));
}

void iconeWifi(Canevas& c, int x, int y, uint32_t col) {
    // x, y : coin haut gauche d'une boîte 20 × 16
    c.fillEllipseArc(x + 10, y + 15, 12, 14, 12, 14, 225, 315, C(col));
    c.fillEllipseArc(x + 10, y + 15, 6, 8, 6, 8, 225, 315, C(col));
    c.fillSmoothCircle(x + 10, y + 14, 2, C(col));
}

void iconeFusee(Canevas& c, int x, int y, uint32_t col) {
    c.drawWideLine(x + 9, y + 2, x + 9, y + 12, 3.0f, C(col));
    c.fillTriangle(x + 6, y + 6, x + 9, y + 1, x + 12, y + 6, C(col));
    c.fillTriangle(x + 6, y + 9, x + 3, y + 14, x + 7, y + 13, C(col));
    c.fillTriangle(x + 12, y + 9, x + 15, y + 14, x + 11, y + 13, C(col));
    c.drawWideLine(x + 9, y + 14, x + 9, y + 17, 1.2f, C(coul::ORANGE));
}

void iconeHorloge(Canevas& c, int x, int y, uint32_t col) {
    c.drawCircle(x + 9, y + 9, 7, C(col));
    c.drawCircle(x + 9, y + 9, 6, C(col));
    c.drawWideLine(x + 9, y + 9, x + 9, y + 5, 0.9f, C(col));
    c.drawWideLine(x + 9, y + 9, x + 12, y + 11, 0.9f, C(col));
}

void iconeEngrenage(Canevas& c, int cx, int cy, uint32_t col, uint32_t fond) {
    for (int i = 0; i < 8; ++i) {
        const float a = i * M_PI / 4;
        c.fillSmoothCircle(cx + std::cos(a) * 9.0f, cy + std::sin(a) * 9.0f, 2.6f, C(col));
    }
    c.fillSmoothCircle(cx, cy, 8, C(col));
    c.fillSmoothCircle(cx, cy, 3.5f, C(fond));
}
