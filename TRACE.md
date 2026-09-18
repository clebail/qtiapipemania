# Le tracé planifié

Un bot qui choisit **au début du niveau le tracé qui atteint l'objectif**, puis
pose les pièces dessus, au lieu de construire de proche en proche devant le flux.

Document ouvert le 18 septembre 2026. Le §2 est mesuré, le reste est raisonné et
attend son code.

---

## 1. Ce qui change

Les bots existants (`glouton` → `space` → `spaceAnticp` → `spaceLarge` →
`memoire`) partagent tous la même forme : ils regardent la tête de construction,
la pièce du haut de file, et décident *ici et maintenant*. Ce qu'ils savent du
futur tient dans un pont de quelques cases (`BotSpaceAnticp::choisirPont`) et
dans un flood-fill qui mesure la place devant (`Bot::tailleRegionVide`).

Un tracé renverse ça. Le chemin complet du réservoir jusqu'à l'objectif est
calculé une fois, sur le plateau de la manche, et chaque case du tracé a dès lors
**son type déterminé** — puisque le type d'une case se déduit entièrement de son
couple (entrée, sortie). Le bot ne décide plus où aller ; il affecte des pièces à
des cases.

### Le fait technique qui porte tout

Sans tracé, la pièce en main est utilisable **à un seul endroit** : la tête. Elle
y passe 4 fois sur 7 (les quatre types qui ont une ouverture sur l'entrée), et
sinon elle part à la défausse.

Avec un tracé, elle est utilisable **partout où le tracé réclame son type**. La
contrainte cesse d'être temporelle (« il me faut ça, ici, maintenant ») et
devient une affectation (« il me faut ça, quelque part »). C'est un changement de
nature, et c'est de là que vient tout le reste de ce document.

---

## 2. La mesure d'avant — ce qu'on cherche à battre

Faite le 18 septembre 2026, avant d'écrire une ligne du planificateur. Sans elle
on argumenterait dans le vide : on saurait qu'un tracé gaspille moins, pas
combien.

### L'instrument

- **Colonne `origine` du journal de gestes** (`journal.h`). Cinq codes :
  `0` pose sur le tracé, `1` défausse selon le plan de défausse, `2` pré-pose de
  l'anticipation, `3` défausse sur un pari, `4` écrasement d'une case occupée.
  Le bot ne déclare rien : `MainWindow::battement` relève les marques du tas
  (`Bot::origineTas`) **avant** le geste et compare, parce que la marque dit ce
  que la case porte et non qui vient de poser — sans l'avant, le tracé qui
  reprend un rebut se compterait comme une défausse.
- **`banc/gestes.cpp`** : le journal sans fenêtre ni écran, même boucle que
  `banc/bench.cpp`. Quatre secondes par partie, et le niveau de départ est un
  argument — une prise à l'écran mettrait plus d'une heure d'horloge à atteindre
  le régime qui nous intéresse.
- **`banc/origines.py`** : la ventilation par origine.

### Brut et servi

Le taux **brut** (`remplie` seul) surestime, et c'est délibéré côté journal :
*« une case réécrite compte pour chacune des poses qui l'ont visée »*. Pour
mesurer une cadence c'est juste ; pour savoir si une défausse a payé, non — la
pièce jetée puis recouverte a bien été perdue. Le taux **servi** compte donc
l'écrasement comme un échec au lieu de l'écarter : la pièce a été dépensée, elle
appartient au dénominateur. Une pose est écrasée si un geste accepté plus tard
dans la même manche vise la même case ; aucune colonne ne le dit, ça se lit sur
(manche, col, row).

### Les chiffres

`BotMemoire`, 60 parties, graines 1000+, départ niveau 35 avec 10 bombes.
456 manches, **109 064 gestes**, niveaux 35 à 41.

| origine | part des gestes | brut | **servi** | ± |
|---|---:|---:|---:|---:|
| 0 — pose sur le tracé | 21,4 % | 99,9 % | **97,0 %** | 0,1 |
| 1 — défausse selon le plan | 38,4 % | 33,6 % | **21,9 %** | 0,2 |
| 2 — pré-pose de l'anticipation | 12,5 % | 98,9 % | **94,0 %** | 0,2 |
| 3 — défausse sur un pari | 2,6 % | 66,4 % | 51,3 % | 0,9 |
| 4 — écrasement d'une case occupée | 25,1 % | 42,5 % | 21,8 % | 0,2 |
| **ensemble** | 100 % | 59,0 % | **47,7 %** | 0,2 |

Quatre lectures, et elles ne pèsent pas le même poids.

**Le bot fait servir 47,7 % de ses pièces.** C'est la référence. Une session
humaine dépouillée au même instrument donne **44,6 %** (niveaux 1 à 5, 826
gestes, 42,3 % de poses recouvertes — le joueur martèle une case comme poubelle,
c'est son `defausser()`). Et on retrouve les ~42 % qu'on déduisait du *« 58 % des
gestes du bot »* de BOMBES.md. Trois instruments, le même endroit.

**63,5 % des gestes sont des défausses** (38,4 + 25,1). C'est la matière que le
tracé prétend supprimer.

**L'anticipation sert à 94,0 %**, et ce chiffre n'avait jamais été isolé. La
pré-pose de `BotSpaceAnticp` est déjà un planificateur miniature — poser d'avance
sur des cases projetées — et à petite échelle elle ne gaspille presque rien.
C'est le meilleur argument en faveur du chantier : on propose d'en faire la même
chose à l'échelle du tracé entier.

**Un quart des gestes écrase une case occupée pour n'en servir qu'un cinquième.**
Le taux bas penche pour le dépotoir qu'on martèle une fois le plan de défausse
saturé, plutôt que pour le tracé qui reprend un rebut. C'est une inférence, pas
une mesure : les distinguer demanderait de savoir si la case est sur le tracé.

---

## 2 bis. La validation humaine — 18 septembre 2026

Le §2 dit ce qu'on cherche à battre. Celui-ci dit qu'on le bat, mesuré sur un
joueur réel suivant le tracé affiché, avant qu'aucun bot n'existe.

| session | gestes | niveaux | **servi** | distance | cadence |
|---|---:|---:|---:|---:|---:|
| freestyle (14/09) | 826 | 1-5 | 44,6 % ±1,7 | 1,11 | 1,16/s |
| tracé seul | 66 | 1-3 | 36,4 % ±5,9 | 3,85 | 0,41/s |
| **tracé + case recommandée** | 546 | 1-10 | **52,2 % ±2,1** | 4,26 | **0,85/s** |

### Le tracé échange du déplacement contre de l'efficacité

C'est le fait structurel que la première tentative a révélé. En improvisant, on
travaille autour de la tête : **1,11 case** de déplacement par geste. En suivant
un tracé, la case qui veut la pièce est là où elle est, et on traverse le
plateau : **3,85**. D'où l'effondrement de la cadence, et d'où le 36,4 % — non
pas parce que le plan est mauvais, mais parce que le joueur mourait avant que le
flux n'atteigne ce qu'il avait posé.

C'est la première mesure de la colonne `distance` du journal, écrite pour ça :
*« le coût physique du déplacement, que le bot ne paie jamais et que le modèle
compte parmi ses sources d'optimisme »*. L'optimisme fait **facteur trois et
demi**, et il est entièrement au bénéfice du bot.

### La case recommandée sépare les deux coûts

`WGame::caseRecommandee` surligne UNE case — la plus en amont qui accepte la
pièce du haut de file — avec le fantôme de la pièce voulue. Résultat : la
cadence double (0,41 → 0,85) pendant que **la distance ne bouge pas** (3,85 →
4,26). Elle supprime le temps de *recherche*, jamais le *trajet*. Les deux coûts
sont donc bien distincts, et un seul est réductible côté humain.

### La courbe d'apprentissage est le vrai chiffre

```
n1:50%  n2:70%  n3:23%  n4:56%  n5:58%  n6:70%  n7:74%  n8:70%  n9:77%  n10:46%
```

Le niveau 3 à 23 % pèse 154 gestes sur 546 et tire toute la moyenne vers le bas —
c'est le même mur qu'à la session précédente. Une fois passé, le joueur tient
**70 à 77 % du niveau 6 au 9**, soit trente points au-dessus du freestyle, à la
souris.

Réserves, et elles comptent : rien n'est apparié, les niveaux diffèrent (1-10
contre 1-5), le joueur était plus entraîné, et les 77 % reposent sur ~55 gestes
(±6). Ce n'est pas une preuve, c'est un signe très net — et il porte sur la
prémisse du chantier, pas sur un calcul d'histogramme.

Le bot partira avec deux avantages que le joueur n'a pas : **zéro trajet** et
**2,4 fois la cadence** (`CADENCE_BOT = 2.0f`, le point de design du §3).

---

## 3. Pourquoi un tracé — l'argument en pièces

### Le critère : l'histogramme, pas la forme

Tirage uniforme sur 7 types (`PieceFile::genererPiece`, borné à `tpCroix`). Sur
une case dont l'entrée est fixée : 2 types sur 7 vont tout droit (le droit de
l'axe, et la croix qui traverse), 1 sur 7 tourne d'un côté, 1 sur 7 de l'autre,
3 sur 7 ne se raccordent pas.

Un tracé ne gaspille une pièce que si **aucune** de ses cases libres ne réclame
ce type. D'où :

> **taux d'utilisation = (nombre de types distincts encore demandés) / 7**

| tracé | types demandés | utilisation |
|---|---|---:|
| ligne droite | droit de l'axe + croix | **2/7 = 29 %** |
| serpentin sur lignes adjacentes | droit, 4 coudes, croix — pas de vertical | **6/7 = 86 %** |
| serpentin espacé d'une ligne, ou spirale | les 7 | **7/7 = 100 %** |

Le serpentin utilise bien les **quatre** coudes et non deux : chaque demi-tour en
consomme deux (`BasGauche` puis `HautGauche` à droite, `BasDroite` puis
`HautDroite` à gauche). Il ne lui manque que le vertical, qu'un demi-tour d'une
case de haut lui rendrait.

**Conséquence de conception** : ne pas planifier le tracé le plus court ou le
plus simple, mais celui dont l'histogramme de types colle au tirage. Un tracé a
exactement L cases quelle que soit sa forme — tourner ne coûte pas une case de
plus, et divise les défausses par trois. C'est gratuit.

Et un corollaire sur l'ordre : le flux impose de servir l'amont, donc l'ensemble
encore libre est toujours la **queue** du tracé. Il ne faut pas y mettre la
longue portion droite, sans quoi l'utilisation retombe vers 2/7 en fin de manche.

### La croix

Joker : elle remplace n'importe quel droit, puisqu'elle traverse tout droit. Et
comme une croix traversée deux fois compte double (conduites indexées par axe,
cf. `Ecoulement::axe`), **l'auto-croisement est la seule façon de l'encaisser à
plein**. Un planificateur peut décider de se croiser ; aucun bot glouton ne le
fera jamais.

### Le budget de gestes

`DELAI_BASE = DELAI_MIN = 22.0f` : le délai avant départ du flux est **constant**
à tous les niveaux. À `CADENCE_BOT = 2.0f`, ça fait **44 gestes gratuits**. Puis
`DUREE_MIN = 1.00f` (atteint au niveau 9) : 2 gestes de plus par case traversée.

Budget ≈ 44 + 2L gestes pour poser L cases ; coût = L/u tirages.

- Même la ligne droite tient jusque vers le **niveau 6** — les 22 s sont
  constantes alors que L croît, donc les petits niveaux ont un surplus énorme.
  La crainte « sur un niveau simple il générera un tout droit et n'aura pas les
  pièces » ne se vérifie pas : elle ne mord qu'à partir du niveau 7.
- Asymptotiquement il faut **u ≥ 1/2**, soit 3 à 4 types distincts demandés en
  permanence. N'importe quel tracé qui tourne y arrive.
- À u = 1, le tracé complet tient dans les 44 gestes gratuits jusqu'à L = 44,
  **niveau 9** : le bot finirait son tuyau avant que le flux ne parte.

### Le préfixe contigu

La bonne grandeur n'est pas « combien de cases sont posées » mais la longueur du
**préfixe contigu** : le flux s'arrête à la première case trouée, pas à la
première manquante. En posant chaque pièce sur la case la plus en amont qui la
réclame, on remplit en gruyère.

Avec un histogramme uniforme, après *n* tirages le préfixe bute sur le type le
plus en retard — l'écart du minimum de 7 binomiales :

> **préfixe contigu ≈ n − 3,3·√n**

Le déficit croît en √n, donc relativement négligeable. À la fin des 22 s (n = 44)
le préfixe vaut déjà ≈ 22 cases. Ensuite, à l'instant *t* : n = 44 + 2t contre
une position de flux de *t* — à t = 200 s (le pire cas, L = 220), 374 contre 200.
Le préfixe avance **par bonds** : quand le type manquant tombe, il saute
par-dessus toutes les cases déjà servies derrière. La course est gagnée avec
trois fois la marge.

### L'ordre de remplissage : toujours le plus en amont

Tranché le 18 septembre 2026, à l'initiative du user, et ce n'est pas une
préférence.

> **Poser la pièce sur la case la plus en amont du tracé qui réclame son type.**
> On ne va loin que quand rien de proche ne l'accepte.

Deux choses distinctes, qu'il est facile de confondre. Ce qui rend le plan
puissant, c'est que la pièce a **un emploi quelque part** — c'est ça qui fait
passer l'utilisation de 4/7 à 7/7. Où la poser parmi ces emplois est une autre
question, et elle n'admet qu'une réponse.

Le facteur est de **sept**. En servant toujours l'amont, le préfixe avance par
bonds au rythme du tirage : c'est le `n − 3,3·√n` ci-dessus, et la course est
gagnée avec trois fois la marge. En servant l'aval, le préfixe n'avance que quand
tombe le type exact de la case bloquante — une chance sur sept, donc une case
toutes les 3,5 s, contre une seconde par case pour le flux. Ce n'est pas un
réglage, c'est la différence entre gagner et mourir.

Et il n'y a **aucun arbitrage** à faire, ce qui n'était pas évident : tenir un
coude dont la première case demandeuse est au rang 30 alors que le préfixe bute
au rang 12 n'aide pas tout de suite, mais on n'a rien de mieux à en faire et il
servira. « Le plus en amont qui accepte » est donc optimal sans condition.

---

## 4. Le terrain — et le mur

> **Corrigé le 19 septembre 2026.** Cette section annonçait 24 blocs et un mur
> au niveau 49. C'est faux depuis que `BLOQUEES_MAX` vaut **64** : le terrain
> libre plafonne à 160 cases et non 200, et le mur tombe au **niveau 39**. Les
> chiffres ci-dessous sont les bons.

15×15 = 225 cases, moins le réservoir, moins `min(64, 2(niveau−1))` blocs
(`BLOQUEES_MAX`, `BLOQUEES_PAS`) : à partir du niveau **33** il reste **160
cases libres**, définitivement. Et `longueurMinimale = min(220, 10 + 4(niveau−1))`.

Mais les cases libres ne sont pas toutes *atteignables* : 64 blocs éparpillés
fracturent le plateau, et ce qui compte est la **poche du réservoir**, mesurée
par inondation depuis sa sortie (20 graines par niveau, 19 septembre 2026).

| niveau | L | libres | poche du réservoir | hors tracé |
|---:|---:|---:|---:|---:|
| 20 | 86 | 186 | 184 | 98 |
| 30 | 126 | 166 | 156 | 30 |
| 35 | 146 | 160 | 151 | **5** |
| 39 | 162 | 160 | ~150 | **négatif** |
| 45 | 186 | 160 | 154 | **−32** |
| 54+ | 220 | 160 | ~150 | **−70** |

Au niveau 35 il faut déjà couvrir **97 %** de la poche : du quasi-hamiltonien.
Au niveau **39**, l'objectif dépasse ce que la poche contient et devient
**arithmétiquement impossible sans auto-croisement**. Au plafond il faudrait une
soixantaine de croix traversées deux fois — sachant qu'une croix exige ses
quatre voisines sur le tracé, ce qui est une autre affaire.

C'est le mur du planificateur, et il n'est pas là où on l'attendait : ce n'est
pas de trouver un bon tracé, c'est qu'au-delà du niveau 39 il n'en existe aucun
sans auto-croisement — une compétence que le déminage n'achète pas. Les bombes
le déplacent quand même : chaque souffle rend jusqu'à huit cases à la poche
(§11).

---

## 5. Les bombes

Le plateau est **statique** pendant la manche : blocs tirés à `nouvelleManche`,
réservoir fixe, et le tracé ne dépend d'aucune information que la file
apporterait. Une pose n'a lieu que quand la pièce en main est exactement le type
que la case réclame — donc pas de pose approximative, et **aucune raison de
replanifier**.

Sauf une : **l'explosion**. C'est le seul événement qui périme un tracé, à un
instant connu d'avance, et le code le fait déjà pour le plan de défausse
(*« `construirePlan()` est refait une fois, quand la bombe a sauté »*).

Trois conséquences pour le ciblage, toutes à écrire :

- **Le critère change.** `Bot::choisirPaquetDeBlocs` vise aujourd'hui le souffle
  qui emporte le plus de blocs — mesure locale, qui ne distingue pas huit blocs
  au milieu d'une zone déjà ouverte d'un seul bloc qui coupe une rangée en deux.
  Un tracé donne la mesure structurelle que le grief cherchait et n'a pas
  trouvée : **longueur du meilleur tracé après le souffle − avant**. Coûteux
  (≤ 225 candidats × une planification), mais une fois par manche, pendant
  `epAttente`, sur un plateau statique ; et on peut se contenter du proxy
  « combien de cases le gabarit perd à cause de ce bloc ».
- **Une bombe par essai est un réglage, pas une loi** (choix du user dans
  BOMBES.md). Avec un tracé, le nombre juste se calcule : assez pour que L
  redevienne atteignable. `DUREE_BOMBE = 2,5 s` contre 22 s de `epAttente`, soit
  huit fenêtres ; pendant `epAttente` rien n'est plein, donc bomber ne risque
  rien. Le terrain condamné est connu : le planificateur construit ailleurs
  pendant les 2,5 s, en évitant le 3×3 qu'il a choisi.
- **Savoir qu'une manche est perdue d'avance.** Si le meilleur tracé fait 180
  alors que L vaut 202, aucun bot actuel ne peut le savoir : il jouera la manche
  et mourra à 180 en y ayant cru. Le planificateur le sait au premier battement,
  et l'essai devient une **passe de déminage assumée** — on vide le stock, on
  ouvre le terrain, on encaisse la défaite, le rejeu part sur un plateau
  possible. C'est l'arbitrage que BOMBES.md laisse explicitement ouvert et
  *« au joueur »*, rendu calculable. Le déminage propre traverse les rejeux du
  niveau, c'est exactement la ressource qu'il faut.

---

## 6. Le plan de défausse sous un tracé

Les deux plans répondent à la même question avec des quantités d'information
opposées. `construirePlan()` est une **couverture d'ignorance** : le bot ne sait
pas où il va, donc il pave le plateau de circuits fermés d'au moins
`PLAN_LONGUEUR_SURE = 40` cases pour que chaque défausse tombe sur quelque chose
de raccordable. Un billet de loterie, dont toute la valeur vient de ce qu'on
ignore par où on passera.

Un tracé supprime cette ignorance là où elle coûtait : sur une case du tracé, le
type est connu, pas parié.

**Mesuré, et tranché contre l'intuition.** Le seuil avait été fixé *avant* la
mesure — sous 15 % de service, le pavage ne pouvait battre personne et sortait
sans campagne. Il fait **21,9 % ± 0,2**. Plus d'une pièce jetée sur cinq finit
traversée. Il ne sort pas. Le taux s'érode bien avec le niveau (23,0 % au 35,
23,2 au 36, 22,4 au 37, puis 19,4 / 18,4 / 19,1 / 20,2 jusqu'au 41), comme prévu
puisque le tracé mange le complément — mais lentement : 4 points quand le hors
tracé passe de ~54 à ~30 cases.

**Décision** : le pavage survit **sur les cases hors tracé**, en passant le tracé
à `construirePlan` comme des pseudo-blocs — il a déjà la notion (`Cases bloquees
du plateau courant`), donc c'est un masquage.

**L'ablation appariée est ajournée**, et volontairement. Elle arbitrerait entre
deux façons de défausser alors que le tracé est censé faire presque disparaître
la défausse : la question ne redeviendra intéressante qu'une fois qu'on saura ce
que le planificateur défausse réellement. À reprendre à ce moment-là, avec un
instrument bien plus fin que le niveau moyen — le taux de service de l'origine 1
se lit à ±0,2 point sur 60 parties, contre ±0,3 niveau sur 600.

**Règle ferme dans tous les cas** : une défausse ne doit **jamais** tomber sur
une case du tracé. Pas pour les points, mais parce qu'une case du tas qui ne
raccorde pas est précisément là où `Bot::tete()` fait reconstruire — le bot
brûlerait un geste à réécrire ce qu'il vient de poser, et ferait reculer le
préfixe contigu dont dépend la course contre le flux.

---

## 6 bis. Le générateur — mesuré le 18 septembre 2026

`trace.h` / `trace.cpp`, sonde `banc/tracer.cpp`, 40 plateaux de niveau 35
(objectif 146, 200 cases libres, 24 blocs).

**Warnsdorff glouton seul ne tient pas** : 19 % de couverture, **0/12** plateaux
atteignant l'objectif. Une grille à quatre voisins offre trop peu
d'échappatoires — le premier mauvais choix coupe le plateau en deux, et une
marche gloutonne ne peut pas revenir dessus. La règle reste une bonne première
intuition, pas une décision.

Il a fallu trois choses, et les trois comptent :

| | objectif atteint | couverture | temps |
|---|---:|---:|---:|
| Warnsdorff glouton | 0 / 12 | 19,0 % | — |
| \+ retour arrière, élagage par la place restante, culs-de-sac écartés | 10 / 12 | 59,3 % | 120 ms |
| \+ **viser l'objectif et non la couverture maximale** | 20 / 20 | 73,0 % | 27 ms |
| \+ équilibrage de l'histogramme par quota | **39 / 40** | 73,0 % | **13 ms** |

Deux leçons au passage.

**Viser l'objectif et non le maximum** change tout : chercher le plus long chemin
possible est un problème bien plus dur que d'en trouver un de 146 cases, et le
jeu ne demande que le second. La recherche s'arrête dès qu'elle a assez, et les
succès deviennent instantanés.

**L'histogramme ne s'équilibre pas tout seul.** À égalité de score de Warnsdorff,
c'est l'ordre de l'énumération `ESens` qui tranchait, et toujours dans le même
sens : le tracé réclamait **34,7 % de vertical pour 7,2 % d'horizontal**. Un type
réclamé cinq fois plus qu'il n'arrive devient le goulot du préfixe contigu
pendant que les autres partent à la défausse. Départager par le type le plus en
retard sur son quota corrige — et accélère la recherche par la même occasion.

**Le quota n'est pas plat**, et c'est la subtilité : la croix traverse tout
droit, donc elle se pose partout où un droit est demandé. Les sept types tirés se
répartissent sur six réclamés — chaque coude reçoit 1/7, les deux droits se
partagent 3/7. L'idéal est donc **21,4 % par droit et 14,3 % par coude**. Obtenu :
19,6 / 19,8 pour les droits, 15,1 à 15,2 pour les coudes. Le reste de l'écart est
géométrique — couvrir 73 % d'une grille force à tourner plus qu'on ne voudrait.

Conséquence directe : **les sept types tirés ont tous un emploi sur le tracé**,
la croix comprise via les droits. L'utilisation théorique est donc de 7/7, contre
les 47,7 % mesurés du bot actuel.

Reste **1 plateau sur 40** où l'objectif n'est pas atteint dans le budget. Savoir
s'il est hors de portée ou seulement mal cherché n'est pas tranché.

---

## 6 ter. Quand l'objectif n'est pas atteint

Le générateur peut rendre un tracé trop court. On ne sait pas distinguer de
l'extérieur les deux causes — terrain impossible, ou recherche qui a manqué —
donc le bot décide sous incertitude. Trois paliers, du moins cher au plus cher.

**Mesure préalable, 18 septembre 2026** : sur le seul plateau en échec des
quarante (graine 3816267512, niveau 35, 144 cases sur 146), multiplier le budget
par cent fait passer l'objectif.

| budget | longueur | temps |
|---:|---:|---:|
| 400 000 | 144 / 146 | 0,46 s |
| 4 000 000 | 144 / 146 | 5,7 s |
| 40 000 000 | **146 / 146** | 15,6 s |

C'était donc mal cherché, pas impossible. Et noter la forme de la courbe : dix
fois plus de budget ne change rien, cent fois débloque. **Ce n'est pas la
profondeur qui paie, c'est la diversification.**

> **Démenti, 19 septembre 2026.** La diversification a enfin été mesurée, la
> perturbation étant une rotation de l'ordre d'énumération des sens dépendant
> de la graine et de la profondeur. Elle ne paie pas :
>
> | | gain moyen (6 graines × 5 niveaux) | coût |
> |---|---:|---|
> | 8 recherches perturbées à budget/8 | **−2,8 cases** | identique |
> | 4 recherches perturbées à plein budget | **+2,0 cases** | ×4 |
>
> Et diviser le budget écrase la **garde** (94 → 47 rangs garantis), ce qui coûte
> bien plus cher que les cases gagnées — voir §17. Ce qui débloquait réellement
> le plateau en échec ci-dessus, c'est la visée : voir §17, *La visée
> inatteignable*.

### 1. Chercher plus

Le bot a 22 s de délai pour ne poser que ~44 gestes : le temps de calcul est la
ressource la moins chère du jeu. Budget de base 400 000 nœuds (13 ms en moyenne),
élargi tant que l'objectif n'est pas atteint.

Mais **par redémarrages perturbés plutôt que par approfondissement** — même
graine, ordre de départage décalé — ce que la courbe ci-dessus dit assez
clairement. Et borné : 15,6 s sur un plateau, c'est déjà les deux tiers du délai
de départ, et ça figerait la fenêtre.

### 2. Bomber, puis replanifier

Si chercher plus ne suffit pas, c'est le terrain. C'est là que le planificateur
gagne son salaire : **il sait au premier battement que la manche est perdue**, ce
qu'aucun bot actuel ne peut savoir. Il ne la joue donc pas comme une tentative
mais comme une passe de déminage assumée — pendant `epAttente` rien n'est plein,
bomber ne risque rien, ça saute en 2,5 s, on replanifie.

Et le choix de la bombe se resserre : inutile d'évaluer les 225 cases comme le
§5 l'envisageait. Les candidates sont les blocs qui bordent **la fin du meilleur
tracé trouvé** et la région libre qu'il n'a pas su atteindre. Une poignée.

### 3. Jouer quand même le plus long tracé trouvé

`Trace` rend le meilleur chemin rencontré, jamais rien. Construire 144 et mourir
à 144 coûte une vie et déclenche le rejeu — même plateau, même file, plus le
déminage acquis si la bombe a sauté proprement. Ce n'est jamais pire
qu'improviser, et c'est une mort **propre** : une explosion qui tue n'accrédite
aucun déminage (BOMBES.md), donc il ne faut surtout pas mourir de sa propre bombe.

### Et le cas insoluble

Niveau **39** et au-delà, L dépasse ce que la poche du réservoir contient :
aucun budget n'y peut rien. Il faut l'auto-croisement — écrit le 19 septembre
2026, voir §17 — et les bombes, qui rendent du terrain à la poche.

Pour garder les proportions : au niveau 35 l'échec est à 1 plateau sur 40 et se
récupère en cherchant. Le palier 2 sera rare, le palier 3 plus encore.

---

## 7. Ce qui reste à trancher

- ~~**Les redémarrages perturbés.**~~ Tranché le 19 septembre 2026, et dans
  l'autre sens : la perturbation a été mesurée et **ne paie pas** (§6 ter). La
  perturbation qui paie n'est pas dans la recherche, elle est sur le terrain —
  c'est la **bombe**, voir §17.
- ~~**L'auto-croisement**~~, écrit le 19 septembre 2026 : `Trace::ameliorer`,
  §17.
- **La fin de tracé**, où le nombre de types encore demandés s'effondre et où les
  défausses reviennent en masse. Deux atténuations, toutes deux à la
  planification : finir par un motif varié, et prévoir un peu de longueur
  au-delà de `longueurMinimale`, qui est un minimum et non une cible.
- **Le tracé haché.** L'histogramme est plat et la longueur y est, mais rien ne
  mesure la *régularité* : un tracé qui alterne les types à chaque case oblige à
  un enchaînement précis là où de longues portions régulières laisseraient de la
  latitude dans l'ordre de pose. À regarder à l'œil avant d'inventer une mesure.

**Retirés de cette liste** :

- *le gabarit* — on n'a pas pris de serpentin ajusté comme prévu, mais une
  recherche en profondeur ordonnée par Warnsdorff. Voir §6 bis ;
- *l'ordre de remplissage*, qu'on croyait être un arbitrage entre servir
  l'urgence et préserver la diversité. Il n'y a pas de tension : l'urgence gagne
  toujours, et la diversité est une affaire de planification, pas de
  remplissage. Voir §3, *L'ordre de remplissage*.

---

## 8. Repères de code

- `trace.h` / `trace.cpp` — le générateur. `Trace::typePour` est publique et
  statique : le futur bot en aura autant besoin que le calcul lui-même.
- `banc/tracer.cpp` — la sonde du gabarit : longueur, couverture, temps,
  histogramme. `tracer <parties> <graine> <niveau> [budget]`.
- `WGame::setAfficherTrace` — l'overlay vert (`0x4fd67a`), case « Afficher le
  tracé ». Recalculé sur une copie **nue** du plateau, donc cocher en cours de
  manche montre le tracé tel qu'il aurait été planifié au départ. Quand un bot
  en planifie un (`Bot::tracePlanifie`), c'est le sien qu'on dessine — sans quoi
  les deux divergent dès qu'une bombe a sauté, puisque le bot replanifie.
  Sans bot, l'overlay s'invalide lui aussi sur les blocs et pas seulement sur la
  graine, pour la même raison.
- `bottrace.h` / `bottrace.cpp` — le bot. Trois règles : servir la case la plus
  en amont, défausser le reste, ne jamais défausser sur le tracé.
- `Bot::reserveeAuTrace` — le masquage du pavage, et l'interdit de défausse.
- `banc/gestes.cpp` — le journal sans écran. Compilation en tête du fichier.
- `banc/origines.py` — la ventilation. `SEUIL_DECISION` y est figé avec sa date.
- `journal.h` — les cinq codes d'`origine`.
- `Bot::origineTas` — le code que le tas porte sur une case. Lire l'avertissement :
  c'est l'état de la case, pas l'auteur du dernier geste.
- `MainWindow::battement` — le relevé par différence, et la dérivation de
  l'origine. `banc/gestes.cpp` la recopie.


---

## 9. Le bot — mesuré le 18 septembre 2026

`BotTrace`, nom `trace` dans `BotFactory` et bouton « Tracé » dans la fenêtre.
Il tient en trois règles, et rien de plus : la case la plus en amont qui réclame
le type en main ; le reste à la défausse ; jamais de défausse sur le tracé. Le
tracé est replanifié aux deux seuls moments qui le périment — nouvelle manche et
bombe qui vient de sauter — par `construirePlan`, devenue virtuelle pour ça.

Les paliers 1 et 2 du §6 ter ne sont **pas** écrits : ni redémarrage perturbé, ni
bombardement pour replanifier. Quand le générateur rend trop court, le bot joue
le plus long tracé trouvé (palier 3).

### Ce que ça donne

Banc apparié, 60 parties, graines `1 + i * 2654435761`, parties complètes
(`bench <parties> <graine> [bot]`, le bot est un argument depuis aujourd'hui).

| | `memoire` | `trace` |
|---|---:|---:|
| niveau moyen | 38,15 | 37,55 |
| **médiane** | **38,0** | **44,5** |
| écart-type | 1,75 | **13,53** |
| min / max | 32 / 41 | 2 / 47 |

43 parties améliorées, 13 dégradées, 4 inchangées — test des signes p = 3,7·10⁻⁵.
**La moyenne ne veut rien dire ici** : la distribution est franchement bimodale.

- **47 parties sur 60 tiennent**, et elles finissent à **44,3 de moyenne**, six
  niveaux au-dessus du v4, avec un maximum à 47 ;
- **13 s'effondrent** (niveaux 2, 3, 4, 11, 11, 12, 13, 14, 15, 17, 21, 22, 27).

### Le taux de service — la mesure du §2, refaite à l'identique

`gestes 60 1000 35 10 <csv> trace`, 1 951 manches, **366 893 gestes**, niveaux 35
à 47. À comparer ligne à ligne avec le tableau du §2.

| origine | part des gestes | **servi** | (v4) |
|---|---:|---:|---:|
| 0 — pose sur le tracé | 83,9 % | **92,4 %** | 21,4 % / 97,0 % |
| 1 — défausse selon le plan | 11,4 % | **0,0 %** | 38,4 % / 21,9 % |
| 4 — écrasement | 4,8 % | 1,2 % | 25,1 % / 21,8 % |
| **ensemble** | 100 % | **77,5 %** ± 0,1 | **47,7 %** |

**47,7 % → 77,5 %.** Et les défausses passent de 63,5 % des gestes à 16,2 %.

**Le pavage ne sert plus à rien, et c'est une conséquence logique** : sous un
tracé, le flux ne quitte jamais le tracé, donc une pièce posée ailleurs ne peut
pas être traversée — 0,0 % sur 41 667 défausses. Les 21,9 % du §2 venaient de ce
que le tracé improvisé finissait par entrer dans le pavage. La question laissée
ouverte au §6 est donc tranchée par le fait : là où le tracé tient, la défausse
est une poubelle et rien d'autre ; l'endroit où elle tombe n'a plus d'importance,
sauf la règle ferme de ne pas toucher au tracé.

### Comment il meurt — deux causes, aucune n'est un bug

**1. Le mur du §4, et il arrive plus tôt que prévu.** Le générateur, budget de
base, sur 20 plateaux par niveau :

| niveau | 40 | 42 | 44 | 45 | 46 | 47 | 48 | 49 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| objectif atteint | 12/20 | 12/20 | 9/20 | 10/20 | 1/20 | 2/20 | 0/20 | 0/20 |
| temps moyen | 237 ms | 246 ms | 320 ms | 381 ms | 823 ms | 776 ms | 1151 ms | 0 ms |

Le mur arithmétique du niveau 49 est bien là (l'élagage refuse le premier pas,
le tracé rendu fait une case), mais **la recherche lâche dès 46** — et c'est
exactement là que les parties saines s'arrêtent, 44 à 47. Le budget est la
ressource qui manque, pas le terrain : c'est le palier 1 qui n'est pas écrit.

**2. Un type qui ne vient pas, et le rejeu qui le répète.** Deux effondrements
autopsiés sur les treize, et les deux sont du même genre — les onze autres ne
sont pas vérifiés. `--graine 4112119563 --niveau 2` :
objectif 14, tracé 14 trouvé, et **zéro `coudeBG` dans les 44 premières pièces**
de la file — vérifié sur la file elle-même, hors bot. Le préfixe contigu bute au
rang 1, le flux meurt au départ, la manche est perdue.

Ce n'est pas rare au point d'être négligeable : `7·(6/7)⁴⁴ ≈ 0,9 %` de chances
qu'un type manque sur les 44 gestes gratuits, et on joue une quinzaine de manches
basses par partie. L'ordre de grandeur colle aux 13 parties sur 60 — ce qui rend
la cause plausible pour les onze autres, sans la démontrer.

L'autre cas autopsié, `--graine 3041712679 --niveau 13` : deux `coudeHG` seulement
en 49 gestes, trou au rang 4, 41 cases du tracé servies sur 58 pour 4 traversées.

**Ce qui transforme l'accident en fin de partie, c'est le rejeu.** Le plateau et
la file sont rejoués à l'identique et le bot est déterministe : il replanifie le
même tracé, attend la même pièce, meurt de la même façon — trois fois, et la
partie est finie. C'est précisément ce que le v4 sait éviter (`BotMemoire`, les
interdits), et le bot tracé n'a rien de tel.

Sur 193 manches jouées depuis le niveau 1 par quatre parties saines, les 60
manches perdues se répartissent ainsi : 17 avec un tracé plus court que
l'objectif (cause 1), 43 avec un tracé assez long mais un trou tardif — rang 107
sur 166, 147 sur 178, jusqu'à 185 sur 186. C'est la fin de tracé du §7, là où le
nombre de types encore réclamés s'effondre.

### À reprendre

Par ordre de ce que ça vaut, mesures ci-dessus à l'appui.

1. **Un tracé différent au rejeu.** C'est le seul des trois qui touche les 13
   effondrements, donc la moyenne. Rien ne sert de rejouer à l'identique un
   tracé dont on sait qu'il est mort faute d'une pièce.
2. **Le palier 1**, chercher plus par redémarrages perturbés : il vise le plafond
   des parties saines, coincé à 46 par la recherche et non par le terrain.
3. **La fin de tracé**, où 43 manches perdues sur 60 se jouent.


---

## 10. Le début du tracé conditionné par la file — 18 septembre 2026

Demande du user, après la mesure du §9 : *« au moins pour les petits niveaux,
est-ce qu'il est possible de conditionner le début du tracé avec un maximum
d'éléments de la file ? »*

### Ce que la mesure disait, et qui a désigné la solution

Les douze effondrements autopsiés du §9 ont **tous** un trou fatal sur un
**coude** — douze sur douze, aux rangs 1, 1, 2, 4, 7, 7, 8, 10, 14, 14, 15 et
45. Ce n'est pas un hasard de tirage : un coude n'arrive que **1 fois sur 7**
quand un droit est servi **3 fois sur 7**, son propre tirage plus la croix qui
le remplace. Une case de coude est trois fois plus exposée qu'une case de droit,
et le quota du générateur, qui équilibre sur le tracé entier, ignore que le
début coûte plus cher que la fin : le flux atteint le rang k à t = 22 + k
secondes, le bot ayant tiré 44 + 2k pièces d'ici là.

D'où deux leviers, cumulés :

1. **La file** fixe les premiers rangs avec ce qu'on tient déjà — exact, mais
   borné à cinq pièces ;
2. **les droits en tête** couvrent tout le préfixe exposé, sans rien savoir de
   la file. `DROITS_EN_TETE = 16` dans trace.cpp, départage avant le quota, et
   seulement là : au-delà on rend la main au quota, sans quoi la fin de tracé
   n'aurait plus que des coudes à demander.

### La contrainte est dégressive, et le vivier n'est pas la garde

Deux pièges, tous deux trouvés en mesurant plutôt qu'en raisonnant.

**Exiger les cinq rangs d'un coup échoue presque toujours**, et pour une raison
purement géométrique : l'entrée d'une case ne laisse que **trois** types
possibles (le droit de son axe et deux coudes), et chaque rang servi restreint
le suivant de la même façon. Une main de cinq pièces tirées au hasard tombe
rarement sur une chaîne réalisable — la recherche meurt en un à quatre nœuds,
faute de candidate payable. On descend donc : 5 rangs, sinon 4, sinon 3… sinon
aucun. C'est le mot du user : *« si il n'y en a qu'une ou deux ou pas du tout,
tant pis, on se lance sans »*.

**Et le vivier reste la file entière quand la garde descend.** Réduire la garde
en réduisant la main aux premières pièces — le réflexe — impose l'ordre de la
file au tracé, alors que le bot sert la case la plus en amont quelle que soit la
place de la pièce dans la file. Avec ce bug la garde retombait à 1 partout et la
contrainte ne servait à rien. Corrigée, elle **tient 3 à 4 rangs sur 5**.

### Effet de bord : c'est le palier 1, gratuitement

Chaque valeur de garde est une recherche repartie sur un ordre de départage
différent. C'est exactement le **redémarrage perturbé** que le §6 ter réclamait,
et la diversification paie comme il l'annonçait — 20 plateaux par niveau :

| niveau | objectif atteint sans contrainte | avec |
|---|---:|---:|
| 35 | 16/20 | **18/20** |
| 44 | 10/20 | **12/20** |
| 46 | 4/20 | 4/20 |

### Le coût, et le bridage

Six recherches pleines coûtent cher là où la recherche est déjà lente. Mesuré au
niveau 46 : **2,9 s en moyenne, 6,9 s au pire, dans un seul battement** — un
tiers du délai de départ, et l'écran figé d'autant, ce qui se verrait à
l'enregistrement.

Les passes contraintes ont donc **le quart du budget**, la passe libre garde le
sien. Le pire cas retombe à 2,8 s, la moyenne à 1,3 s, et le taux d'objectif
ci-dessus ne bouge pas : une passe contrainte qui a besoin de plus que ce
quart-là n'a de toute façon pas trouvé un début facile.

| niveau | 13 | 35 | 44 | 46 |
|---|---:|---:|---:|---:|
| sans contrainte | 0 ms | 72 ms | 384 ms | 623 ms |
| avec, bridée | 0 ms | 69 ms | 620 ms | 1258 ms |

---

## 11. Le blocage du plateau saturé — 18 septembre 2026

Signalé par le user en regardant jouer : `--graine 791501436 --niveau 44
--vies 10 --bombes 8`, *« truc étrange, il arrête de jouer »*. Ce n'étaient pas
les cases mortes : elles ne sont calculées que quand l'overlay les affiche et ne
pilotent aucune décision.

Reproduit : il reste **12 cases vides et les douze sont sur le tracé**. La pièce
en main n'y a pas d'emploi, et la règle ferme du §6 — jamais de défausse sur le
tracé — ne laisse plus aucune case où jeter. `defausser()` ne pose rien, la file
ne descend plus, et le bot ne rejoue **plus jamais** : ni pose ni défausse, la
manche s'éteint sur un plateau à douze cases vides.

Ce n'est pas rare, c'est structurel : le hors tracé vaut 22 cases au niveau 43,
18 au 44 (§4). Le tas les occupe toutes en une manche.

**Correctif** : plateau saturé, on écrase un de ses propres rebuts — **du même
type en priorité** (choix du user : le plateau ne change alors pas d'un iota, on
ne défait aucun raccord, et le geste ne coûte que ses 25 points), à défaut un
rebut quelconque, le plus loin de la tête comme partout ailleurs. La règle ferme
tient : on ne touche jamais au tracé.

### Et le mur du §4 n'est pas arithmétique

Sur la graine du user, une fois le blocage levé, le bot monte au **50** en
partant du 44. Le §4 annonçait l'impossibilité passé le 49, L dépassant les 200
cases libres — mais ces 200 supposent les 24 blocs intacts. **Les bombes en
retirent**, donc le terrain libre monte au-dessus de 200 et l'objectif
redevient atteignable. Le mur existe, il est mobile.


---

## 12. Le banc du 18 septembre au soir

60 parties appariées, graines `1 + i * 2654435761`, parties complètes. « v1 » est
le bot du §9 ; « v2 » ajoute la contrainte de file, les droits en tête et le
déblocage du plateau saturé.

| | `memoire` | tracé v1 | **tracé v2** |
|---|---:|---:|---:|
| niveau moyen | 38,15 | 37,55 | **43,00** |
| médiane | 38,0 | 44,5 | **50,0** |
| écart-type | 1,75 | 13,53 | 12,64 |
| min / max | 32 / 41 | 2 / 47 | **11 / 52** |

- **v2 contre `memoire` : +4,85 niveau** (t = +2,93, IC 95 % [+1,61, +8,09]),
  44 parties améliorées, 13 dégradées, 3 inchangées. C'est la première version
  qui gagne aussi en MOYENNE, pas seulement en médiane.
- **v2 contre v1 : +5,45** (t = +2,82), 44 améliorées, 8 dégradées. Le pire cas
  remonte de 2 à 11.

### Ce que le banc dit vraiment, et que la moyenne cache

La distribution reste franchement **bimodale**, et l'écart-type le crie : 1,75
pour le v4 contre 12,64 pour le tracé. Deux profils de risque opposés, pas deux
réglages du même.

- **42 parties sur 52 tiennent** et finissent à **48,6 de moyenne** ;
- les autres s'effondrent entre 11 et 29.

### Le pari, et où il se trouve exactement

Mot du user, et c'est la bonne lecture : *« sa stratégie est presque uniquement
basée sur les paris, c'est couillu comme méthode, mais ça ne passe pas à tous
les coups »*.

Le §1 de ce document dit que le tracé **supprime** la spéculation — sur une case
du tracé, le type est connu, pas parié. C'est vrai case par case, et faux à
l'échelle de la manche. Le pari s'est **déplacé** : le bot choisit un chemin
unique au premier battement et mise que le tirage lui livrera chaque type
**avant que le flux n'atteigne sa case**. Une mise unique, engagée d'un coup,
sur une séquence de deux cents tirages. Les bots d'avant ne pariaient jamais
là-dessus : ils adaptaient le chemin à la pièce qui venait.

Et la mise est **concentrée en tête**. Sur les dix-huit trous fatals autopsiés,
tous sont entre les rangs 1 et 22, et **dix-sept sur dix-huit sont des coudes**.
Le flux atteint le rang k à t = 22 + k secondes : le rang 1 a eu 44 tirages pour
se combler, le rang 100 en a eu 244. Le pari tient presque entièrement dans les
vingt premières cases.

La contrainte de file et les droits en tête réduisent la mise sans changer sa
nature : 3 à 4 rangs garantis, 16 biaisés. Les trous restants sont maintenant
aux rangs **12, 15, 19, 22** — juste derrière la zone couverte. On repousse le
pari, on ne le supprime pas.

### À reprendre, par ordre de valeur

1. **Rendre le plan révisable.** Quand le préfixe bute et que le flux approche,
   re-router à partir du trou avec ce qu'on tient, au lieu d'attendre un type
   qui ne vient pas. Le tracé cesse d'être une mise unique pour devenir une
   suite de petites mises. C'est l'idée de la file du §10, appliquée en cours de
   manche à la tête de ce qui reste à servir.
2. **Le palier 2, enfin calculable.** Mesuré sur `--graine 791501436
   --niveau 45` (objectif 186) : sans bombe le tracé fait 128, avec **une** 184,
   avec **deux** 186 — l'objectif. Au-delà ça plafonne : ce n'est plus le
   terrain qui bloque mais le budget de recherche. Or `gererBombes` en pose
   **une par essai et rien avant l'essai 2** (bot.cpp:2047-2051, règle de
   BOMBES.md écrite avant qu'un tracé existe) : le bot joue donc une manche
   qu'il sait perdue, paie une vie, et n'en pose qu'une au rejeu. Le ciblage
   actuel suffit — le tracé sert de test d'arrêt, pas de fonction de score — et
   les bombes doivent être espacées de deux cases, une bombe prise dans un
   souffle sautant sans exploser.


---

## 13. Les deux décisions du user — et le pari qui disparaît

18 septembre, fin de journée. Devant la bimodalité du §12, le user tranche deux
fois, et contre mes deux propositions :

> **Le coup des bombes on garde, c'est stupide de se lancer dans un niveau perdu
> d'avance. Pour le recalcul en cours de route, non, puisque les niveaux perdus
> le sont toujours dans les premiers coups : on laisse sa chance au bot une
> fois, puis comme il connaît la file exacte à son second essai, là il recalcule
> le tracé en fonction !**

Le second point est une idée que je n'avais pas eue, et c'est la meilleure des
deux. Le rejeu est **parfait** — même plateau, même file, pièce pour pièce. Une
manche perdue rend donc au bot la séquence exacte des tirages à venir : là où le
premier essai se contente des cinq pièces visibles, le rejeu fait tenir au tracé
les **quarante-quatre pièces qui tomberont avant le départ du flux**.

### Ce qui est écrit

- **`BotTrace::bomberMaintenant`** — tant que le tracé planifié est plus court
  que l'objectif et qu'il reste du stock, on bombe. Une à la fois : deux bombes
  voisines s'annulent, et on ne sait pas combien il en faut avant d'avoir
  replanifié. Chaque explosion refait le plan, donc le tracé, donc la boucle
  s'arrête d'elle-même. `Bot::bomberMaintenant` garde la règle de BOMBES.md pour
  les autres bots — une par essai, rien avant le rejeu.
- **Pendant que la mèche brûle, le bot ne joue plus.** Ce qu'il poserait serait
  périmé par la replanification, et surtout `demanderFoncer()` jetterait la
  manche avant l'explosion — c'est ce qui tuait le réservoir enfermé dans une
  poche de deux cases.
- **`BotTrace::noterFile` / `mainConnue`** — relevé de la séquence au premier
  essai, sur la **fenêtre des cinq visibles** et non sur le haut de pile : deux
  pièces identiques d'affilée y seraient indiscernables. Bornée aux pièces qui
  tombent avant le départ du flux, parce qu'au-delà l'ordre compte et que la
  contrainte, qui ne raisonne qu'en multiensemble, ne saurait plus le dire.
- **La garde se cherche par dichotomie.** La faisabilité est monotone — servir
  k+1 rangs sert les k premiers — donc six recherches au lieu de quarante-quatre.

### Le banc, 60 parties appariées

| | `memoire` | tracé v1 | tracé v2 | **tracé v3** |
|---|---:|---:|---:|---:|
| niveau moyen | 38,15 | 37,55 | 43,00 | **48,98** |
| médiane | 38,0 | 44,5 | 50,0 | **50,0** |
| **écart-type** | 1,75 | 13,53 | 12,64 | **4,58** |
| min / max | 32 / 41 | 2 / 47 | 11 / 52 | **20 / 51** |

**+10,83 niveau contre le v4** (t = +17,04, IC 95 % [+9,59, +12,08]), **57
parties améliorées sur 60**, 2 dégradées, 1 inchangée. Contre la v2 du matin :
+5,98 (t = +3,37).

### Le pari a disparu, et ça se lit dans l'écart-type

C'était la vraie question du §12 : une stratégie de pari, ou il passe ou il
casse. L'écart-type tombe de **12,64 à 4,58** et le minimum remonte de 11 à 20.
La distribution n'est plus bimodale du tout :

    20  36  40  43  46 46  48  49 x5  50 x33  51 x15

**56 parties sur 60 finissent à 45 ou au-dessus**, 48 sur 60 à 50 ou 51.

Les deux décisions du user attaquent les deux moitiés du pari, et c'est pour ça
qu'elles se complètent : la bombe supprime les manches **imprenables** (le terrain
ne permettait pas l'objectif), la file connue supprime les manches **manquées**
(le terrain permettait, le tirage a refusé). Il ne reste rien entre les deux.

### Le nouveau mur, et il est d'une autre nature

Le bot meurt maintenant au **50-51**, et toujours au même endroit. Ce n'est plus
le tirage qui le trahit, c'est l'arithmétique du §4 : `longueurMinimale` vaut
4n + 6, soit 206 au niveau 50 et 210 au 51, pour 200 cases libres qu'une poignée
de bombes ne relève que de quelques unités. Il faudrait l'**auto-croisement**
(§7), que rien ne sait faire, ou un stock de bombes que le jeu ne donne pas.

Le bot n'est plus limité par sa stratégie. Il est limité par le jeu.


---

## 14. L'échéance des rangs — 18 septembre 2026, tard

Le §13 laissait une partie sur soixante mourir au niveau **20**, et le user a
demandé la graine pour regarder : `--graine 2175734978 --niveau 20 --vies 5
--bombes 10`.

### Ce que la graine montrait

Le bot arrive au niveau 20 avec **5 vies et le stock de bombes plein**, et il y
laisse cinq vies et zéro bombe. Ce n'est donc pas une partie déjà abîmée — la
question que le user posait — c'est une manche qui le tue cinq fois de suite.

Le tracé fait **86 cases pour un objectif de 86** : le terrain permet. Relevé
geste par geste des douze dernières secondes :

```
t=80,4  vertical    -> défausse | 1er trou rang 69 (coudeHD, posable) | flux 57
t=81,9  coudeHG     -> défausse | 1er trou rang 69 (coudeHD, posable) | flux 59
...  24 tirages d'affilée, pas un seul coudeHD ...
t=92,1  vertical    -> défausse | 1er trou rang 69 (coudeHD, posable) | flux 69
   >>> BARRE D'ESPACE, traversées 69
```

Un trou unique, posable tout du long, et la pièce qui ne vient pas. Vingt-quatre
tirages sans un type donné, c'est 2,4 % — assez rare pour être une malchance,
assez fréquent pour tuer une partie sur soixante.

### La borne qui manquait

La garde du §10 s'arrêtait aux pièces tombant **avant le départ du flux** : 44,
donc 42 rangs garantis. Mais le flux n'atteint le rang *k* qu'à t = 22 + k
secondes, et le bot aura tiré `44 + 2k` pièces d'ici là. **Le rang 69 avait 182
tirages devant lui**, pas 44 — la garde s'arrêtait bien avant la vraie limite,
faute de compter le temps.

Chaque rang porte donc maintenant son **échéance** : une pièce d'indice *i* peut
servir le rang *k* si `i < gestesAvantDepart + k * gestesParCase`. Le multiensemble
devient un problème d'ordonnancement, et comme les échéances croissent avec le
rang, servir chaque rang avec la **pièce la plus ancienne qui convient** est
optimal — argument d'échange classique. Le glouton n'est pas une heuristique ici,
il est exact.

Conséquence immédiate sur la graine du user :

    essai 1 : 86 / 86 , garde  0 sur 5     (file visible)
    essai 2 : 86 / 86 , garde 86 sur 185   (file connue)

**Le tracé entier est garanti servable en temps voulu**, et la manche passe. La
garde n'est plus « le début du tracé » mais le tracé complet, dès que le rejeu
donne la séquence.

`gestesParCase` vaut la cadence, soit une case par seconde : c'est `DUREE_MIN`,
donc le flux au plus rapide. Minorer la durée de remplissage est du bon côté —
on suppose moins de pièces disponibles qu'il n'y en aura vraiment.

### Deux corrections que seule la mesure a trouvées

La première version de l'échéance a donné **−0,93 niveau** au banc, pas un gain.
Cinq graines gagnaient gros, six perdaient plus gros. Deux défauts, et aucun ne
se voyait à la lecture :

**Un plancher hérité.** `gestesAvantDepart` était minoré par `enMain.size()`,
garde-fou écrit quand la main valait cinq pièces. Avec la file entière il portait
la base à 136 : **toutes les pièces devenaient disponibles dès le rang 0**, donc
l'échéance qu'on venait d'introduire était purement et simplement annulée. La
garde annonçait « 54 sur 54 » et ne garantissait rien.

**Un optimisme d'un geste.** Même corrigé, le planificateur affectait au rang 45
la pièce **133 pour une échéance de 134**. Une pièce de marge, et la manche
tombait quand même : la pièce d'indice *i* n'est pas disponible à `i / cadence`
mais à `(i+1) / cadence` — il faut un geste pour la consommer, et elle n'est
posable qu'une fois en tête de file — et le flux **entre** dans la case avant de
l'avoir remplie. D'où `MARGE_ECHEANCE = 4` gestes, deux secondes, qui ne coûtent
que quelques rangs de garde.

### Le banc, 60 parties appariées

| | `memoire` | v3 | v4 (échéance nue) | **v5 (échéance + marge)** |
|---|---:|---:|---:|---:|
| niveau moyen | 38,15 | 48,98 | 48,05 | **49,75** |
| médiane | 38,0 | 50,0 | 50,0 | **50,0** |
| écart-type | 1,75 | 4,58 | 6,83 | **2,24** |
| minimum | 32 | 20 | 12 | **35** |
| parties sous 45 | — | 20, 36, 40, 43 | 12, 23, 34, 35, 37, 41, 43 | **35, 43** |

**+11,60 niveau contre le v4** (t = +29,2), **59 parties améliorées sur 60**.

Contre la v3, en revanche : **+0,77 seulement, t = +1,24, non significatif**. Il
faut le dire tel quel — sur la moyenne, l'échéance ne se distingue pas du bruit
à soixante parties. **Ce qu'elle achète est ailleurs** : l'écart-type tombe de
4,58 à 2,24 et le pire cas remonte de 20 à 35. Il ne reste plus une seule partie
cassée ; la plus basse finit au niveau 35, c'est-à-dire au-dessus de la médiane
du v4.

C'est exactement ce qu'on lui demandait. Le §12 constatait une stratégie de
pari ; il n'y a plus de pari du tout.


---

## 15. La relève, et le bonus — 18 septembre 2026

Demande du user, une fois la queue de distribution réglée : *« ce serait bien
qu'une fois l'objectif atteint, le bot spaceAnticp prenne le relais de bottrace,
avec en plus le remplacement de tronçons droits par les croix pour maximiser le
score »*.

C'est le bon découpage, et il tient à une propriété du tracé qu'on avait sous
les yeux : **le tracé est calculé pour atteindre `longueurMinimale`, donc il
n'existe pas au-delà**. Ce qui reste à gagner après n'est plus une manche mais
des points — 50 par case traversée (`POINTS_PAR_CASE`) — et construire de proche
en proche devant le flux sans objectif à viser est exactement ce que le v3 sait
faire.

### Ce qui est écrit

- **`BotTrace` dérive désormais de `BotSpaceAnticp`** et lui délègue `jouer()`
  dès que `objectifRestant()` tombe à zéro. Conséquence directe : le bot
  n'appuie plus sur la barre d'espace dès que son tracé est servi, il continue à
  construire ; la barre revient à `abandonner()`, c'est-à-dire au moment où plus
  aucune pièce ne prolonge le tuyau.
- **La croix sur un droit du tracé** est autorisée à partir de ce moment-là
  seulement : le garde-fou du §6 devient `reserveeAuTrace(x, y) &&
  objectifRestant() > 0`. Avant l'objectif, une croix posée sur le tracé coûte
  25 points et ne rapporte rien — un tracé planifié ne revient jamais s'y
  croiser. Après, c'est le v3 qui construit, et lui se croise volontiers : une
  croix traversée deux fois vaut deux fois 50 points (`Ecoulement` indexe les
  conduites par axe).

### Et la dernière case cesse d'être indifférente

Remarque du user dans la foulée : *« ça veut dire que le bot trace doit éviter au
maximum de finir sur une case sans sortie »*. C'était écrit noir sur blanc dans
`finaliser` — *« le flux fuira derrière, ce qui ne coûte rien une fois l'objectif
atteint »* — et c'est devenu faux le jour même, puisque le v3 repart de là.

- **Warnsdorff s'inverse sur la dernière case.** Partout ailleurs on prend
  l'étranglement d'abord, pour ne pas se couper du terrain ; la dernière case
  n'a plus de suite à se ménager, elle a un successeur à accueillir. On finit
  donc au large.
- **Sa sortie vise la place** : parmi les côtés qui donnent sur une case libre,
  celui dont la voisine est elle-même la plus ouverte. Le droit de son axe n'est
  plus qu'un repli. Cette case n'est payée par aucune garde — la recherche type
  une case en la QUITTANT, et celle-ci n'est jamais quittée — donc on peut la
  choisir librement.

### Le banc, 24 parties appariées (niveau ET score)

`banc/bench.cpp` rend désormais deux colonnes, et `banc/analyse.py` prend en
troisième argument celle qu'on veut lire — les relevés à une colonne d'avant
restent comparables.

| | `memoire` | tracé sans relève | **tracé avec relève** |
|---|---:|---:|---:|
| niveau moyen | 37,83 | 49,42 | **49,42** |
| score moyen | 249 998 | 321 134 | **354 594** |
| médiane du score | 256 788 | 333 125 | **375 688** |
| meilleur score | 287 000 | 431 475 | **472 725** |

**La relève seule : +33 459 points** (t = +14,4, IC 95 % [+28 900, +38 019]),
**24 parties améliorées sur 24**, aucune dégradée — et **±0,00 niveau, les 24
parties finissent exactement au même niveau**. C'est logique et ça vaut d'être
dit : quand la relève se déclenche l'objectif est déjà sécurisé, donc tout ce qui
suit ne peut qu'ajouter des points. Le gain est gratuit.

Contre le v4, l'ensemble donne **+11,58 niveau et +104 596 points (+42 %)**.

**Ce que ça coûte, en revanche** : le bot n'appuie plus sur la barre d'espace dès
que son tracé est servi, donc chaque manche se joue en entier au lieu d'être
avalée en ×8. C'est le temps de jeu supplémentaire pendant lequel il encaisse ses
points — mais ça multiplie d'autant le coût du banc, qui passe de quarante
minutes à plusieurs heures pour soixante parties. Les mesures ci-dessus sont
donc sur vingt-quatre.


---

## 16. Le terrain se durcit — 18 septembre 2026

Décision du user, pour raccourcir la prise vidéo : `BLOQUEES_MAX` passe de **24
à 64**. Le bot finissait au niveau 50, la vidéo avec lui.

Balayage, 12 parties complètes par valeur :

| `BLOQUEES_MAX` | niveau moyen | fourchette | score moyen |
|---:|---:|---|---:|
| 24 (avant) | 50,1 | 49-51 | 373 722 |
| 44 | 41,8 | 39-43 | 217 127 |
| 48 | 40,6 | 38-43 | 206 914 |
| **64** | **34,1** | **33-36** | 162 350 |
| 72 | 34,1 | 33-36 | 160 641 |

**Pourquoi 64 et pas plus.** `nbCasesBloquees()` vaut `min(BLOQUEES_MAX,
(niveau − 1) × BLOQUEES_PAS)`, donc le plafond ne mord qu'à partir du niveau
`MAX/2 + 1` — le 33 pour 64, c'est-à-dire là où le bot meurt désormais. Au-delà,
le plafond est atteint après sa mort et ne sert plus à rien : 72 donne
exactement les mêmes niveaux. **C'est la rampe qui fixe la fin, pas le
plafond.**

Trois conséquences mesurées :

- la vidéo raccourcit de plus d'un tiers en niveaux, et davantage en durée : les
  manches des niveaux 40-50 sont les plus longues, ce sont celles qui
  disparaissent ;
- le score tombe de 374 k à 162 k. Court et gros chiffre tirent en sens
  inverse ;
- la fourchette reste serrée (33-36), donc une prise reste prévisible.

**Les chiffres du §4 datent de 24 blocs** et restent lisibles comme tels : les
200 cases libres qu'il cite en deviennent 160, et le mur arithmétique
(objectif 4n+6 contre le terrain libre) arrive d'autant plus tôt.

### Et la manche imprenable ne se joue plus les bras ballants

Même séance, autre demande du user : *« je suis à peu près sûr qu'il finit le
dernier niveau sans jouer la moindre pièce, c'est dommage »*. Mesuré sur
`--graine 1`, les six dernières manches :

    niveau 51 :   2 gestes, 0 bombes, trace 1 / 210, 0 traversees, 0 pieces sur le plateau

Deux gestes, plateau vide, six fois de suite — une par vie. Le tracé rendait
**une** case pour un objectif de 210 (le mur du §4), et le stock de bombes était
vide, dépensé aux niveaux précédents.

`BotTrace` dérive donc maintenant de `BotMemoire`, et la relève a **deux
motifs** : objectif acquis → le v3 pour le bonus (§15) ; **manche imprenable —
tracé trop court et plus une bombe pour le rallonger — → le v4 pour toute la
manche**. Résultat sur la même graine : **50 gestes au lieu de 2, et 43 pièces
sur le plateau au lieu de zéro**. La manche était perdue de toute façon ; elle
se remplit au moins à l'écran. C'est la règle « pour le spectacle » demandée
dans BOMBES.md le 14 septembre.

---

## 17. La séance du 19 septembre 2026 — le planificateur rendait une case

Journée passée sur les bombes et le planificateur, à l'initiative du user qui
regardait jouer. Quatre défauts, tous invisibles au banc, tous chers.

### La visée inatteignable

`Trace::calculer` élague chaque candidate par la place restante : depuis cette
case, le terrain atteignable doit valoir au moins ce qu'il reste à parcourir.
La règle est juste, mais **binaire** — elle se referme d'un coup quand la visée
approche la taille de la poche, et il ne reste plus une seule branche.

Mesure sur `--graine 2998034427 --niveau 36`, poche 155 cases, même budget :

| on demande | 20 | 60 | 100 | 110 | 120 | 130 | 140 | 145 | **150** |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| elle rend | 20 | 60 | 100 | 107 | **117** | 111 | 95 | 49 | **1** |

Plus on demande, moins on obtient — et à l'objectif exact, **une case en zéro
milliseconde**. Soixante-quatre fois le budget n'y change rien. C'était le cas
de **tous les niveaux au-delà de 39**, et ça explique le palier 3 du §6 ter
(« jouer le plus long tracé trouvé ») qui ne s'est jamais déclenché : il n'y
avait pas de plus long tracé, il y avait une case.

**Correctif** (`BotTrace::chercherAuMieux`) : on demande l'objectif, et s'il
échoue on redescend la visée par dichotomie en gardant le meilleur tracé
rencontré — pas celui de la plus haute visée qui réussit, le meilleur venant
souvent d'une visée qui a échoué. Résultat : 1 → 97-124 cases aux niveaux 40 à 55.

### La garde perdue

Corollaire, et il coûte plus cher encore. Une recherche lancée sur une visée
inatteignable rend un long tracé **de garde 0** : la dichotomie de la garde,
qui vise la même longueur, échoue à chacune de ses passes contraintes. Seule la
passe libre aboutit, et elle ne garantit rien.

`--graine 1188181038 --niveau 38`, après une bombe, **même plateau, même
longueur** :

| visée demandée | longueur | garde | traversées en jeu |
|---:|---:|---:|---:|
| 158 (l'objectif, hors d'atteinte) | 130 | **0** | **17** |
| 130 (atteignable) | 130 | **20** | **135** |

**Facteur huit**, à tracé identique. C'est le début du tracé qui fait le
préfixe contigu, donc ce que le flux parcourt. **Correctif** : une passe finale
à plein budget sur `qMax(bas, longueur trouvée)` — on redemande ce qu'on vient
de trouver — et à longueur égale on garde le tracé de plus grande garde.

> **Ne jamais juger un tracé sur sa seule longueur. Toujours sur le couple
> (longueur, garde).**

Et la garde plafonne pour une raison mesurée : les **échéances**. Sans elles,
avec 218 pièces connues, elle couvre le tracé entier ; avec elles, elle s'arrête
là où la chaîne casse — sur le plateau ci-dessus, au rang 17, où le tracé
réclame un douzième coude bas-droite dont la prochaine occurrence arrive au 82ᵉ
tirage pour une échéance de 74. Ce n'est pas la recherche qui échoue, c'est le
terrain qui ne le permet pas.

### Le tracé trop court, jeté

Le relais vers le v4 (§12) avait été écrit pour le cas dégénéré — un tracé
d'UNE case — et s'appliquait dès qu'il manquait une seule case. Sur
`--graine 2998034427 --niveau 36`, le planificateur rendait 139 cases pour un
objectif de 150 : le bot les jetait pour improviser, et la manche rendait
**vingt** traversées au lieu de 139. **Correctif** : un tracé trop court reste
un tracé, on le sert jusqu'au bout ; le v4 ne prend la main qu'après.

### La bombe qui visait les blocs

`Bot::choisirPaquetDeBlocs` maximise les blocs emportés. Pour un bot qui
planifie, ce qu'une bombe doit acheter c'est de la **longueur de tracé**, et les
deux ne coïncident pas. Pour chaque emplacement possible, le tracé qui en
résulte (`--graine 2998034427`) :

| niveau | sans bombe | critère blocs | meilleur emplacement | médiane |
|---:|---:|---:|---:|---:|
| 31 | 12 | 118 | **130** | 18 |
| 33 | 1 | 4 | **135** | 1 |
| 34 | 85 | **75** | **108** | 88 |

Au niveau 34 le critère faisait **pire que ne rien faire**, et sous la médiane
des emplacements. **Correctif** (`BotTrace::choisirPaquetDeBlocs`) : on essaie.
Chaque emplacement est soufflé sur une copie, le tracé recalculé, on garde le
meilleur — le résultat de la bombe est donc connu avant qu'elle soit posée.
Balayage complet à 25 000 nœuds par candidat, sous échéance de 3 s, dans le
thread. Au niveau 31, cinq bombes deviennent une.

Trois règles autour, toutes venues d'observations du user :

- **une bombe par rejeu**, jamais au premier essai — le tracé garde sa chance ;
- **au rejeu, on dépense sans condition** : le terrain ouvert reste ouvert d'un
  essai à l'autre, et deux bombes stériles séparément peuvent payer ensemble
  (niveau 33 : 1 → 109 → 138). Sans ça le bot mourait à 136/138 — deux cases —
  avec neuf bombes en stock ;
- **le tracé ne recule jamais** après un souffle : si la recherche rend plus
  court, on garde le précédent, ses cases sont toujours libres.

### L'auto-croisement, enfin

`Trace::ameliorer` : une fenêtre de 14 rangs glisse le long du tracé, chacune
est re-résolue par une recherche exhaustive qui a le droit de repasser sur une
case, entrée et sortie figées, et on remplace si ça rend strictement plus de
passes.

`Trace` sait désormais qu'une case porte deux rangs : elle retient le **premier**
(c'est lui qui porte l'échéance) et annonce **tpCroix** comme type, seul type
qui traverse les deux axes.

Une loi à connaître : le damier. Un tracé alterne les couleurs, donc une marche
de L visites a ses deux bouts de même couleur ssi L est impair, et une croix
ajoute une visite **de sa propre couleur**. À encombrement et extrémités figés,
on ne peut donc ajouter des croix que **par paires** — une seule retournerait la
parité et aucune marche n'existerait. Une croix isolée exige une case de terrain
en plus.

La réécriture **ne touche pas au préfixe garanti** : la garde promet les types
exacts de ces cases-là, les réécrire garderait le chiffre en jetant la garantie.
D'où un gain modeste (+2,5 passes, 1 à 2 croix, moins d'une milliseconde, zéro
tracé illégal sur 40) là où le prototype annonçait +12 : plus le planificateur
garantit, moins il reste à réécrire.

Et la croix est **servie en priorité là où elle est irremplaçable** (règle du
user) : une case de croisement n'accepte qu'elle, une case de droit en accepte
deux sur sept. Sauf quand le flux talonne, où l'amont reprend tous ses droits.

### Ce que ça donne

Banc apparié, 12 graines, départ niveau 30, 3 vies, 3 bombes :

| | avant | après |
|---|---:|---:|
| score | 16 833 | **32 802** |
| écart apparié | | **+15 969 (t = 6,14)**, 11 parties sur 12 améliorées |
| niveau atteint | 32,25 | 32,33 (t = 0,27, nul) |

Le bot ne va pas plus loin : **il ne gâche plus**. Le mur reste vers 32-33, et
il est ailleurs — la recherche trouve ~105 cases sur une poche qui en offre 155.
C'est le dernier gros gisement du planificateur, et il est ouvert.

### Un outil qui manquait

`banc/fenetre.cpp` pilote la **vraie** `MainWindow` hors écran, un battement
toutes les 16 ms de montre. Tous les autres harnais déroulent une manche de
soixante secondes en quelques millisecondes : ils sont **structurellement
aveugles** à tout ce qui dépend du temps réel. Celui-ci a attrapé deux pannes
que le banc ne pouvait pas voir — dont un bot qui brûlait huit vies sans jamais
poser une bombe, là où le banc en posait quatre.
