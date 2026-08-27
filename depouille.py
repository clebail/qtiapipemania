#!/usr/bin/env python3
"""Dépouillement du CSV du banc d'essai (point 7 de BOT.md).

    qtiapipemania --bench --bot space --parties 1000 \
        --cadences 1,1.5,2,2.5,3,4,5 > space.csv
    ./depouille.py space.csv

Le banc ne sort aucune agrégation : les mille lignes sont la donnée, et le
résumé se calcule ici. Sortie en Markdown, directement collable dans BOT.md.

Deux séries ou plus se comparent de façon appariée — le banc joue les mêmes
graines à toutes les cadences et pour tous les bots, donc l'écart se lit à
variance très réduite. C'est la seule lecture valable d'une comparaison :
une différence de moyennes non appariée se noie dans le bruit de tirage.
"""

import argparse
import csv
import math
import os
import sys
from collections import defaultdict

# Au-delà, la table de survie devient illisible ; on coupe et on le dit.
MAX_COLONNES_SURVIE = 18
# Un niveau n'ouvre une colonne que si quelqu'un le passe encore un peu.
SEUIL_COLONNE_SURVIE = 0.01


# --- Lecture -----------------------------------------------------------------

class Partie:
    __slots__ = ("cadence", "graine", "niveau", "score", "cases", "cause",
                 "mures", "remplacements", "region_fin", "manque_fin")

    def __init__(self, ligne):
        self.cadence = float(ligne["cadence"])
        self.graine = int(ligne["graine"])
        self.niveau = int(ligne["niveau"])
        self.score = int(ligne["score"])
        self.cases = int(ligne["cases"])
        self.cause = ligne["cause"]
        # Manches finies tete morte, et remplacements payes, sur toute la partie.
        self.mures = int(ligne["mures"])
        self.remplacements = int(ligne["remplacements"])
        # Manche fatale : place encore atteignable, et ce qu'il manquait pour
        # tenir l'objectif. region_fin < manque_fin = enfermement au sens fort.
        self.region_fin = int(ligne["region_fin"])
        self.manque_fin = int(ligne["manque_fin"])

    @property
    def censuree(self):
        """Arrêtée par le banc, pas par le jeu : le niveau réel est plus haut."""
        return self.cause != "perdu"


class Serie:
    """Un fichier du banc : les parties, indexées par cadence puis par graine."""

    def __init__(self, nom):
        self.nom = nom
        self.parties = defaultdict(dict)   # cadence -> graine -> Partie
        self.doublons = 0

    @property
    def cadences(self):
        return sorted(self.parties)

    def ajouter(self, partie):
        if partie.graine in self.parties[partie.cadence]:
            self.doublons += 1
        self.parties[partie.cadence][partie.graine] = partie

    def a(self, cadence):
        return list(self.parties[cadence].values())


ENTETE = ["cadence", "graine", "niveau", "score", "cases", "cause",
          "mures", "remplacements", "region_fin", "manque_fin"]


def lire(chemin):
    serie = Serie(os.path.splitext(os.path.basename(chemin))[0] if chemin != "-" else "stdin")

    try:
        flux = sys.stdin if chemin == "-" else open(chemin, newline="")
    except OSError as erreur:
        raise SystemExit("%s : %s" % (chemin, erreur.strerror))

    try:
        lecteur = csv.DictReader(flux)
        if lecteur.fieldnames != ENTETE:
            raise SystemExit("%s : en-tête inattendue %s, attendu %s"
                             % (serie.nom, lecteur.fieldnames, ENTETE))

        for numero, ligne in enumerate(lecteur, start=2):
            try:
                serie.ajouter(Partie(ligne))
            except (TypeError, ValueError, KeyError) as erreur:
                raise SystemExit("%s ligne %d : %s" % (serie.nom, numero, erreur))
    finally:
        if flux is not sys.stdin:
            flux.close()

    if not serie.parties:
        raise SystemExit("%s : aucune partie." % serie.nom)

    return serie


# --- Statistiques ------------------------------------------------------------

def moyenne(xs):
    return sum(xs) / len(xs)


def erreur_type(xs):
    """Écart-type de la moyenne. Deux erreurs-types ≈ intervalle à 95 %."""
    if len(xs) < 2:
        return 0.0
    m = moyenne(xs)
    variance = sum((x - m) ** 2 for x in xs) / (len(xs) - 1)
    return math.sqrt(variance / len(xs))


def mediane(xs):
    tries = sorted(xs)
    milieu = len(tries) // 2
    if len(tries) % 2:
        return float(tries[milieu])
    return (tries[milieu - 1] + tries[milieu]) / 2


def apparier(gauche, droite):
    """Écarts droite − gauche sur les seules graines jouées des deux côtés."""
    communes = sorted(set(gauche) & set(droite))
    return [droite[g].niveau - gauche[g].niveau for g in communes]


# --- Mise en forme -----------------------------------------------------------

def nombre(x, decimales=2):
    """Virgule décimale et vrai signe moins, comme le reste de BOT.md."""
    return ("%.*f" % (decimales, x)).replace(".", ",").replace("-", "\u2212")


def cadence_str(c):
    return nombre(c, 0) if c == int(c) else nombre(c, 1)


def tableau(entetes, lignes):
    largeurs = [len(e) for e in entetes]
    for ligne in lignes:
        for i, cellule in enumerate(ligne):
            largeurs[i] = max(largeurs[i], len(cellule))

    rendu = ["| " + " | ".join(e.ljust(largeurs[i]) for i, e in enumerate(entetes)) + " |",
             "|" + "|".join("-" * (largeurs[i] + 2) for i in range(len(entetes))) + "|"]
    for ligne in lignes:
        rendu.append("| " + " | ".join(c.ljust(largeurs[i]) for i, c in enumerate(ligne)) + " |")
    return "\n".join(rendu)


# --- Rapport -----------------------------------------------------------------

def resume(serie):
    lignes = []
    for cadence in serie.cadences:
        parties = serie.a(cadence)
        niveaux = [p.niveau for p in parties]
        censurees = sum(1 for p in parties if p.censuree)

        lignes.append([
            cadence_str(cadence),
            str(len(parties)),
            nombre(moyenne(niveaux)),
            "± " + nombre(2 * erreur_type(niveaux)),
            nombre(mediane(niveaux), 1),
            str(max(niveaux)),
            nombre(moyenne([p.cases for p in parties]), 1),
            nombre(moyenne([p.score for p in parties]), 0),
            "%d" % censurees,
        ])

    return tableau(["cadence", "parties", "niveau moyen", "± 2 e.t.", "médiane",
                    "max", "cases moy.", "score moyen", "arrêtées"], lignes)


def survie(serie, niveaux_demandes):
    """Part des parties qui **passent** le niveau N.

    Le banc écrit le niveau où la partie s'est arrêtée, donc le premier échoué :
    une partie de niveau k a franchi 1..k−1. Une partie arrêtée par le plafond
    compte comme survivante partout — elle n'a échoué nulle part.
    """
    plafond = max(p.niveau for cadence in serie.cadences for p in serie.a(cadence))

    if niveaux_demandes:
        colonnes = list(range(1, niveaux_demandes + 1))
    else:
        colonnes = []
        for n in range(1, plafond + 1):
            if any(sum(1 for p in serie.a(c) if p.niveau > n) / len(serie.a(c))
                   >= SEUIL_COLONNE_SURVIE for c in serie.cadences):
                colonnes.append(n)
            else:
                break
        colonnes = colonnes[:MAX_COLONNES_SURVIE] or [1]

    lignes = []
    for cadence in serie.cadences:
        parties = serie.a(cadence)
        taux = [sum(1 for p in parties if p.niveau > n) / len(parties) for n in colonnes]
        lignes.append([cadence_str(cadence)] + [nombre(100 * t, 0) + " %" for t in taux])

    tronquee = niveaux_demandes is None and colonnes[-1] < plafond - 1
    return tableau(["cadence"] + [str(n) for n in colonnes], lignes), tronquee


def ecarts_internes(serie):
    """Ce que rapporte le passage d'une cadence à la suivante, apparié."""
    cadences = serie.cadences
    if len(cadences) < 2:
        return None

    lignes = []
    for basse, haute in zip(cadences, cadences[1:]):
        diffs = apparier(serie.parties[basse], serie.parties[haute])
        if not diffs:
            continue
        lignes.append([
            "%s → %s" % (cadence_str(basse), cadence_str(haute)),
            str(len(diffs)),
            ("+" if moyenne(diffs) >= 0 else "") + nombre(moyenne(diffs)),
            "± " + nombre(2 * erreur_type(diffs)),
        ])

    return tableau(["cadences", "graines", "écart de niveau", "± 2 e.t."], lignes) if lignes else None


def ecarts_entre_series(reference, autre):
    lignes = []
    for cadence in sorted(set(reference.cadences) & set(autre.cadences)):
        diffs = apparier(reference.parties[cadence], autre.parties[cadence])
        if not diffs:
            continue
        lignes.append([
            cadence_str(cadence),
            str(len(diffs)),
            nombre(moyenne([p.niveau for p in reference.a(cadence)])),
            nombre(moyenne([p.niveau for p in autre.a(cadence)])),
            ("+" if moyenne(diffs) >= 0 else "") + nombre(moyenne(diffs)),
            "± " + nombre(2 * erreur_type(diffs)),
        ])

    if not lignes:
        return None
    return tableau(["cadence", "graines", reference.nom, autre.nom, "écart apparié", "± 2 e.t."], lignes)


def rapport(series, niveaux_demandes, sortie):
    ecrire = lambda *args: print(*args, file=sortie)

    for serie in series:
        ecrire("## %s\n" % serie.nom)
        ecrire(resume(serie))

        censurees = sum(1 for c in serie.cadences for p in serie.a(c) if p.censuree)
        if censurees:
            ecrire("\n*%d parties arrêtées par le banc (plafond de niveau ou de ticks) : "
                   "leur niveau réel est plus haut, les moyennes de ces cadences sont "
                   "donc minorées.*" % censurees)
        if serie.doublons:
            ecrire("\n*%d lignes de graine déjà vue, ignorées au profit de la dernière : "
                   "le fichier concatène des runs qui se recouvrent.*" % serie.doublons)

        ecrire("\n### Taux de survie — part des parties qui franchissent le niveau\n")
        table, tronquee = survie(serie, niveaux_demandes)
        ecrire(table)
        if tronquee:
            ecrire("\n*Colonnes coupées là où la survie tombe sous 1 %.*")

        interne = ecarts_internes(serie)
        if interne:
            ecrire("\n### Ce que rapporte la cadence, à graines appariées\n")
            ecrire(interne)
        ecrire("")

    for autre in series[1:]:
        table = ecarts_entre_series(series[0], autre)
        if table:
            ecrire("## %s contre %s, apparié\n" % (autre.nom, series[0].nom))
            ecrire(table)
            ecrire("")


def main():
    options = argparse.ArgumentParser(
        description="Dépouille le CSV du banc (qtiapipemania --bench).",
        epilog="Plusieurs fichiers : le premier sert de référence, les autres lui "
               "sont comparés à graines appariées.")
    options.add_argument("fichiers", nargs="*", default=["-"],
                         help="CSV du banc (défaut : entrée standard).")
    options.add_argument("--niveaux", type=int, metavar="N",
                         help="Nombre de niveaux dans la table de survie "
                              "(défaut : jusqu'à ce que la survie tombe sous 1 %%).")
    options.add_argument("--sortie", metavar="FICHIER",
                         help="Écrire le rapport là plutôt que sur la sortie standard.")
    arguments = options.parse_args()

    if arguments.niveaux is not None and arguments.niveaux <= 0:
        raise SystemExit("--niveaux attend un entier positif.")

    series = [lire(chemin) for chemin in arguments.fichiers]

    if arguments.sortie:
        with open(arguments.sortie, "w") as flux:
            rapport(series, arguments.niveaux, flux)
    else:
        rapport(series, arguments.niveaux, sys.stdout)


if __name__ == "__main__":
    main()
