# Programme de l'écran

Programme de l'écran Elecrow CrowPanel Advance 7" (DIS02170A, ESP32-S3-WROOM-1-N16R8, 800 × 480).

- **Page 1, accueil** : météo de la nuit, compteur, phase de la Lune, chiffre du jour, heure, prochain lancement.
- **Page 2, compteur** : le compteur en grand, avec l'alerte « nouvelle exoplanète » (1 minute, jamais entre 22 h et 7 h).
- **Page 3, météo** : 8 créneaux de 3 heures, jour et nuit : ciel, température, nuages, vent, et la nuit seeing et transparence.

On change de page en balayant l'écran. Un appui de 5 secondes permet de changer de Wi-Fi ou de ville. Sans geste pendant 3 minutes, l'écran revient à l'accueil. La luminosité baisse entre 22 h et 7 h.

## Installer

Pas besoin de ce dossier : utilise la page `https://damienesj38.github.io/Compteur-Exoplanetes/installer/` (Chrome ou Edge sur ordinateur).

## Sources des données

- Compteur, chiffre du jour, lancements : `docs/data.json` de ce dépôt, mis à jour toutes les heures.
- Météo : modèles Météo-France AROME et ARPEGE via l'API Open-Meteo (licence CC BY 4.0).
- Seeing et transparence : 7Timer! (modèle GFS de la NOAA).
- Coordonnées de la ville : géocodage Open-Meteo.

Toutes les connexions se font en HTTPS, avec vérification des certificats (racines Mozilla dans `certs/`).

## Publier une nouvelle version

1. Changer `FW_VERSION` dans `src/config.h`.
2. Compiler : `pio run -e v13` et `pio run -e v12` (PlatformIO).
3. Copier `.pio/build/v13/firmware.bin` vers `docs/firmware/compteur-v13.bin` (idem pour v12). Copier aussi `firmware.factory.bin` vers `docs/firmware/compteur-v13-full.bin` pour la page d'installation.
4. Mettre le même numéro dans `config/reglages.json` (`firmware.version`) et dans `docs/installer/manifest-*.json`.

Les écrans déjà installés se mettent alors à jour tout seuls par Wi-Fi, dans l'heure qui suit.

## Simulateur

`tools/simulateur/` dessine les pages sur ordinateur (avec LovyanGFX compilé pour Linux) et les enregistre en PNG, pour vérifier la mise en page sans la carte.

## Crédits

- Brochage, rétroéclairage et tactile : repris de [robominds/Crowpanel-Advance-7.0-HMI-Display](https://github.com/robominds/Crowpanel-Advance-7.0-HMI-Display) (licence MIT, voir `src/LICENSE-robominds.txt`).
- Police Barlow Condensed : SIL Open Font License 1.1 (`src/fonts/OFL-Barlow.txt`).
- Bibliothèques : LovyanGFX, ArduinoJson, WiFiManager.
