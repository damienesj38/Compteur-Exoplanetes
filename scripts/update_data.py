#!/usr/bin/env python3
"""Prépare docs/data.json, le fichier que lit l'écran du compteur d'exoplanètes.

Lancé toutes les heures par GitHub Actions. Uniquement la bibliothèque standard.

Sources :
  - exoplanet.eu (compteur de l'Observatoire de Paris), avec la page d'accueil en secours
  - Launch Library 2 (The Space Devs) pour les prochains lancements
  - JPL Horizons pour les distances du chiffre du jour

Si une source tombe, la dernière valeur connue est conservée et l'erreur est
listée dans "alertes" : le script ne fait pas échouer la tâche pour autant.
"""

from __future__ import annotations

import datetime as dt
import hashlib
import html
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path
from zoneinfo import ZoneInfo

RACINE = Path(__file__).resolve().parent.parent
CONFIG = RACINE / "config"
ETAT = RACINE / "data" / "etat.json"
SORTIE = RACINE / "docs" / "data.json"

URL_COMPTEUR = "https://exoplanet.eu/scholar_api/counter/simple/"
URL_ACCUEIL = "https://exoplanet.eu/home/"
URL_LANCEMENTS = "https://ll.thespacedevs.com/2.2.0/launch/upcoming/?limit=12"
URL_HORIZONS = "https://ssd.jpl.nasa.gov/api/horizons.api"

UA = "compteur-exoplanetes/1.0 (projet perso, ecran ESP32)"
UA_KM = 149_597_870.7
C_KM_S = 299_792.458

MOIS_FR = {
    "janvier": 1, "février": 2, "fevrier": 2, "mars": 3, "avril": 4, "mai": 5, "juin": 6,
    "juillet": 7, "août": 8, "aout": 8, "septembre": 9, "octobre": 10, "novembre": 11,
    "décembre": 12, "decembre": 12,
}
MOIS_EN = {m: i for i, m in enumerate(
    ["jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec"], start=1)}

STATUTS_FR = {
    "Go": "Feu vert", "TBD": "Date à confirmer", "TBC": "À confirmer",
    "Hold": "Suspendu", "In Flight": "En vol", "Success": "Réussi",
    "Failure": "Échec", "Partial Failure": "Échec partiel",
}
STATUTS_TERMINES = {"Success", "Failure", "Partial Failure"}

MISSIONS_FR = {
    "unknown payload": "Charge non communiquée",
    "unknown": "Mission non communiquée",
}

LIEUX_COURTS = [
    ("guiana", "Kourou"), ("kourou", "Kourou"),
    ("cape canaveral", "Cap Canaveral"), ("kennedy", "Cap Canaveral"),
    ("vandenberg", "Vandenberg"), ("starbase", "Starbase"), ("boca chica", "Starbase"),
    ("baikonur", "Baïkonour"), ("plesetsk", "Plessetsk"), ("vostochny", "Vostotchny"),
    ("jiuquan", "Jiuquan"), ("wenchang", "Wenchang"), ("xichang", "Xichang"),
    ("taiyuan", "Taiyuan"), ("tanegashima", "Tanegashima"),
    ("satish dhawan", "Sriharikota"), ("sriharikota", "Sriharikota"),
    ("mahia", "Mahia (N.-Z.)"), ("wallops", "Wallops"), ("kodiak", "Kodiak"),
    ("andøya", "Andøya"), ("andoya", "Andøya"), ("esrange", "Esrange"),
]


# --------------------------------------------------------------------------- réseau

def telecharger(url: str, essais: int = 3, delai: float = 5.0) -> str:
    """GET avec quelques nouvelles tentatives. Remplaçable dans les tests."""
    derniere = None
    for i in range(essais):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=40) as rep:
                return rep.read().decode("utf-8", errors="replace")
        except Exception as e:  # noqa: BLE001
            derniere = e
            if i < essais - 1:
                time.sleep(delai * (i + 1))
    raise RuntimeError(f"{url} : {derniere}")


# --------------------------------------------------------------------------- utilitaires

def html_en_texte(source: str) -> str:
    source = re.sub(r"(?is)<(script|style)\b.*?</\1>", " ", source)
    source = re.sub(r"(?s)<[^>]+>", "\n", source)
    source = html.unescape(source)
    return re.sub(r"[ \t  ]+", " ", source)


def entier_depuis(texte: str) -> int:
    return int(re.sub(r"[^\d]", "", texte))


def nombre_fr(x: float, decimales: int = 0) -> str:
    s = f"{x:,.{decimales}f}"
    return s.replace(",", " ").replace(".", ",")


def charger_json(chemin: Path, defaut):
    try:
        return json.loads(chemin.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return defaut


def ecrire_json(chemin: Path, contenu) -> None:
    chemin.parent.mkdir(parents=True, exist_ok=True)
    chemin.write_text(json.dumps(contenu, ensure_ascii=False, indent=1) + "\n", encoding="utf-8")


# --------------------------------------------------------------------------- compteur

def analyser_compteur(source: str) -> tuple[int, str | None]:
    """Page de l'API scholar : '... Objets extrasolaires confirmés ... 8540 ... dernière modification : 21 novembre 2026'."""
    texte = html_en_texte(source)
    bas = texte.lower()
    pos = bas.find("confirm")
    if pos < 0:
        raise ValueError("libellé 'confirmés' introuvable")

    def candidats(debut: int, fin: int) -> list[int]:
        trouves = []
        for m in re.finditer(r"\d{1,3}(?:[ .,]\d{3})+|\d{4,6}", texte[debut:fin]):
            avant = bas[max(0, debut + m.start() - 14):debut + m.start()]
            if any(mois in avant for mois in MOIS_FR) or any(mois in avant for mois in MOIS_EN):
                continue  # une année dans une date, pas le compteur
            trouves.append(entier_depuis(m.group(0)))
        return trouves

    apres = candidats(pos, len(texte))
    avant = candidats(0, pos)
    if apres:
        total = apres[0]
    elif avant:
        total = avant[-1]
    else:
        raise ValueError("nombre introuvable autour du libellé")
    date = None
    d = re.search(r"(?:derni[eè]re modification|last modified)\s*:?\s*(\d{1,2})\s+([a-zéû]+)\s+(\d{4})", bas)
    if d and d.group(2) in MOIS_FR:
        date = dt.date(int(d.group(3)), MOIS_FR[d.group(2)], int(d.group(1))).isoformat()
    return total, date


def analyser_accueil(source: str) -> tuple[int, str | None]:
    """Page d'accueil : 'Last update: Nov. 21, 2026 currently 8540 extrasolar objects.'"""
    texte = html_en_texte(source)
    m = re.search(r"currently\s+([\d][\d ,.]*?)\s+extrasolar objects", texte, re.I)
    if not m:
        raise ValueError("'currently N extrasolar objects' introuvable")
    total = entier_depuis(m.group(1))
    date = None
    d = re.search(r"Last update:\s*([A-Za-z]+)\.?\s+(\d{1,2}),\s*(\d{4})", texte)
    if d and d.group(1)[:3].lower() in MOIS_EN:
        date = dt.date(int(d.group(3)), MOIS_EN[d.group(1)[:3].lower()], int(d.group(2))).isoformat()
    return total, date


def lire_compteur(alertes: list[str], ancien: int | None = None) -> tuple[int, str | None, str] | None:
    for url, analyse, nom in ((URL_COMPTEUR, analyser_compteur, "compteur"),
                              (URL_ACCUEIL, analyser_accueil, "accueil")):
        try:
            total, date = analyse(telecharger(url))
            if not 1_000 <= total <= 200_000:
                raise ValueError(f"valeur aberrante {total}")
            if ancien and abs(total - ancien) > 0.2 * ancien:
                raise ValueError(f"saut suspect {ancien} -> {total}")
            return total, date, nom
        except Exception as e:  # noqa: BLE001
            alertes.append(f"exoplanet.eu ({nom}) : {e}")
    return None


def variation_sur(historique: dict[str, int], aujourd_hui: dt.date, jours: int, total: int) -> int | None:
    limite = (aujourd_hui - dt.timedelta(days=jours)).isoformat()
    anciennes = [d for d in historique if d <= limite]
    if not anciennes:
        return None
    return total - historique[max(anciennes)]


# --------------------------------------------------------------------------- lancements

def lieu_court(nom_lieu: str) -> str:
    bas = nom_lieu.lower()
    for cle, court in LIEUX_COURTS:
        if cle in bas:
            return court
    return nom_lieu.split(",")[0].strip()


def analyser_lancements(source: str, maintenant: dt.datetime, nombre: int, filtre: list[str]) -> list[dict]:
    resultats = json.loads(source).get("results") or []
    sortie = []
    for r in resultats:
        statut = (r.get("status") or {}).get("abbrev") or ""
        net = r.get("net")
        if not net or statut in STATUTS_TERMINES:
            continue
        quand = dt.datetime.fromisoformat(net.replace("Z", "+00:00"))
        if quand < maintenant - dt.timedelta(minutes=30):
            continue
        operateur = (r.get("launch_service_provider") or {}).get("name") or ""
        if filtre and not any(f.lower() in operateur.lower() for f in filtre):
            continue
        config = (r.get("rocket") or {}).get("configuration") or {}
        pad = r.get("pad") or {}
        lieu = (pad.get("location") or {}).get("name") or ""
        mission = (r.get("mission") or {}).get("name") or (r.get("name") or "").split("|")[-1].strip()
        mission = MISSIONS_FR.get(mission.strip().lower(), mission)
        sortie.append({
            "nom": r.get("name") or "",
            "fusee": config.get("name") or "",
            "fusee_complete": config.get("full_name") or "",
            "mission": mission,
            "operateur": operateur,
            "net": quand.astimezone(dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
            "statut": statut,
            "statut_fr": STATUTS_FR.get(statut, statut),
            "pas_de_tir": pad.get("name") or "",
            "lieu": lieu,
            "lieu_court": lieu_court(lieu) if lieu else "",
            "pays": (pad.get("location") or {}).get("country_code") or "",
        })
        if len(sortie) >= nombre:
            break
    return sortie


# --------------------------------------------------------------------------- Horizons

def horizons_distance_ua(cible: str, verif: str, jour: dt.date) -> float:
    params = {
        "format": "json", "COMMAND": f"'{cible}'", "OBJ_DATA": "'NO'", "MAKE_EPHEM": "'YES'",
        "EPHEM_TYPE": "'OBSERVER'", "CENTER": "'500@399'",
        "START_TIME": f"'{jour.isoformat()}'", "STOP_TIME": f"'{(jour + dt.timedelta(days=1)).isoformat()}'",
        "STEP_SIZE": "'1 d'", "QUANTITIES": "'20'", "CSV_FORMAT": "'YES'",
    }
    rep = json.loads(telecharger(f"{URL_HORIZONS}?{urllib.parse.urlencode(params)}"))
    return analyser_horizons(rep.get("result") or "", verif)


def analyser_horizons(resultat: str, verif: str) -> float:
    nom = re.search(r"Target body name:\s*([^\n{]+)", resultat)
    if not nom or verif.lower() not in nom.group(1).lower():
        raise ValueError(f"cible inattendue : {nom.group(1).strip() if nom else '?'}")
    bloc = re.search(r"\$\$SOE\s*(.*?)\s*\$\$EOE", resultat, re.S)
    if not bloc:
        raise ValueError("pas d'éphéméride (ligne $$SOE absente)")
    champs = [c.strip() for c in bloc.group(1).splitlines()[0].split(",")]
    nombres = [float(c) for c in champs[1:] if re.fullmatch(r"-?\d+(\.\d+)?(E[-+]?\d+)?", c, re.I)]
    if len(nombres) < 2:
        raise ValueError("colonnes delta/deldot introuvables")
    return nombres[0]


def formater_distance(km: float) -> tuple[str, str]:
    if km >= 1e9:
        return nombre_fr(km / 1e9, 1), "milliards de km"
    if km >= 1e6:
        v = km / 1e6
        return nombre_fr(v, 1 if v < 100 else 0), "millions de km"
    return nombre_fr(round(km, -2)), "km"


def formater_duree(secondes: float) -> tuple[str, str, str]:
    s = int(round(secondes))
    if s < 3600:
        m, s = divmod(s, 60)
        return f"{m}:{s:02d}", "min s", f"{m} min {s:02d} s"
    h, reste = divmod(s, 3600)
    m = reste // 60
    return f"{h}:{m:02d}", "h min", f"{h} h {m:02d} min"


# --------------------------------------------------------------------------- chiffre du jour

def calculer_chiffre(item: dict, jour: dt.date, total_exo: int | None) -> dict:
    t = item["type"]
    if t in ("distance", "temps_lumiere"):
        km = horizons_distance_ua(item["cible"], item["verif"], jour) * UA_KM
        if t == "distance":
            valeur, unite = formater_distance(km)
            texte = item["texte"].format(valeur=valeur, unite=unite)
        else:
            valeur, unite, duree = formater_duree(km / C_KM_S)
            texte = item["texte"].format(texte_duree=duree)
    elif t in ("jours_depuis", "jours_avant"):
        ref = dt.date.fromisoformat(item["date"])
        n = (jour - ref).days if t == "jours_depuis" else (ref - jour).days
        if n <= 0:
            raise ValueError("date dépassée")
        valeur, unite = nombre_fr(n), "jours"
        texte = item["texte"].format(valeur=valeur, annees=int(n // 365.25))
    elif t == "moyenne_exo":
        if not total_exo:
            raise ValueError("compteur indisponible")
        mois = (jour - dt.date.fromisoformat(item["depuis"])).days / 30.4375
        valeur, unite = nombre_fr(total_exo / mois, 0), "par mois"
        texte = item["texte"].format(valeur=valeur)
    elif t == "fixe":
        valeur, unite, texte = item["valeur"], item.get("unite", ""), item["texte"]
    else:
        raise ValueError(f"type inconnu {t}")
    return {"date": jour.isoformat(), "id": item["id"], "valeur": valeur, "unite": unite,
            "legende": item["legende"], "texte": texte}


def ordre_candidats(items: list[dict], vus: dict[str, str], jour: dt.date,
                    ecart: int, prioritaires: list[str]) -> list[dict]:
    def valide(it):
        return not (it["type"] == "jours_avant" and dt.date.fromisoformat(it["date"]) <= jour)

    def eligible(it):
        d = vus.get(it["id"])
        return d is None or (jour - dt.date.fromisoformat(d)).days >= ecart

    def cle(it):
        d = vus.get(it["id"])
        prio = prioritaires.index(it["id"]) if it["id"] in prioritaires else 999
        hachage = hashlib.sha1(it["id"].encode()).hexdigest()
        return (d is not None, d or "", prio, hachage)

    valides = [it for it in items if valide(it)]
    eligibles = sorted((it for it in valides if eligible(it)), key=cle)
    if eligibles:
        return eligibles
    return sorted(valides, key=cle)  # réserve trop petite : le plus ancien d'abord


def choisir_chiffre(etat: dict, items: list[dict], jour: dt.date, reglages: dict,
                    total_exo: int | None, alertes: list[str]) -> dict | None:
    courant = etat.get("chiffre_courant")
    if courant and courant.get("date") == jour.isoformat():
        return courant
    vus = etat.setdefault("chiffres_vus", {})
    for item in ordre_candidats(items, vus, jour, reglages["ecart_minimum_jours"],
                                reglages.get("commencer_par", [])):
        try:
            chiffre = calculer_chiffre(item, jour, total_exo)
        except Exception as e:  # noqa: BLE001
            alertes.append(f"chiffre du jour {item['id']} : {e}")
            continue
        vus[item["id"]] = jour.isoformat()
        etat["chiffre_courant"] = chiffre
        return chiffre
    return courant


# --------------------------------------------------------------------------- programme principal

def main() -> int:
    reglages = charger_json(CONFIG / "reglages.json", {})
    items = charger_json(CONFIG / "chiffres.json", {}).get("chiffres", [])
    etat = charger_json(ETAT, {})
    precedent = charger_json(SORTIE, {})
    alertes: list[str] = []

    fuseau = ZoneInfo(reglages.get("fuseau", "Europe/Paris"))
    maintenant = dt.datetime.now(dt.timezone.utc)
    jour = maintenant.astimezone(fuseau).date()

    # 1. Compteur
    compteur = lire_compteur(alertes, etat.get("dernier_total"))
    historique: dict[str, int] = etat.setdefault("historique", {})
    if compteur:
        total, date_catalogue, source = compteur
        ancien = etat.get("dernier_total")
        if ancien is not None and total != ancien:
            etat["derniere_variation"] = {
                "le": maintenant.strftime("%Y-%m-%dT%H:%M:%SZ"), "de": ancien, "a": total, "nombre": total - ancien}
        etat["dernier_total"] = total
        etat["date_catalogue"] = date_catalogue or etat.get("date_catalogue")
        etat["source_compteur"] = source
        historique[jour.isoformat()] = total
        for d in sorted(historique)[:-400]:
            del historique[d]
    total = etat.get("dernier_total")

    exo = {
        "total": total,
        "libelle": reglages.get("compteur", {}).get("libelle", "Objets extrasolaires confirmés"),
        "maj_catalogue": etat.get("date_catalogue"),
        "source": "exoplanet.eu (Observatoire de Paris)",
        "nouvelles_24h": variation_sur(historique, jour, 1, total) if total else None,
        "nouvelles_7j": variation_sur(historique, jour, 7, total) if total else None,
        "nouvelles_30j": variation_sur(historique, jour, 30, total) if total else None,
        "derniere_variation": etat.get("derniere_variation"),
        "historique_30j": [{"date": d, "total": historique[d]} for d in sorted(historique)[-30:]],
    }

    # 2. Lancements
    reg_l = reglages.get("lancements", {})
    try:
        lancements = analyser_lancements(telecharger(URL_LANCEMENTS), maintenant,
                                         reg_l.get("nombre", 5), reg_l.get("operateurs_filtre", []))
        etat["lancements_cache"] = lancements
    except Exception as e:  # noqa: BLE001
        alertes.append(f"Launch Library 2 : {e}")
        lancements = [l for l in etat.get("lancements_cache", [])
                      if dt.datetime.fromisoformat(l["net"].replace("Z", "+00:00")) > maintenant]

    # 3. Chiffre du jour
    chiffre = choisir_chiffre(etat, items, jour, reglages.get("chiffre_du_jour", {"ecart_minimum_jours": 40}),
                              total, alertes)

    donnees = {
        "version_format": 1,
        "genere_le": maintenant.strftime("%Y-%m-%dT%H:%M:%SZ"),
        "exoplanetes": exo,
        "chiffre_du_jour": chiffre,
        "lancements": lancements,
        "firmware": reglages.get("firmware", {"version": "0.0.0", "url": ""}),
        "alertes": alertes,
    }

    ecrire_json(ETAT, etat)
    sans_date = lambda d: {k: v for k, v in d.items() if k != "genere_le"}  # noqa: E731
    if sans_date(donnees) != sans_date(precedent):
        ecrire_json(SORTIE, donnees)
        print("data.json mis à jour")
    else:
        print("Aucun changement")
    for a in alertes:
        print(f"::warning::{a}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
