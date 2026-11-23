// Mise à jour du programme par Wi-Fi, depuis le dépôt GitHub du compteur.
#pragma once

#include <string>

bool versionPlusRecente(const std::string& distante, const std::string& locale);
// Télécharge et installe la nouvelle version si data.json en annonce une. Redémarre en cas de succès.
void verifierMiseAJour(const std::string& version, const std::string& url);
