#include "reseau.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

#include "config.h"

extern const uint8_t paquet_ca_debut[] asm("_binary_certs_x509_crt_bundle_bin_start");
extern const uint8_t paquet_ca_fin[] asm("_binary_certs_x509_crt_bundle_bin_end");

namespace {

// Conversion date civile → secondes Unix (UTC), sans dépendre du fuseau du système.
time_t versUnix(int a, int mo, int j, int h, int mi, int s) {
    a -= mo <= 2;
    const int ere = (a >= 0 ? a : a - 399) / 400;
    const unsigned ae = static_cast<unsigned>(a - ere * 400);
    const unsigned ja = (153 * (mo + (mo > 2 ? -3 : 9)) + 2) / 5 + j - 1;
    const unsigned je = ae * 365 + ae / 4 - ae / 100 + ja;
    const long jours = ere * 146097L + static_cast<long>(je) - 719468L;
    return static_cast<time_t>(jours * 86400L + h * 3600L + mi * 60L + s);
}

time_t isoVersUnix(const char* iso) {  // « 2026-11-24T11:09:00Z »
    int a, mo, j, h = 0, mi = 0, s = 0;
    if (!iso || sscanf(iso, "%d-%d-%dT%d:%d:%d", &a, &mo, &j, &h, &mi, &s) < 3) return 0;
    return versUnix(a, mo, j, h, mi, s);
}

String encoderUrl(const String& s) {
    String r;
    const char* hex = "0123456789ABCDEF";
    for (size_t i = 0; i < s.length(); ++i) {
        uint8_t c = s[i];
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') r += static_cast<char>(c);
        else {
            r += '%';
            r += hex[c >> 4];
            r += hex[c & 15];
        }
    }
    return r;
}

std::string chaine(JsonVariantConst v) {
    const char* s = v.as<const char*>();
    return s ? std::string(s) : std::string();
}

}  // namespace

void preparerClient(NetworkClientSecure& client) {
    client.setCACertBundle(paquet_ca_debut, paquet_ca_fin - paquet_ca_debut);
    client.setTimeout(15);
}

bool telecharger(const String& url, String& corps) {
    NetworkClientSecure client;
    preparerClient(client);
    HTTPClient http;
    http.setTimeout(15000);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setUserAgent("compteur-exoplanetes/" FW_VERSION);
    if (!http.begin(client, url)) return false;
    const int code = http.GET();
    if (code != HTTP_CODE_OK) {
        Serial.printf("reseau: %s -> %d\n", url.c_str(), code);
        http.end();
        return false;
    }
    corps = http.getString();
    http.end();
    return corps.length() > 0;
}

bool lireDonneesCompteur(DonneesCompteur& d) {
    String corps;
    if (!telecharger(URL_DONNEES, corps)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, corps)) return false;

    DonneesCompteur n;
    JsonObjectConst e = doc["exoplanetes"];
    n.total = e["total"].is<int>() ? e["total"].as<int>() : VALEUR_ABSENTE;
    if (e["libelle"].is<const char*>()) n.libelle = chaine(e["libelle"]);
    n.majCatalogue = chaine(e["maj_catalogue"]);
    n.nouvelles24h = e["nouvelles_24h"].is<int>() ? e["nouvelles_24h"].as<int>() : VALEUR_ABSENTE;

    JsonObjectConst c = doc["chiffre_du_jour"];
    n.chiffreValeur = chaine(c["valeur"]);
    n.chiffreUnite = chaine(c["unite"]);
    n.chiffreLegende = chaine(c["legende"]);

    for (JsonObjectConst l : doc["lancements"].as<JsonArrayConst>()) {
        if (n.nbLancements >= NB_LANCEMENTS) break;
        Lancement& x = n.lancements[n.nbLancements++];
        x.fusee = chaine(l["fusee"]);
        x.mission = chaine(l["mission"]);
        x.lieu = chaine(l["lieu_court"]);
        x.statut = chaine(l["statut_fr"]);
        x.net = isoVersUnix(l["net"].as<const char*>());
    }
    n.firmwareVersion = chaine(doc["firmware"]["version"]);
    n.firmwareUrl = chaine(doc["firmware"]["url"]);
    n.ok = n.total != VALEUR_ABSENTE;
    n.recuLe = time(nullptr);
    if (n.ok) d = n;
    return n.ok;
}

bool lireMeteo(const Reglages& r, Meteo& m) {
    const char* variables =
        "hourly=temperature_2m,weather_code,cloud_cover,precipitation,wind_speed_10m,wind_direction_10m,is_day"
        "&timezone=auto&timeformat=unixtime&forecast_days=3";
    char pos[64];
    snprintf(pos, sizeof pos, "latitude=%.4f&longitude=%.4f&", r.lat, r.lon);
    // Météo-France (AROME/ARPEGE) d'abord ; en secours, les modèles par défaut d'Open-Meteo.
    const String urls[] = {
        String("https://api.open-meteo.com/v1/meteofrance?") + pos + variables,
        String("https://api.open-meteo.com/v1/forecast?models=meteofrance_seamless&") + pos + variables,
        String("https://api.open-meteo.com/v1/forecast?") + pos + variables,
    };
    for (const String& url : urls) {
        String corps;
        if (!telecharger(url, corps)) continue;
        JsonDocument doc;
        if (deserializeJson(doc, corps)) continue;
        JsonObjectConst h = doc["hourly"];
        JsonArrayConst t = h["time"];
        if (t.isNull() || t.size() == 0) continue;

        const time_t maintenant = time(nullptr);
        m.n = 0;
        for (size_t i = 0; i < t.size() && m.n < NB_HEURES; ++i) {
            time_t ti = t[i].as<long>();
            if (maintenant > 1700000000 && ti < maintenant - 3 * 3600) continue;
            Heure& x = m.h[m.n++];
            x.t = ti;
            x.temp = h["temperature_2m"][i] | 0.0f;
            x.code = h["weather_code"][i] | 3;
            x.nuages = h["cloud_cover"][i] | 0;
            x.pluie = h["precipitation"][i] | 0.0f;
            x.vent = h["wind_speed_10m"][i] | 0.0f;
            x.ventDir = h["wind_direction_10m"][i] | 0;
            x.jour = (h["is_day"][i] | 0) == 1;
        }
        m.decalageUtc = doc["utc_offset_seconds"] | m.decalageUtc;
        m.ok = m.n > 0;
        m.recuLe = maintenant;
        Serial.printf("meteo: %d heures depuis %s\n", m.n, url.c_str());
        if (m.ok) return true;
    }
    return false;
}

bool lireAstro(const Reglages& r, Meteo& m) {
    char url[160];
    snprintf(url, sizeof url, "https://www.7timer.info/bin/api.pl?lon=%.3f&lat=%.3f&product=astro&output=json", r.lon,
             r.lat);
    String corps;
    if (!telecharger(url, corps)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, corps)) return false;
    const char* init = doc["init"];  // « 2026112300 » (UTC)
    int a, mo, j, h;
    if (!init || sscanf(init, "%4d%2d%2d%2d", &a, &mo, &j, &h) != 4) return false;
    m.astroInit = versUnix(a, mo, j, h, 0, 0);
    m.nAstro = 0;
    for (JsonObjectConst p : doc["dataseries"].as<JsonArrayConst>()) {
        if (m.nAstro >= NB_ASTRO) break;
        int s = p["seeing"] | 0, tr = p["transparency"] | 0;
        m.astroHeure[m.nAstro] = p["timepoint"] | 0;
        m.seeing[m.nAstro] = (s >= 1 && s <= 8) ? s : 0;
        m.transparence[m.nAstro] = (tr >= 1 && tr <= 8) ? tr : 0;
        ++m.nAstro;
    }
    m.astroOk = m.nAstro > 0;
    return m.astroOk;
}

bool geocoder(const String& ville, Reglages& r, String& nomTrouve) {
    String url = "https://geocoding-api.open-meteo.com/v1/search?count=1&language=fr&format=json&name=" + encoderUrl(ville);
    String corps;
    if (!telecharger(url, corps)) return false;
    JsonDocument doc;
    if (deserializeJson(doc, corps)) return false;
    JsonObjectConst res = doc["results"][0];
    if (res.isNull()) return false;
    r.lat = res["latitude"] | r.lat;
    r.lon = res["longitude"] | r.lon;
    nomTrouve = res["name"] | ville.c_str();
    return true;
}
