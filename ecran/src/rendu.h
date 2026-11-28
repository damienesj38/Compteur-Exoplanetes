// Outils de dessin sur le canevas 800 × 480 : textes, chiffres à segments,
// pictogrammes météo, planètes et Lune. Indépendant du matériel.
#pragma once

#include <LovyanGFX.hpp>
#include <string>

#include "polices.h"

using Canevas = lgfx::LGFX_Sprite;

// Couleurs de la maquette.
namespace coul {
constexpr uint32_t FOND = 0x04060B, LIGNE = 0x1B2636, CASE = 0x0D1522, CASE_JOUR = 0x101A29, CASE_NUIT = 0x0B111B,
                   CYAN = 0x4FD8FF, ORANGE = 0xFF9F43, VIOLET = 0xC38BFF, CORAIL = 0xFF7A5C, VERT = 0x8CFF6B,
                   GRIS = 0x8A97A8, GRIS_FONCE = 0x6C7A8C, GRIS_TRES_FONCE = 0x3A4B63, BLANC = 0xEEF3F9,
                   BLANC_BLEU = 0xE9F8FF, JAUNE = 0xFFD36B, BLEU = 0x7FB8FF, NUAGE = 0xCFD8E6, NUAGE_SOMBRE = 0x5D6B80;
}

lgfx::rgb888_t C(uint32_t hex);
uint32_t melange(uint32_t a, uint32_t b, float t);  // t = 0 → a, 1 → b

enum Aligne { GAUCHE, CENTRE, DROITE };

// Texte simple. y = haut du texte.
int texte(Canevas& c, const std::string& s, int x, int y, Police p, uint32_t couleur, Aligne a = GAUCHE);
// Texte en capitales espacées (étiquettes).
int etiquette(Canevas& c, const std::string& s, int x, int y, Police p, uint32_t couleur, int espacement = 2,
              Aligne a = GAUCHE);
int largeurEtiquette(Canevas& c, const std::string& s, Police p, int espacement = 2);
// Texte tronqué avec « … » s'il dépasse la largeur.
void texteLimite(Canevas& c, const std::string& s, int x, int y, int largeur, Police p, uint32_t couleur);
// Texte sur plusieurs lignes (coupure aux espaces). Renvoie la hauteur utilisée.
int texteMultiligne(Canevas& c, const std::string& s, int x, int y, int largeur, Police p, uint32_t couleur,
                    int interligne, int maxLignes);
std::string majuscules(const std::string& s);

// Chiffres façon afficheur LCD. Caractères : 0-9, espace, « - », « : », « , », « + ».
// Renvoie la largeur. Si dessiner == false, mesure seulement.
int segments(Canevas& c, const std::string& s, int x, int y, int L, int H, int E, uint32_t allume,
             bool fantomes = true, bool halo = true, bool dessiner = true);

// Pictogrammes.
void picto(Canevas& c, int cx, int cy, int taille, int codeMeteo, int nuages, bool jour);
void soleil(Canevas& c, int cx, int cy, int r, uint32_t couleur = 0xFFC24A);
void nuage(Canevas& c, int cx, int cy, int largeur, uint32_t couleur);
void lune(Canevas& c, int cx, int cy, int r, double eclairement, bool croissante, bool hemisphereSud = false);
void petiteLune(Canevas& c, int cx, int cy, int r, uint32_t couleur, uint32_t fond);
void planeteAnneau(Canevas& c, int cx, int cy, int r, uint32_t clair, uint32_t moyen, uint32_t sombre,
                   uint32_t anneau, uint32_t fond);
void planete(Canevas& c, int cx, int cy, int r, uint32_t clair, uint32_t moyen, uint32_t sombre);
void etoile(Canevas& c, int cx, int cy, int r, uint32_t fond);
void ciel(Canevas& c, int x, int y, int w, int h, int nb, unsigned graine);
void fleche(Canevas& c, int cx, int cy, int longueur, float angleDeg, uint32_t couleur);
void pointsPage(Canevas& c, int cx, int cy, int page);
void iconeWifi(Canevas& c, int x, int y, uint32_t couleur);
void iconeFusee(Canevas& c, int x, int y, uint32_t couleur);
void iconeHorloge(Canevas& c, int x, int y, uint32_t couleur);
void iconeEngrenage(Canevas& c, int cx, int cy, uint32_t couleur, uint32_t fond);
