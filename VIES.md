# Les vies et le rejeu

Plan de la suite, arrêté avec le user le 10 septembre 2026. **Les §4.A — le
moteur — et §4.B — le bot — sont écrits depuis le 10 septembre 2026** ; C (le
banc) reste à faire, et avec lui la mesure qui dira si le §B paie. Pour tout le
reste, ce fichier est la spécification, pas un journal.

---

## 1. Ce qui change

Le jeu passe de « une partie, une mort » à **trois vies, et le niveau perdu se
rejoue**. Pour le bot, ça change de catégorie : il ne joue plus un coup unique
contre le hasard, il joue un **jeu répété avec information reportée**. Entre
deux tentatives, il autopsie sa manche et s'interdit le coup qui l'a tué.

### Le fait technique qui porte tout

```cpp
// Partie::nouvelleManche()
plat->reinitialiser(nbCasesBloquees(), grainePour(grainePartie, niveauCourant, CANAL_PLATEAU));
// La file repart neuve a chaque manche : c'est la condition pour qu'un
// niveau donne soit identique d'une partie a l'autre.
fil->reinitialiser(grainePour(grainePartie, niveauCourant, CANAL_FILE));
```

La file **et** le plateau dérivent de `(graine de partie, niveau)`. Rejouer un
niveau redonne donc la **même file, pièce pour pièce** : le rejeu est *parfait*.
Trois conséquences, et la deuxième est la plus sous-estimée.

1. **L'interdit est chirurgical.** Tout ce qui précède le carrefour est
   identique, donc le bot y arrive dans exactement le même état. Il n'a rien à
   généraliser : il rejoue, il bifurque à un endroit précis, et tout l'amont est
   du terrain déjà validé. C'est une recherche arborescente à N échantillons,
   pas de l'apprentissage.
2. **La malchance cesse d'exister à la reprise.** Sur `--graine 2420522734
   --niveau 18`, le bot attend quatorze gestes un vertical ou une croix et la
   file n'en livre aucun sur dix-neuf tirages (probabilité ≈ 1/550, tirage
   uniforme sur 7 types, `piecefile.cpp:56`). Au second essai il **sait** quelles
   pièces arrivent. L'attente n'est plus un pari, c'est un fait consultable.
3. **Revenir à la branche du premier essai reproduit sa mort à la case près.**
   Écarté pour cette raison : ce serait une vie jetée par construction.

---

## 2. Les règles

### Vies

- **3 au départ, plafond 9.**
- Perdre un niveau coûte une vie et **rejoue le même niveau**. Le compteur de
  niveau ne monte pas.
- Les points d'un niveau perdu sont **conservés**. Remise à zéro au game over
  seulement, en même temps que la mémoire des coups fatals.

### Gains

Deux sources, gardées toutes les deux parce qu'elles ne récompensent pas la même
chose.

| règle | nature | vies par partie (mesuré) |
|---|---|---|
| **tous les 20 000 points, palier mémorisé** (≈ 400 traversées) | rythme | **2,58** (max 5) |
| **belle manche réussie : ≥ 110 traversées** | pic | **3,25** (max 13) |

Total attendu : **3 + 2,58 + 3,25 ≈ 8,8 vies** sur une partie moyenne. Le plafond
de 9 ne mordra plus seulement sur les bonnes parties, il mordra couramment.

Les deux chiffres sont mesurés sur les 400 parties du banc **jouées sans vies** :
chacune s'arrête à la première défaite, niveau 19,05 en moyenne. Avec les vies
les parties iront plus loin, et comme les deux règles paient davantage en fin de
partie, 2,58 et 3,25 sont des planchers.

#### Le palier se mémorise

`prochainPalier` monte de 20 000 à chaque vie donnée et ne redescend jamais.
Franchir 20 000 paie une fois ; ensuite il faut atteindre 40 000, même si le
score est repassé sous 20 000 entre-temps.

Sans ce cliquet la même barre se paie plusieurs fois, parce que le score
**descend** : 25 points par écrasement (`PENALITE_REMPLACEMENT`, partie.cpp:15).
Finir un niveau à 20 100, écraser cinq pièces — 19 975 — et le premier tuyau
parcouru de la manche suivante repasse la barre. Mesuré sur les mêmes parties :

| | vies/partie | max |
|---|---|---|
| palier mémorisé | **2,58** | 5 |
| franchissement recompté | 3,13 | 8 |

**+21 %, et 173 parties sur 400 concernées.** Le repassage n'est pas un cas
limite : l'écrasement est un geste ordinaire du bot, et il y a en moyenne 0,55
redescente sous un palier déjà payé par partie. C'est aussi ce qui corrige le
« 3,18 (max 7) » de la version précédente de ce fichier : ce chiffre comptait les
franchissements, pas les paliers.

La règle des 20 000 reste celle qui **ne s'éteint jamais** : elle paie
proportionnellement au chemin parcouru. Sans elle il n'y a plus rien après le
niveau 13.

#### La belle manche n'est pas plate, elle monte

Seuil **absolu** de traversées, pas un multiple de l'objectif. La version
précédente la disait « remarquablement plate » sur 6,1 / 6,6 / 8,7 % — mais
l'analyse s'arrêtait au niveau 18, c'est-à-dire au niveau de mort moyen. Au-delà,
la fréquence décolle :

| niveaux | manches réussies | médiane traversées | ≥ 110 | ≥ 120 |
|---|---|---|---|---|
| 1-6 | 2390 | 62 | 11,6 % | 7,5 % |
| 7-12 | 2246 | 72 | 11,6 % | 6,9 % |
| 13-18 | 1697 | 89 | 18,2 % | 9,8 % |
| 19-24 | 758 | 107 | 43,5 % | 20,3 % |
| 25 et + | 127 | 125 | 96,1 % | 74,8 % |

Barème du seuil sur les 7 218 manches réussies mesurées :

| seuil | 100 | 105 | **110** | 115 | 120 | 130 |
|---|---|---|---|---|---|---|
| vies/partie | 5,00 | 4,03 | **3,25** | 2,48 | 1,88 | 1,03 |
| % des manches | 27,7 | 22,3 | **18,0** | 13,8 | 10,4 | 5,7 |

Passer de 120 à 110 fait donc 1,88 → 3,25 vies, et le gain tombe **tard** : 59 %
des belles manches sont au niveau 13 ou plus. C'est le point à assumer — à 110,
la belle manche n'est plus un pic, c'est une seconde règle de rythme, qui paie là
où la règle des points paie déjà. Elle paie là où le joueur en a besoin, mais le
plafond de 9 en mangera une partie.

#### Réservé aux manches réussies, sous peine de partie sans fin

Contrairement aux points, qui restent acquis même quand la manche est perdue, la
belle manche ne paie que sur une **manche réussie**. Ce n'est pas un scrupule de
barème, c'est une condition d'arrêt.

L'objectif atteint 110 au niveau 26 : au-delà, une manche **perdue** peut franchir
le seuil (5 cas mesurés). Or perdre rejoue le **même niveau**, avec la **même
file** (§1). Une manche qui perd en traversant 110 cases rendrait donc la vie
qu'elle vient de coûter, et elle le referait à chaque tentative : solde nul, la
partie ne se termine plus, elle tourne. Les vétos font bien diverger les essais,
mais rien ne garantit qu'ils fassent passer l'objectif — ils n'ont pas à porter
la condition d'arrêt.

D'où la règle générale à tenir pour tout futur gain : **aucune vie ne doit pouvoir
être gagnée deux fois sur le même état de jeu.** Les points la respectent par le
cliquet, la belle manche par la réussite — qui fait monter le niveau, donc change
la manche.

### La règle écartée : objectif × 2

Mesurée à **6,8 vies par partie, dont 5,4 avant le niveau 8**, puis **zéro dès
le niveau 19**. Elle s'éteint au niveau 13 — pas à cause de la géométrie (201
cases libres) mais parce que le tracé du bot grandit trop lentement : sa médiane
passe de 62 traversées aux niveaux 1-6 à 118 au-delà du niveau 25, soit ≈ 2,3 par
niveau, quand objectif × 2 en réclame 8.

Le bot trace ~65 cases dès le niveau 1 pour un objectif de 10, parce que la
manche ne s'arrête pas quand l'objectif est atteint mais quand le flux termine.
Donc ×2 récompensait la **petitesse de l'objectif**, pas la performance : ce
n'est pas le bot qui progresse, c'est la barre qui monte sous lui.

Écartée aussi, la vie par palier de points *au sein d'un niveau* : le maximum
théorique d'un niveau est 225 × 50 = 11 250, sous les 20 000.

---

## 3. La mémoire des coups fatals

### Forme

**`(case, entrée) → direction interdite`**, et non un numéro de geste : le
numéro meurt dès que le bot diverge, la paire survit. C'est aussi le vocabulaire
de `entreeTete` (bot.cpp), donc ça se lit dans la même langue que les
obligations.

Effacée au game over, comme les graines.

### Le blâme vise les carrefours, jamais le dernier geste

Une mort a souvent **deux causes distinctes**, et seule la seconde est un choix.
Sur `--graine 3468902482` (mort au niveau 19, 56/82) :

- **le refus** — tête `(14,1)`, `restant−1 = 25`, capacité du seul coup ouvert
  29, mais `culDeSac` exige `besoin = 35`. Le bot refuse le coup qui boucle la
  manche. Ce n'est pas une bifurcation : le rejouer donnerait le même refus.
- **l'engagement** — carrefour `(6,0)`, 60 gestes plus tôt : `gauche` offre 118
  cases, `droite` 41, il reste 38 à parcourir. Le bot prend 41. La capacité
  s'effondre et ne remonte jamais.

```
  carrefour  61 : restant=52  meilleure=174  marge=+123
  carrefour  71 : restant=38  meilleure=118  marge=+81
  carrefour  81 : restant=31  meilleure= 36  marge=+6
```

**Marqueur retenu** : la chute de la capacité atteignable (`espaceApres` non
borné, `maxi = 0`) rapportée à ce qu'il reste à faire. Calculable en direct,
pas seulement a posteriori.

### Pourquoi il choisit mal

`BotSpaceAnticp::choisirPont` renvoie **la première pièce de la file qui passe
le filtre**. Les deux voies passent le seuil, donc elles sont indiscernables et
l'ordre de la file tranche. Rien ne compare 118 à 41 — le critère ne connaît que
« ≥ besoin » contre « < besoin ».

### Mécanique des interdits

- Les vétos se **cumulent** d'un essai à l'autre.
- **Véto dur tant qu'il reste des vies à dépenser ; sur la dernière vie il
  cède** et redevient une préférence — le bot joue pour survivre, plus pour
  explorer. Sans ça il se suiciderait sur place au lieu de reprendre la branche
  connue.
- Un véto dur peut condamner la tête quand la direction interdite est la seule
  vivante, c'est-à-dire quand le carrefour est devenu une **case obligée** (voir
  `OBLIGATION.md`). Ce suicide est **informatif** : il prouve que le carrefour
  n'était pas une bifurcation mais un passage obligé.
- **Anti-empoisonnement** : un véto qui provoque le suicide **sort de
  l'ensemble** et est remplacé par un véto sur le carrefour précédent de la
  liste. Sinon il se redéclenche à chaque vie restante et le bot se suicide en
  boucle jusqu'au game over.
- La liste de blâme de l'essai 1 se **descend dans l'ordre**, une entrée
  consommée par mort, sans recalcul sur la partie qui vient de se jouer. Stable
  et prévisible. **La tronquer aux carrefours dont la chute est significative** :
  plus bas ce n'est que du bruit, et un véto sur un carrefour sans enjeu est
  arbitraire. Si la liste s'épuise avant les vies, rejouer sans véto plutôt
  qu'avec un véto au hasard.

---

## 4. Le plan, dans l'ordre

**Où on en est au 10 septembre 2026** : A fait et vérifié, B entier devant nous,
C amorcé. Le moteur tourne, les vies se gagnent et se dépensent — et elles ne
servent encore à rien au bot, mesuré 400 fois sur 400 (§C.2). C'est normal : le
rendement est tout entier dans B.

### A. Le moteur — fait

1. ✅ `Partie` : `viesRestantes` (3, plafond 9), `Partie::vies()` pour l'affichage.
2. ✅ `epGameOver` ajouté à côté d'`epPerdue` (`common.h`). `epPerdue` veut
   désormais dire « manche perdue, il reste des vies ».
3. ✅ Défaite → décrémenter, `nouvelleManche()` sans toucher à `niveauCourant`.
   Points conservés.
4. ✅ Plus de vie → `epGameOver`, puis `nouvellePartie()` à la fin de la pause :
   points, vies et palier repartent à zéro. La mémoire du bot viendra avec le §B.
5. ✅ Gains dans `Partie::crediterVies()` : `casesTraversees() >= 110` sur une
   manche **réussie** seulement (sur une manche perdue, le rejeu à l'identique
   rendrait la vie indéfiniment) ; paliers de 20 000 points avec
   `prochainPalier` **mémorisé et jamais décrémenté** — sinon les écrasements
   font repayer la même barre (+21 % de vies, §2). Plafonner à 9. Les gains sont
   crédités **avant** le décompte de la vie perdue : une manche qui franchit un
   palier en mourant paie la vie qu'elle est en train de perdre.
6. ✅ Affichage : une rangée de cœurs sous le numéro de niveau, calibrée pour que
   les neuf du plafond tiennent sur une ligne, et de hauteur constante pour que
   le panneau ne sautille pas à chaque vie gagnée.

### A bis. Ce que le rejeu a cassé au passage : `Partie::numeroManche()`

Le bot repérait la nouvelle manche à la **graine du plateau** (`bot.cpp`, avec le
commentaire « c'est le seul signal fiable »). C'était vrai tant qu'une défaite
relançait une partie neuve. Depuis le rejeu, la manche suivante a exactement la
même graine dérivée — le bot ne voyait donc plus rien : il gardait son tas, son
plan **et son `fonce`**, lançait le flux sur un plateau vide et brûlait ses sept
vies d'affilée sans poser une pièce.

Corrigé par un compteur de manches strictement croissant dans `Partie`, sur
lequel le bot se cale. À surveiller pour tout le reste du code : **plus rien de
dérivé de `(graine, niveau)` ne distingue deux manches.**

### B. Le bot — écrit le 10 septembre 2026, `botmemoire.cpp`

Une **sous-classe** du v3, `BotMemoire` (nom de stratégie `memoire`), et non une
modification de `BotSpaceAnticp` : `choisirPont` et `poseAcceptable` sont déjà
`virtual` et documentés comme le point d'extension, le §5 interdit de toucher à
`BotSpaceAnticp::jouer()`, et le §C.3 réclame un bras de référence intact pour la
mesure appariée. Fusion à envisager si le banc valide.

1. ✅ **Journal des carrefours** — à chaque nouvelle tête, la capacité de chaque
   direction vivante (`espaceApres(..., maxi = 0)`) et `restant`. Deux types qui
   débouchent du même côté sont un seul choix : le journal porte sur la
   DIRECTION, jamais sur le type — la croix traverse tout droit. Une tête à
   moins de deux directions vivantes n'est pas un carrefour et n'y entre pas.
   Refait seulement quand `signaturePlateau()` bouge, comme le marquage des
   obligations : la tête ne peut pas avoir changé sans ça.
2. ✅ **Fonction de blâme** — voir la correction du marqueur ci-dessous.
3. ✅ **Mémoire** — `(case, entrée) → direction interdite`, cumulée, effacée au
   **changement de niveau** et non seulement au game over (voir ci-dessous).
4. ✅ **Application** — véto dur dans `poseAcceptable` si `vies > 1` ; sur la
   dernière vie, `choisirPont` fait une première passe véto forcé puis accepte
   tout, ce qui en fait une simple préférence.
5. ✅ **Anti-empoisonnement** — un véto qui ne laisse aucun type prolonger la
   tête est levé sur-le-champ et reposé sur le premier carrefour amont dont la
   gravité passe le seuil ; s'il n'y en a pas, il est simplement abandonné —
   un véto sur un carrefour sans enjeu ne vaut pas mieux que pas de véto.

#### Le marqueur du §3 se lit décalé, pas au carrefour

**« La chute de la capacité atteignable rapportée à ce qu'il reste à faire »
n'est pas mesurable au carrefour lui-même.** Première version écrite et mesurée :
comparer les branches d'un même carrefour — ce que la branche abandonnée offrait
de plus que la branche prise. Résultat sur `--graine 1910881835 --niveau 31` :
**52 carrefours journalisés, 0 retenus**, et les trois tentatives identiques à la
case près (114 traversées chacune).

La raison : `espaceApres` inonde en 4-connexité, donc depuis n'importe quelle
direction on retombe sur la même grande poche. `meilleure` et `prise` sont égales
**48 fois sur 52**. Le choix ne se voit pas où il se fait, il se voit un
carrefour plus loin.

Le marqueur retenu est donc une **différence décalée** :

```
marge(i)   = meilleure(i) - restant(i)
gravité(i) = marge(i) - marge(i+1)
```

C'est la table du §3 lue dans le bon sens — `61 : +123`, `71 : +81`, `81 : +6`
donne gravité(71) = 75, et c'est bien le 71 qui a pris 41 quand 118 s'offrait.
Sur la manche mesurée, deux carrefours brûlent 33 et 31 cases de marge en un pas
quand les cinquante autres oscillent entre 0 et 2 : le fossé est franc, et
`SEUIL_BLAME` (10, calé sur `COUSSIN_CUL_DE_SAC`) tombe dedans sans réglage fin.

Le regret du carrefour n'a pas besoin d'un terme à part : une direction plus
étroite fait tomber `meilleure` au carrefour suivant, elle est déjà dans la
chute. Et le **dernier** carrefour reste à zéro — pas de successeur, donc pas de
chute, ce qui applique tout seul la règle « le blâme vise les carrefours, jamais
le dernier geste ».

#### L'oubli est avancé au changement de niveau

Le §3 borne l'oubli au game over. Le code l'avance au **niveau suivant**, qui
l'englobe : le véto porte sur une case d'un plateau donné, et le rejeu parfait —
toute sa raison d'être — ne vaut que *dans* un niveau. Le garder d'un niveau à
l'autre poserait un interdit arbitraire sur un terrain qui n'a plus rien à voir,
exactement ce que le §3 refuse pour un carrefour sans enjeu.

**Le piège du §A bis se repose un cran plus haut, et il vaut d'être écrit avant
de coder.** Le point 3 veut effacer la mémoire au game over : le bot doit donc
distinguer *manche neuve* de *partie neuve*. `numeroManche()` monte dans les
deux cas et ne suffit pas. Le signal de la partie, c'est `Partie::getGraine()`,
retirée au hasard par `nouvellePartie()`. Se tromper de signal ici ne se verrait
pas tout de suite — les vétos d'une partie morte survivraient à la suivante,
en la sabotant sur un plateau qui n'a plus rien à voir.

### C. Le banc

1. ✅ Les cinq outils de `banc/` s'arrêtent sur `epGameOver` et non plus sur
   `epPerdue` — sans quoi ils mesuraient « niveau du premier échec ».
   `banc/vies.cpp` déroule une partie manche par manche pour lire les vies.
2. **Les vies ne rapportaient rien au bot sans mémoire, et c'était mesuré** :
   400 parties appariées, niveau atteint **19,05 avec vies contre 19,05 sans**,
   **400 parties sur 400 identiques au niveau près**. Le rejeu étant parfait et
   le bot sans mémoire, chaque tentative reproduisait la même mort — exactement
   le §1.3. Tout le rendement des vies était donc dans le §B, pas dans le
   moteur. Relevés : `banc/vies-400.txt` et `banc/sans-vies-400.txt`.

   ✅ **Le §B le débloque, et c'est mesuré aussi.** 200 parties appariées,
   graines 1 à 200, `spaceAnticp` contre `memoire` :

   | | niveau moyen |
   |---|---|
   | spaceAnticp (v3) | 19,77 |
   | memoire (v4) | **23,41** |
   | écart | **+3,64**, t = +9,64 |

   **93 mieux, 0 moins bien, 107 égales**, gain maximum +21 niveaux. Le zéro
   n'est pas de la chance, il est *structurel* : les vétos s'effacent au
   changement de niveau, donc la première tentative d'un niveau donné est
   identique à celle du v3 — plateau et file dérivent de `(graine, niveau)`
   seuls. Le v4 ne peut qu'égaler ou dépasser, jamais perdre. Les 107 égalités
   sont les parties où la liste de blâme n'a rien trouvé à corriger.

   ⚠️ **Réserve de méthode** : mesuré avec un banc jetable qui reproduit la
   boucle de `MainWindow::battement` (bot avant `Partie::avancer`, `foncer()`
   qui coupe le jeu du bot pendant l'accélération), pas avec les outils de
   `banc/` — qui ne sont pas dans le dépôt. Le bras v3 y sort à 19,77 contre
   19,05 au §C.2, ce qui donne l'ordre de l'écart de méthode : bien plus petit
   que les 3,64 mesurés, mais pas nul. À refaire avec le vrai banc.
3. **Nouveau bras de référence.** `banc/reference-400.txt` et
   `banc/dynamique-400.txt` mesurent le niveau atteint sans vies et deviennent
   inexploitables pour comparer. La connaissance parfaite de la file au rejeu
   est un bond bien plus gros que les vies elles-mêmes.
4. Mesurer le **rendement des vies** : combien de tentatives supplémentaires
   font réellement progresser le niveau atteint, et combien ne font que rejouer
   la même mort. Le §C.2 en donne le solde global (+3,64) ; il reste à
   décomposer par tentative.
5. **Balayer `SEUIL_BLAME`.** Posé à 10 sans mesure, sur la seule observation
   que le bruit vit entre 0 et 2 et les coupables au-dessus de 30. C'est le
   premier réglage du v4 à passer au banc.

---

## 5. Ce qui n'est PAS au programme, et pourquoi

**On ne touche pas à `BotSpaceAnticp::jouer()`.** Le refus décrit au §3 est un
défaut réel, mais son remède naïf est mesuré perdant. Balayage complet de
`COUSSIN_CUL_DE_SAC`, 400 parties appariées par bras :

| coussin | 0 | 3 | 5 | **10 (actuel)** | 15 | 20 |
|---|---|---|---|---|---|---|
| niveau moyen | 15,06 | 17,99 | 18,53 | **19,05** | 19,04 | 18,75 |
| écart | −3,98 | −1,06 | −0,52 | — | −0,01 | −0,30 |
| t | −13,95 | −5,89 | −3,73 | — | −0,09 | −2,22 |

La valeur actuelle est à l'optimum, sur un plateau qui va jusqu'à 15 et
redescend à 20 — ce n'est pas un réglage sur un fil. La marge paie l'optimisme du flood
d'`espaceApres` — avoir tout juste `restant − 1` selon une majoration ne livre
pas `restant − 1` traversées réelles.

Le pari du user : **les vies corrigeront le refus d'elles-mêmes**, parce que le
refus ne mord que lorsque le bot s'est enfermé dans une branche étroite. Le
blâme pointe le carrefour, éviter le carrefour évite l'état où le refus fait
mal. Le remède est en amont du symptôme.

**Le motif à surveiller** : une mort causée *uniquement* par un refus, sans
mauvais carrefour en amont. Le refus n'étant pas une bifurcation, il n'entrera
jamais dans la liste de blâme et le rejeu le reproduira à l'identique jusqu'à
épuisement des vies. Signe visible : **un niveau où toutes les tentatives
meurent exactement au même endroit.**

---

## 6. Repères de code

| quoi | où |
|---|---|
| graines dérivées par niveau (le rejeu parfait) | `Partie::nouvelleManche()`, `grainePour()`, partie.cpp |
| états de partie, à scinder | `EEtatPartie`, common.h:38 |
| points par case, bonus de départ anticipé | `POINTS_PAR_CASE`, `POINTS_DEPART_ANTICIPE`, partie.cpp:15 |
| ce qui fait **descendre** le score, d'où le cliquet | `PENALITE_REMPLACEMENT` (25), `Partie::poserPiece()`, partie.cpp |
| relevé par manche et par palier (les mesures du §2) | `banc/manches.cpp`, `banc/paliers.cpp` |
| objectif du niveau | `Partie::longueurMinimale()`, partie.cpp |
| longueur réellement parcourue | `Partie::casesTraversees()` |
| tirage uniforme des 7 types | piecefile.cpp:56 |
| capacité atteignable d'une direction | `Bot::espaceApres(..., maxi = 0)`, bot.cpp |
| la marge et son coussin | `Bot::placeExigee()`, `COUSSIN_CUL_DE_SAC`, bot.cpp |
| le choix du pont, sans préférence | `BotSpaceAnticp::choisirPont()`, botspaceanticp.cpp |
| le journal, le blâme, les vétos, le désamorçage | `BotMemoire`, botmemoire.cpp |
| le seuil de bruit du blâme | `SEUIL_BLAME`, botmemoire.cpp |
| entrée imposée, même vocabulaire que les vétos | `Bot::entreeTete`, bot.cpp |
