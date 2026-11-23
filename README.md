# Compteur d'exoplanètes · données

Ce dépôt prépare chaque heure le fichier `data.json` lu par l'écran du compteur d'exoplanètes (Elecrow CrowPanel Advance 7").

- **Le compteur** : nombre d'objets extrasolaires confirmés, d'après exoplanet.eu (Observatoire de Paris).
- **Le chiffre du jour** : une distance, un temps-lumière ou un anniversaire spatial. Un même chiffre ne revient pas avant 40 jours.
- **Les prochains lancements** : les 5 suivants, toutes agences confondues, d'après Launch Library 2 (The Space Devs).

L'écran n'a besoin d'aucun compte GitHub : il lit simplement l'adresse publique du fichier.

## Mise en route (une seule fois, depuis un ordinateur)

1. **Créer le dépôt.** Sur github.com, bouton **New repository**. Nom : `compteur-exoplanetes`. Visibilité : **Public**. Ne coche rien d'autre, puis **Create repository**.
2. **Envoyer les fichiers.** Sur la page du dépôt vide, clique sur le lien **uploading an existing file**. Dézippe l'archive sur ton PC, ouvre le dossier `compteur-exoplanetes`, sélectionne **tout son contenu** (y compris le dossier `.github`) et glisse-le dans la page. En bas, **Commit changes**.
   Vérifie ensuite que le dépôt contient bien `.github/workflows/maj-donnees.yml`. Si le dossier `.github` manque (certains navigateurs ignorent les dossiers qui commencent par un point), crée le fichier à la main : **Add file › Create new file**, tape `.github/workflows/maj-donnees.yml` comme nom (les `/` créent les dossiers), colle le contenu du fichier fourni, puis **Commit changes**.
3. **Activer la page web.** Onglet **Settings**, puis **Pages** dans le menu de gauche. Sous **Build and deployment**, choisis **Source : GitHub Actions**.
4. **Premier lancement.** Onglet **Actions**. Si GitHub demande d'activer les workflows, accepte. Clique sur **Mise à jour des données**, puis **Run workflow**, puis le bouton vert **Run workflow**. Au bout d'une à deux minutes, une coche verte apparaît.
5. **Vérifier.** Ouvre `https://damienesj38.github.io/compteur-exoplanetes/` (remplace `damienesj38` si ton identifiant GitHub est différent). Tu dois voir le compteur, le chiffre du jour et le prochain lancement.

Ensuite, tout tourne seul, toutes les heures.

**L'adresse à donner à l'écran** : `https://damienesj38.github.io/compteur-exoplanetes/data.json`

## Si quelque chose coince

- **Croix rouge à l'étape « Enregistrer l'historique »** : Settings › Actions › General › Workflow permissions › coche **Read and write permissions** › Save, puis relance.
- **Croix rouge à l'étape « Publier »** : vérifie l'étape 3 (Source : GitHub Actions).
- **Croix rouge à l'étape « Tests »** : un fichier de `config/` a été mal modifié (souvent une virgule ou un guillemet). Annule la dernière modification.
- **Une source en panne** (exoplanet.eu, lancements, Horizons) : rien ne casse. La dernière valeur connue reste affichée et le problème est listé dans `alertes`, en bas de la page web.
- **GitHub désactive les tâches planifiées d'un dépôt public sans activité pendant 60 jours.** Ici, le script enregistre les nouvelles données au moins une fois par jour, ce qui compte comme une activité.

## Régler

Tout se fait dans le dossier `config/`, directement sur github.com (clic sur le fichier, puis l'icône crayon) :

- `reglages.json`
  - `lancements.nombre` : combien de lancements garder (5 par défaut) ;
  - `lancements.operateurs_filtre` : par exemple `["SpaceX"]` pour ne garder que SpaceX, ou `["SpaceX", "Arianespace"]`. Liste vide = tout le monde ;
  - `chiffre_du_jour.ecart_minimum_jours` : délai avant qu'un chiffre puisse revenir (40) ;
  - `compteur.libelle` : le texte affiché au-dessus du compteur.
- `chiffres.json` : la réserve du chiffre du jour (46 chiffres). Pour en ajouter un, copie une ligne existante et change son `id`.

## Contenu de `data.json`

| Clé | Contenu |
|---|---|
| `exoplanetes.total` | le compteur (8540 au 21 novembre 2026) |
| `exoplanetes.nouvelles_24h`, `_7j`, `_30j` | progression, disponible quand l'historique couvre la période |
| `exoplanetes.derniere_variation` | date et nombre de la dernière hausse, pour l'alerte de l'écran |
| `exoplanetes.historique_30j` | un point par jour, pour une future courbe |
| `chiffre_du_jour` | `valeur` (format afficheur, ex. `25,9`), `unite`, `legende` (courte), `texte` (phrase complète) |
| `lancements[]` | `fusee`, `mission`, `operateur`, `net` (heure UTC), `statut_fr`, `lieu_court` |
| `firmware` | version et adresse du programme de l'écran, pour les mises à jour par Wi-Fi |
| `alertes` | sources en panne lors du dernier passage |

## Tester sur un ordinateur

```
python3 -m unittest discover tests     # tests hors ligne
python3 scripts/update_data.py         # vrai passage, nécessite internet
```

Sources : [exoplanet.eu](https://exoplanet.eu), [Launch Library 2](https://thespacedevs.com/llapi), [JPL Horizons](https://ssd.jpl.nasa.gov/horizons/).
