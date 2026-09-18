#!/usr/bin/env python3
"""Dépouillement du journal de gestes, par origine de pose (colonne `origine`).

    qtiapipemania --niveau 35 --bombes 10 --journal mesure.csv
    ./banc/origines.py mesure.csv

Répond à une question et une seule : **une défausse finit-elle par servir ?**
C'est la thèse entière du plan de défausse (`Bot::construirePlan`) — une pièce
jetée est un billet de loterie, posée sur un circuit qu'on raccordera peut-être.
La colonne `remplie` dit si le billet gagne.

Le taux BRUT surestime, et c'est voulu côté journal : une case réécrite compte
pour chacune des poses qui l'ont visée, parce que chaque pose était bien un
geste. Pour mesurer une cadence c'est juste ; pour savoir si une défausse a payé,
non — la pièce jetée puis recouverte a été perdue, même si la case a fini
traversée. D'où le taux SERVI, qui compte l'écrasement comme un échec au lieu de
l'écarter : la pièce a bien été dépensée, elle appartient au dénominateur.

Une pose est écrasée si un geste accepté plus tard dans la même manche vise la
même case. Aucune colonne ne le dit ; ça se lit sur (manche, col, row).
"""

import argparse
import csv
import math
import sys
from collections import defaultdict

# Les codes de la colonne, tels que journal.h les définit.
LIBELLES = {
    0: "pose sur le tracé",
    1: "défausse selon le plan",
    2: "pré-pose de l'anticipation",
    3: "défausse sur un pari",
    4: "écrasement d'une case occupée",
}

# Sous ce taux de service, le plan de défausse ne peut battre aucune règle
# concurrente : il sort sans campagne. Au-dessus, seule une ablation appariée
# tranche, puisque ce qui compte est l'écart avec la défausse bête et non le
# taux lui-même. Seuil fixé AVANT la première mesure, le 18 septembre 2026.
SEUIL_DECISION = 0.15


class Geste:
    __slots__ = ("manche", "session", "niveau", "col", "row", "accepte",
                 "remplie", "origine", "ecrase")

    def __init__(self, ligne, avec_origine):
        self.manche = int(ligne["manche"])
        self.niveau = int(ligne["niveau"])
        self.col = int(ligne["col"])
        self.row = int(ligne["row"])
        self.accepte = ligne["accepte"] == "1"
        self.remplie = ligne["remplie"] == "1"
        self.origine = int(ligne["origine"]) if avec_origine else 0
        self.ecrase = False

    @property
    def servi(self):
        return self.remplie and not self.ecrase


def lire(chemins):
    gestes = []
    avec_origine = True

    for chemin in chemins:
        with open(chemin, newline="") as f:
            lecteur = csv.DictReader(f)

            if "origine" not in (lecteur.fieldnames or []):
                avec_origine = False

            # `Journal` ouvre en AJOUT : plusieurs lancements s'accumulent dans
            # un même fichier, et le compteur de manches repart à 1 à chaque
            # fois. Sans ce découpage, la manche 1 de la première session et
            # celle de la seconde seraient confondues — et l'écrasement, qui se
            # lit sur (manche, col, row), deviendrait n'importe quoi.
            session = 0
            precedente = None

            for ligne in lecteur:
                g = Geste(ligne, "origine" in (lecteur.fieldnames or []))

                # Une BAISSE, pas une égalité : toutes les lignes d'une même
                # manche portent son numéro.
                if precedente is not None and g.manche < precedente:
                    session += 1

                precedente = g.manche
                g.manche = (chemin, session, g.manche)
                g.session = (chemin, session)
                gestes.append(g)

    return gestes, avec_origine


def marquer_ecrases(gestes):
    """Sur une case donnée d'une manche donnée, seule la dernière pose tient."""
    derniere = {}

    for i, g in enumerate(gestes):
        if g.accepte:
            derniere[(g.manche, g.col, g.row)] = i

    for i, g in enumerate(gestes):
        if g.accepte and derniere[(g.manche, g.col, g.row)] != i:
            g.ecrase = True


def taux(gestes):
    """Proportion servie, et son erreur-type binomiale."""
    n = len(gestes)

    if n == 0:
        return 0.0, 0.0, 0

    p = sum(1 for g in gestes if g.servi) / n
    return p, math.sqrt(p * (1 - p) / n), n


def table_origines(acceptes):
    par = defaultdict(list)

    for g in acceptes:
        par[g.origine].append(g)

    total = len(acceptes)

    print("| origine | gestes | part | remplie (brut) | servi (corrigé) | ± |")
    print("|---|---:|---:|---:|---:|---:|")

    for code in sorted(par):
        lot = par[code]
        brut = sum(1 for g in lot if g.remplie) / len(lot)
        p, se, n = taux(lot)
        print(f"| {code} — {LIBELLES.get(code, '?')} | {n} | "
              f"{100 * n / total:.1f} % | {100 * brut:.1f} % | "
              f"{100 * p:.1f} % | {100 * se:.1f} |")

    p, se, n = taux(acceptes)
    print(f"| **ensemble** | **{n}** | 100 % | "
          f"{100 * sum(1 for g in acceptes if g.remplie) / n:.1f} % | "
          f"**{100 * p:.1f} %** | {100 * se:.1f} |")


def table_par_niveau(acceptes, code):
    par = defaultdict(list)

    for g in acceptes:
        if g.origine == code:
            par[g.niveau].append(g)

    if not par:
        return

    print(f"\n### Origine {code} — {LIBELLES.get(code, '?')}, par niveau\n")
    print("| niveau | gestes | servi | ± |")
    print("|---:|---:|---:|---:|")

    for niveau in sorted(par):
        p, se, n = taux(par[niveau])
        print(f"| {niveau} | {n} | {100 * p:.1f} % | {100 * se:.1f} |")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("csv", nargs="+", help="un ou plusieurs journaux de gestes")
    ap.add_argument("--niveau-min", type=int, default=0,
                    help="ne garder que les gestes à partir de ce niveau")
    ap.add_argument("--session", type=int, default=None,
                    help="ne garder que cette session du fichier (0 = la première)")
    args = ap.parse_args()

    gestes, avec_origine = lire(args.csv)

    if not gestes:
        sys.exit("aucun geste lu")

    marquer_ecrases(gestes)

    sessions = sorted(set(g.session for g in gestes))

    if len(sessions) > 1:
        print(f"> {len(sessions)} sessions dans ce fichier "
              f"(le compteur de manches repart à 1 à chaque lancement). "
              f"`--session N` pour n'en garder qu'une.\n")

    if args.session is not None:
        if args.session >= len(sessions):
            sys.exit(f"session {args.session} inconnue : il y en a {len(sessions)}")

        gestes = [g for g in gestes if g.session == sessions[args.session]]

    if args.niveau_min:
        gestes = [g for g in gestes if g.niveau >= args.niveau_min]

    acceptes = [g for g in gestes if g.accepte]
    refuses = len(gestes) - len(acceptes)

    if not acceptes:
        sys.exit("aucun geste accepté")

    print(f"# Journal — {len(gestes)} gestes, "
          f"{len(set(g.manche for g in gestes))} manches, "
          f"niveaux {min(g.niveau for g in gestes)} à {max(g.niveau for g in gestes)}\n")

    if not avec_origine:
        print("> Ce journal est **d'avant la colonne `origine`** : tout est "
              "compté en 0. Les taux restent lisibles, la ventilation non.\n")

    if refuses:
        print(f"Gestes refusés : {refuses} "
              f"({100 * refuses / len(gestes):.1f} % du total). Ils coûtent un "
              f"geste et ne posent rien ; ils sont hors des tables.\n")

    ecrases = sum(1 for g in acceptes if g.ecrase)
    print(f"Poses écrasées plus tard dans leur manche : {ecrases} "
          f"({100 * ecrases / len(acceptes):.1f} %). C'est l'écart entre le "
          f"brut et le corrigé.\n")

    table_origines(acceptes)
    table_par_niveau(acceptes, 1)

    lot = [g for g in acceptes if g.origine == 1]

    if lot:
        p, se, n = taux(lot)
        print(f"\n## Décision\n")
        print(f"Taux de service de la défausse selon le plan : "
              f"**{100 * p:.1f} %** ± {100 * se:.1f} sur {n} poses.")
        print(f"Seuil fixé d'avance : {100 * SEUIL_DECISION:.0f} %.\n")

        if p + 1.96 * se < SEUIL_DECISION:
            print("Sous le seuil, intervalle compris : le plan de défausse ne "
                  "repêche presque rien. **Il sort**, sans campagne.")
        elif p - 1.96 * se > SEUIL_DECISION:
            print("Au-dessus du seuil, intervalle compris : le plan repêche "
                  "assez pour qu'on ne puisse pas le condamner ici. **Ablation "
                  "appariée nécessaire** — c'est l'écart avec la défausse bête "
                  "qui décide, pas ce taux.")
        else:
            print("L'intervalle recouvre le seuil : **pas assez de poses pour "
                  "trancher**. Prendre plus de manches avant de décider.")


if __name__ == "__main__":
    main()
