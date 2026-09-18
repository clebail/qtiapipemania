#!/usr/bin/env python3
"""Comparaison appariée des deux bancs : même graine, même niveau de départ,
donc l'écart se lit à variance très réduite (cf. depouille.py)."""
import math
import sys


def lire(chemin, champ=0):
    """Une ligne par partie. Une ou deux colonnes (niveau, puis score) :
    on ne lit que celle qu'on demande, pour que les relevés d'avant restent
    comparables à ceux d'après."""
    valeurs = []
    with open(chemin) as f:
        for ligne in f:
            morceaux = ligne.split()
            if len(morceaux) > champ and morceaux[champ].lstrip("-").isdigit():
                valeurs.append(int(morceaux[champ]))
    return valeurs


def moyenne(xs):
    return sum(xs) / len(xs) if xs else 0.0


def ecart_type(xs):
    if len(xs) < 2:
        return 0.0
    m = moyenne(xs)
    return math.sqrt(sum((x - m) ** 2 for x in xs) / (len(xs) - 1))


def mediane(xs):
    ys = sorted(xs)
    n = len(ys)
    if n == 0:
        return 0.0
    return ys[n // 2] if n % 2 else (ys[n // 2 - 1] + ys[n // 2]) / 2


champ = int(sys.argv[3]) if len(sys.argv) > 3 else 0
av = lire(sys.argv[1], champ)
ap = lire(sys.argv[2], champ)
n = min(len(av), len(ap))
av, ap = av[:n], ap[:n]

print(f"parties appariées : {n}   ({'score' if champ else 'niveau'})\n")
print(f"{'':22} {'avant':>8} {'après':>8}")
print(f"{'moyenne':22} {moyenne(av):>8.2f} {moyenne(ap):>8.2f}")
print(f"{'médiane':22} {mediane(av):>8.1f} {mediane(ap):>8.1f}")
print(f"{'écart-type':22} {ecart_type(av):>8.2f} {ecart_type(ap):>8.2f}")
print(f"{'min / max':22} {min(av):>4}/{max(av):<3} {min(ap):>4}/{max(ap):<3}")

d = [b - a for a, b in zip(av, ap)]
md = moyenne(d)
se = ecart_type(d) / math.sqrt(n) if n else 0.0

print(f"\nécart apparié (après - avant)")
print(f"  moyenne      : {md:+.3f} niveau")
print(f"  erreur-type  : {se:.3f}")
if se > 0:
    t = md / se
    print(f"  t            : {t:+.2f}   (|t| > 2 ≈ significatif à 5 %)")
    print(f"  IC 95 %      : [{md - 1.96 * se:+.3f}, {md + 1.96 * se:+.3f}]")

mieux = sum(1 for x in d if x > 0)
pire = sum(1 for x in d if x < 0)
print(f"\n  parties améliorées : {mieux}")
print(f"  parties dégradées  : {pire}")
print(f"  inchangées         : {n - mieux - pire}")
