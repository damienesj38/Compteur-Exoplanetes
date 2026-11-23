"""Tests hors ligne : python3 -m unittest discover tests"""

import datetime as dt
import json
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "scripts"))
import update_data as u  # noqa: E402

FIX = Path(__file__).resolve().parent / "fixtures"


def lire(nom):
    return (FIX / nom).read_text(encoding="utf-8")


class Compteur(unittest.TestCase):
    def test_page_scholar(self):
        self.assertEqual(u.analyser_compteur(lire("compteur.html")), (8540, "2026-11-21"))

    def test_page_scholar_avec_espace_insecable(self):
        html = lire("compteur.html").replace("8540", "8&nbsp;540")
        self.assertEqual(u.analyser_compteur(html)[0], 8540)

    def test_nombre_avant_le_libelle(self):
        html = "<h2>8540</h2><h5>Objets extrasolaires confirmés</h5><p>dernière modification : 21 novembre 2026</p>"
        self.assertEqual(u.analyser_compteur(html), (8540, "2026-11-21"))

    def test_annee_jamais_prise_pour_le_compteur(self):
        html = "<h5>Objets extrasolaires confirmés</h5><p>dernière modification : 21 novembre 2026</p>"
        with self.assertRaises(ValueError):
            u.analyser_compteur(html)

    def test_page_accueil(self):
        self.assertEqual(u.analyser_accueil(lire("accueil.html")), (8540, "2026-11-21"))

    def test_variation(self):
        h = {"2026-11-10": 8500, "2026-11-15": 8526, "2026-11-21": 8538}
        jour = dt.date(2026, 11, 22)
        self.assertEqual(u.variation_sur(h, jour, 7, 8540), 14)
        self.assertEqual(u.variation_sur(h, jour, 1, 8540), 2)
        self.assertIsNone(u.variation_sur(h, jour, 30, 8540))


class Lancements(unittest.TestCase):
    def test_filtrage_et_champs(self):
        maintenant = dt.datetime(2026, 11, 22, 20, 8, tzinfo=dt.timezone.utc)
        l = u.analyser_lancements(lire("lancements.json"), maintenant, 5, [])
        self.assertEqual([x["mission"] for x in l], ["Amazon Leo (LE-04)", "Progress MS-36 (97P)", "USSF-266"])
        self.assertEqual(l[0]["fusee"], "Ariane 64")
        self.assertEqual(l[0]["lieu_court"], "Kourou")
        self.assertEqual(l[0]["statut_fr"], "Feu vert")
        self.assertEqual(l[1]["lieu_court"], "Baïkonour")
        self.assertEqual(l[2]["lieu_court"], "Vandenberg")

    def test_filtre_operateur(self):
        maintenant = dt.datetime(2026, 11, 22, 20, 8, tzinfo=dt.timezone.utc)
        l = u.analyser_lancements(lire("lancements.json"), maintenant, 5, ["spacex"])
        self.assertEqual([x["operateur"] for x in l], ["SpaceX"])


class Horizons(unittest.TestCase):
    def test_lecture(self):
        resultat = json.loads(lire("horizons_voyager1.json"))["result"]
        self.assertAlmostEqual(u.analyser_horizons(resultat, "voyager 1"), 173.185823726554)

    def test_mauvaise_cible_refusee(self):
        resultat = json.loads(lire("horizons_voyager1.json"))["result"]
        with self.assertRaises(ValueError):
            u.analyser_horizons(resultat, "voyager 2")

    def test_formats(self):
        km = 173.185823726554 * u.UA_KM
        self.assertEqual(u.formater_distance(km), ("25,9", "milliards de km"))
        self.assertEqual(u.formater_duree(km / u.C_KM_S)[0], "24:00")
        self.assertEqual(u.formater_distance(384_400), ("384 400", "km"))
        self.assertEqual(u.formater_distance(628_000_000), ("628", "millions de km"))
        self.assertEqual(u.formater_duree(0.9875 * u.UA_KM / u.C_KM_S), ("8:13", "min s", "8 min 13 s"))


class ChiffreDuJour(unittest.TestCase):
    def setUp(self):
        self.items = json.loads((u.CONFIG / "chiffres.json").read_text(encoding="utf-8"))["chiffres"]
        u.horizons_distance_ua = lambda cible, verif, jour: 5.2  # pas de réseau

    def test_reserve_suffisante(self):
        self.assertGreaterEqual(len(self.items), 41)
        self.assertEqual(len({i["id"] for i in self.items}), len(self.items))

    def test_rotation_sans_repetition_40_jours(self):
        etat, alertes = {}, []
        reglages = {"ecart_minimum_jours": 40, "commencer_par": ["voyager1_distance"]}
        vus = []
        jour = dt.date(2026, 11, 23)
        for i in range(400):
            c = u.choisir_chiffre(etat, self.items, jour + dt.timedelta(days=i), reglages, 8540, alertes)
            vus.append(c["id"])
        self.assertEqual(vus[0], "voyager1_distance")
        for i, ident in enumerate(vus):
            self.assertNotIn(ident, vus[max(0, i - 40):i], f"{ident} revient trop tôt (jour {i})")
        self.assertEqual(alertes, [])

    def test_meme_chiffre_toute_la_journee(self):
        etat, reglages = {}, {"ecart_minimum_jours": 40}
        jour = dt.date(2026, 11, 23)
        a = u.choisir_chiffre(etat, self.items, jour, reglages, 8540, [])
        b = u.choisir_chiffre(etat, self.items, jour, reglages, 8540, [])
        self.assertEqual(a, b)

    def test_tous_les_textes_se_formatent(self):
        for item in self.items:
            c = u.calculer_chiffre(item, dt.date(2026, 11, 23), 8540)
            self.assertNotIn("{", c["texte"], item["id"])
            self.assertLessEqual(len(c["legende"]), 32, item["id"])


if __name__ == "__main__":
    unittest.main()
