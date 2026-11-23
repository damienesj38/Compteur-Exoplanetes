// Téléchargements HTTPS (certificats vérifiés) et lecture des sources de données.
#pragma once

#include <Arduino.h>
#include <NetworkClientSecure.h>

#include "model.h"

// Prépare un client HTTPS qui vérifie les certificats (racines Mozilla intégrées).
void preparerClient(NetworkClientSecure& client);

bool telecharger(const String& url, String& corps);

bool lireDonneesCompteur(DonneesCompteur& d);
bool lireMeteo(const Reglages& r, Meteo& m);
bool lireAstro(const Reglages& r, Meteo& m);

// Cherche les coordonnées d'une ville (Open-Meteo Geocoding). Renvoie false si introuvable.
bool geocoder(const String& ville, Reglages& r, String& nomTrouve);
