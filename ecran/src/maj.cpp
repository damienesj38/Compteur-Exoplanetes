#include "maj.h"

#include <Arduino.h>
#include <HTTPUpdate.h>
#include <NetworkClientSecure.h>

#include "config.h"
#include "ecran.h"
#include "pages.h"
#include "reseau.h"

namespace {

std::string g_version;

void progression(int fait, int total) {
    static int dernier = -1;
    int p = total > 0 ? static_cast<int>(100LL * fait / total) : 0;
    if (p / 5 == dernier / 5) return;
    dernier = p;
    ecranMiseAJour(canevas, p, g_version);
    afficher();
}

}  // namespace

bool versionPlusRecente(const std::string& d, const std::string& l) {
    int a[3] = {0, 0, 0}, b[3] = {0, 0, 0};
    sscanf(d.c_str(), "%d.%d.%d", &a[0], &a[1], &a[2]);
    sscanf(l.c_str(), "%d.%d.%d", &b[0], &b[1], &b[2]);
    for (int i = 0; i < 3; ++i)
        if (a[i] != b[i]) return a[i] > b[i];
    return false;
}

void verifierMiseAJour(const std::string& version, const std::string& urlModele) {
    if (version.empty() || urlModele.empty() || !versionPlusRecente(version, FW_VERSION)) return;
    std::string url = urlModele;
    size_t p = url.find("{rev}");
    if (p != std::string::npos) url.replace(p, 5, VERSION_CARTE);

    Serial.printf("maj: %s -> %s (%s)\n", FW_VERSION, version.c_str(), url.c_str());
    g_version = version;
    ecranMiseAJour(canevas, 0, version);
    afficher();

    NetworkClientSecure client;
    preparerClient(client);
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.onProgress(progression);
    httpUpdate.rebootOnUpdate(true);
    t_httpUpdate_return r = httpUpdate.update(client, url.c_str());
    if (r != HTTP_UPDATE_OK) {
        Serial.printf("maj: échec (%d) %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
    }
}
