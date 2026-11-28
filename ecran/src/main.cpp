// Compteur d'exoplanètes : écran Elecrow CrowPanel Advance 7" (ESP32-S3).
//
// Page 1 : accueil (météo de la nuit, compteur, Lune, chiffre du jour, heure, prochain lancement)
// Page 2 : compteur seul, avec l'alerte « nouvelle exoplanète »
// Page 3 : météo heure par heure, jour et nuit
// On passe d'une page à l'autre en balayant l'écran. Un appui de 5 secondes permet de
// changer de Wi-Fi ou de ville.

#include <Arduino.h>
#include <Preferences.h>
#include <cmath>
#include <vector>
#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>

#include "astro.h"
#include "board_pins.h"
#include "config.h"
#include "ecran.h"
#include "maj.h"
#include "pages.h"
#include "panel_mcu.h"
#include "polices.h"
#include "reseau.h"
#include "tactile.h"

namespace {

Preferences prefs;
DonneesCompteur donnees;
Meteo meteo;
Reglages reglages;
EtatEcran etat;

uint32_t tDonnees = 0, tMeteo = 0, tAstro = 0, tInteraction = 0, tAlerte = 0, tClignote = 0, tWifi = 0;
bool premiereDonnees = true, premiereMeteo = true, premiereAstro = true;
bool aRedessiner = true, alerteEnAttente = false, menuReinit = false;
int dernierTotalVu = VALEUR_ABSENTE;
int derniereMinute = -1, derniereLuminosite = -1;
std::string versionEchouee;
uint32_t tVersionEchouee = 0;

// Geste en cours
bool doigtPose = false;
int16_t xDepart = 0, yDepart = 0, xDernier = 0, yDernier = 0;
uint32_t tDepart = 0;

bool heureCalme(time_t maintenant) {
    int h = heureLocale(maintenant, meteo.decalageUtc).tm_hour;
    return h >= HEURE_DEBUT_NUIT || h < HEURE_FIN_NUIT;
}

void dessiner() {
    const time_t maintenant = time(nullptr);
    if (menuReinit) ecranReinitialisation(canevas);
    else if (etat.page == 0) pageAccueil(canevas, donnees, meteo, reglages, maintenant, etat);
    else if (etat.page == 1) pageCompteur(canevas, donnees, meteo, maintenant, etat);
    else pageMeteo(canevas, meteo, reglages, maintenant);
    afficher();
    aRedessiner = false;
}

void message(const std::string& titre, const std::vector<std::string>& lignes, uint32_t couleur = coul::CYAN) {
    ecranMessage(canevas, titre, lignes, couleur);
    afficher();
}

void demarrerAlerte() {
    etat.alerte = true;
    etat.page = 1;
    etat.clignote = true;
    tAlerte = tClignote = millis();
    alerteEnAttente = false;
    aRedessiner = true;
}

void finirAlerte() {
    etat.alerte = false;
    etat.page = 0;
    aRedessiner = true;
}

void reglerLuminosite() {
    int l = heureCalme(time(nullptr)) ? LUMINOSITE_NUIT : LUMINOSITE_JOUR;
    if (l != derniereLuminosite) {
        panel_mcu::backlightLevel(static_cast<uint8_t>(l));
        derniereLuminosite = l;
    }
}

// ------------------------------------------------------------------ Wi-Fi et ville

// Le réseau et son mot de passe sont rangés dans la mémoire de l'écran (Preferences),
// pour se reconnecter seul après une coupure de courant. Ils ne quittent jamais l'écran.
bool connecterReseauConnu(const String& ssid, const String& mdp, uint32_t delaiMs) {
    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), mdp.c_str());
    const uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < delaiMs) delay(250);
    return WiFi.status() == WL_CONNECTED;
}

void configurerWifi() {
    const String ssidConnu = prefs.getString("wifi_ssid", "");
    const String mdpConnu = prefs.getString("wifi_mdp", "");

    // 1. Réseau déjà connu : on insiste environ 2 minutes, le temps qu'une box
    //    redémarre après une coupure de courant.
    if (!ssidConnu.isEmpty()) {
        const int ESSAIS = 6;
        for (int essai = 1; essai <= ESSAIS; ++essai) {
            std::string ligne2 = essai == 1 ? "Patiente quelques secondes."
                                            : "Nouvel essai " + std::to_string(essai) + "/" + std::to_string(ESSAIS) + "…";
            message("Connexion au Wi-Fi…", {std::string("Réseau « ") + ssidConnu.c_str() + " »", ligne2});
            if (connecterReseauConnu(ssidConnu, mdpConnu, 20000)) return;
            WiFi.disconnect(true);
            delay(1000);
        }
    }

    // 2. Premier démarrage, ou réseau introuvable : page de configuration.
    WiFi.mode(WIFI_STA);
    WiFiManager wm;
    String villeActuelle = prefs.getString("ville", "");
    WiFiManagerParameter champVille("ville", "Ta ville (pour la météo)", villeActuelle.c_str(), 60,
                                    "placeholder=\"ex. Tencin\"");
    wm.addParameter(&champVille);
    wm.setTitle("Compteur d'exoplanètes");
    wm.setClass("invert");
    std::vector<const char*> menu = {"wifi", "exit"};
    wm.setMenu(menu);
    // Réseau déjà connu mais absent (box en panne, coupure) : la page reste ouverte
    // 5 minutes, puis l'écran redémarre et réessaie le réseau connu.
    wm.setConfigPortalTimeout(ssidConnu.isEmpty() ? 0 : 300);
    wm.setConnectTimeout(20);
    wm.setAPCallback([](WiFiManager*) {
        ecranConfiguration(canevas, NOM_WIFI_CONFIG);
        afficher();
    });

    message("Connexion au Wi-Fi…", {"Patiente quelques secondes."});
    if (!wm.autoConnect(NOM_WIFI_CONFIG)) ESP.restart();

    // Mémoriser le réseau qui vient de fonctionner.
    const String ssid = WiFi.SSID();
    const String mdp = WiFi.psk();
    if (!ssid.isEmpty() && (ssid != ssidConnu || mdp != mdpConnu)) {
        prefs.putString("wifi_ssid", ssid);
        prefs.putString("wifi_mdp", mdp);
    }

    String villeSaisie = champVille.getValue();
    villeSaisie.trim();
    if (villeSaisie.length() > 0 && villeSaisie != villeActuelle) {
        prefs.putString("ville", villeSaisie);
        prefs.remove("lat");  // à géolocaliser à nouveau
    }
}

void localiserVille() {
    String ville = prefs.getString("ville", "");
    if (ville.isEmpty()) {
        reglages.ville = "Ville non définie";
        return;
    }
    reglages.ville = prefs.getString("nom", ville).c_str();
    if (prefs.isKey("lat")) {
        reglages.lat = prefs.getDouble("lat", reglages.lat);
        reglages.lon = prefs.getDouble("lon", reglages.lon);
        return;
    }
    String trouve;
    if (geocoder(ville, reglages, trouve)) {
        prefs.putDouble("lat", reglages.lat);
        prefs.putDouble("lon", reglages.lon);
        prefs.putString("nom", trouve);
        reglages.ville = trouve.c_str();
    } else {
        message("Ville introuvable", {"Je n'ai pas trouvé « " + std::string(ville.c_str()) + " ».",
                                      "Appuie 5 secondes sur l'écran pour corriger la ville. En attendant, la météo reste indisponible."},
                coul::ORANGE);
        meteo.ok = false;
        reglages.ville = ville.c_str();
        reglages.lat = NAN;
        delay(8000);
    }
}

// ------------------------------------------------------------------ données

void majDonnees() {
    if (!lireDonneesCompteur(donnees)) return;
    aRedessiner = true;
    const time_t maintenant = time(nullptr);
    if (donnees.total != VALEUR_ABSENTE) {
        if (dernierTotalVu != VALEUR_ABSENTE && donnees.total > dernierTotalVu) {
            if (heureCalme(maintenant)) alerteEnAttente = true;
            else demarrerAlerte();
        }
        if (donnees.total != dernierTotalVu) {
            dernierTotalVu = donnees.total;
            prefs.putInt("total", dernierTotalVu);
        }
    }
    // Mise à jour du programme (pas pendant une alerte), en évitant de réessayer sans cesse une version qui échoue.
    if (!etat.alerte && !(donnees.firmwareVersion == versionEchouee && millis() - tVersionEchouee < 6UL * 3600 * 1000)) {
        verifierMiseAJour(donnees.firmwareVersion, donnees.firmwareUrl);
        if (versionPlusRecente(donnees.firmwareVersion, FW_VERSION)) {  // toujours là : échec
            versionEchouee = donnees.firmwareVersion;
            tVersionEchouee = millis();
            aRedessiner = true;
        }
    }
}

void taches() {
    const uint32_t t = millis();
    if (WiFi.status() != WL_CONNECTED) {
        if (etat.wifiOk) aRedessiner = true;
        etat.wifiOk = false;
        if (t - tWifi > 30000) {
            tWifi = t;
            WiFi.reconnect();
        }
        return;
    }
    if (!etat.wifiOk) aRedessiner = true;
    etat.wifiOk = true;
    if (doigtPose) return;  // ne pas bloquer l'écran pendant un geste

    if (premiereDonnees || t - tDonnees > (donnees.ok ? INTERVALLE_DONNEES : NOUVEL_ESSAI)) {
        premiereDonnees = false;
        tDonnees = t;
        majDonnees();
    }
    if (!std::isnan(reglages.lat)) {
        if (premiereMeteo || t - tMeteo > (meteo.ok ? INTERVALLE_METEO : NOUVEL_ESSAI)) {
            premiereMeteo = false;
            tMeteo = t;
            if (lireMeteo(reglages, meteo)) {
                prefs.putInt("decalage", meteo.decalageUtc);
                aRedessiner = true;
            }
        }
        if (premiereAstro || t - tAstro > (meteo.astroOk ? INTERVALLE_ASTRO : NOUVEL_ESSAI)) {
            premiereAstro = false;
            tAstro = t;
            if (lireAstro(reglages, meteo)) aRedessiner = true;
        }
    }
}

// ------------------------------------------------------------------ gestes

void gestes() {
    lireTactile();
    const bool appuye = tactileAppuye();
    const uint32_t t = millis();
    if (appuye) {
        if (!doigtPose) {
            doigtPose = true;
            xDepart = xDernier = tactileX();
            yDepart = yDernier = tactileY();
            tDepart = t;
        } else {
            xDernier = tactileX();
            yDernier = tactileY();
        }
        // Appui long immobile : menu de réinitialisation.
        if (!menuReinit && t - tDepart > 5000 && abs(xDernier - xDepart) < 30 && abs(yDernier - yDepart) < 30) {
            menuReinit = true;
            aRedessiner = true;
        }
        return;
    }
    if (!doigtPose) return;
    doigtPose = false;
    tInteraction = t;
    const int dx = xDernier - xDepart, dy = yDernier - yDepart;

    if (menuReinit) {
        if (t - tDepart > 4000) return;  // relâchement de l'appui long qui a ouvert le menu
        const bool dansY = yDernier >= BOUTON_Y && yDernier <= BOUTON_Y + BOUTON_H;
        if (dansY && xDernier >= BOUTON_OUI_X && xDernier <= BOUTON_OUI_X + BOUTON_L) {
            message("Réinitialisation…", {"L'écran va redémarrer sur la page de configuration."}, coul::ORANGE);
            WiFiManager wm;
            wm.resetSettings();
            prefs.remove("ville");
            prefs.remove("nom");
            prefs.remove("lat");
            prefs.remove("lon");
            prefs.remove("wifi_ssid");
            prefs.remove("wifi_mdp");
            delay(1500);
            ESP.restart();
        } else if (dansY && xDernier >= BOUTON_NON_X && xDernier <= BOUTON_NON_X + BOUTON_L) {
            menuReinit = false;
            aRedessiner = true;
        }
        return;
    }
    if (etat.alerte) {  // un toucher arrête l'alerte
        finirAlerte();
        return;
    }
    if (abs(dx) > 90 && abs(dx) > abs(dy)) {
        etat.page = dx < 0 ? (etat.page + 1) % 3 : (etat.page + 2) % 3;
        aRedessiner = true;
    }
}

}  // namespace

// ------------------------------------------------------------------ Arduino

void setup() {
    Serial.begin(115200);
    Serial.printf("\nCompteur d'exoplanètes %s (carte %s)\n", FW_VERSION, VERSION_CARTE);
    Wire.begin(board::I2C_SDA, board::I2C_SCL, board::I2C_HZ);
    panel_mcu::begin();
    initPolices();
    if (!initEcran()) {
        delay(5000);
        ESP.restart();
    }
    message("Compteur d'exoplanètes", {"Démarrage… (version " FW_VERSION ")"});
    delay(200);  // laisser l'horloge de l'écran se stabiliser avant d'allumer
    panel_mcu::backlightOn();
    initTactile();

    prefs.begin("compteur", false);
    dernierTotalVu = prefs.getInt("total", VALEUR_ABSENTE);
    meteo.decalageUtc = prefs.getInt("decalage", 3600);

    configurerWifi();
    etat.wifiOk = true;

    configTime(0, 0, "pool.ntp.org", "time.google.com");
    message("Mise à l'heure…", {"Récupération de l'heure sur internet."});
    for (int i = 0; i < 40 && time(nullptr) < 1700000000; ++i) delay(250);

    localiserVille();
    message("Chargement des données…", {"Compteur, chiffre du jour, lancements et météo."});
    tInteraction = millis();
}

void loop() {
    gestes();
    taches();
    const uint32_t t = millis();
    const time_t maintenant = time(nullptr);

    if (etat.alerte) {
        if (t - tClignote > 500) {
            tClignote = t;
            etat.clignote = !etat.clignote;
            aRedessiner = true;
        }
        if (t - tAlerte > DUREE_ALERTE) finirAlerte();
    } else if (alerteEnAttente && !heureCalme(maintenant)) {
        demarrerAlerte();
    } else if (etat.page != 0 && !menuReinit && t - tInteraction > RETOUR_ACCUEIL) {
        etat.page = 0;
        aRedessiner = true;
    }

    const int minute = heureLocale(maintenant, meteo.decalageUtc).tm_min;
    if (minute != derniereMinute) {
        derniereMinute = minute;
        aRedessiner = true;
        reglerLuminosite();
    }
    if (aRedessiner && !doigtPose) dessiner();
    delay(5);
}
