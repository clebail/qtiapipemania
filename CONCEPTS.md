# Deux concepts à développer

Note de conception du 19 septembre 2026, prise pendant la séance sur les bombes.
Rien de tout ça n'est écrit dans le code : c'est le cahier des charges, avec les
chiffres qui le contraignent déjà.

L'intention d'ensemble, dans les mots du user :

> Je ne veux pas faire une vidéo pour faire une vidéo. Je veux faire un bot qui
> déchire tout. Et avoir des niveaux suffisamment difficiles mais qui paraissent
> atteignables pour que même lui en chie.

Et le corollaire, qui commande tout le reste :

> Le jeu, même si je pense infaisable par un humain, doit rester à destination
> d'humains.

---

## 1. Pourquoi il faut ces deux concepts

Le bot passe aujourd'hui les vingt premiers niveaux sans réfléchir. Ce n'est pas
une opinion, c'est arithmétique : au niveau 20 l'objectif fait **86 cases**, la
poche du réservoir en offre **184**, et le bot dispose de **44 gestes gratuits**
avant que le flux ne parte (délai de 22 s constant, cadence 2 gestes/s). Rien ne
lui résiste.

Conséquence pour la vidéo : tout ce qui a été construit — planification, visée
adaptative, étude de bombe, croix — **ne sert à rien avant le niveau 28**. À
l'écran, les vingt premiers niveaux sont une formalité et la difficulté n'arrive
que quand le bot commence à mourir.

Ces deux concepts ne servent donc pas à affaiblir le bot. Ils servent à
**retirer le surplus**, pour que sa force soit visible dès le niveau 5 et que
les chantiers qui restent (les croix, la couverture) deviennent des nécessités
plutôt que des raffinements.

### Le critère de réglage

Pas « combien de niveaux atteints » mais **à quel prix** : le nombre de **vies
dépensées par niveau**. Aujourd'hui le bot passe la plupart des niveaux du
premier coup jusqu'au 30 — zéro tension à l'écran. Réglé pour qu'il en perde en
moyenne une par niveau, il gagne toujours, mais il paie, et ça se voit.

C'est cette courbe-là qu'il faudra mesurer, pas celle du niveau atteint.

---

## 2. L'achat de bombes

**Prix : au moins 6 500 points** (décision du user).

Le chiffre tient debout, et c'est le haut de la fourchette de ce qu'une bombe
rapporte. Mesuré le 19 septembre sur `--graine 2998034427`, une bombe bien
ciblée achète entre 20 et 130 cases de tracé :

| niveau | tracé avant | après la bombe |
|---:|---:|---:|
| 31 | 12 | 130 |
| 33 | 1 | 109 |
| 34 | 85 | 102 |
| 36 | 147 | 150 |

À 50 points la traversée, ça vaut **1 000 à 6 500 points**. Tarifée à 6 500, la
bombe n'est donc jamais un achat d'optimisation : seulement un sauvetage, qu'on
ne fait que pour passer un niveau qu'on allait perdre. C'est exactement le rôle
qu'elle a pris dans le code.

Et les deux monnaies restent comparables sans que l'une écrase l'autre : une vie
coûte 20 000 points au palier (`PALIER_VIE`), une bombe en coûterait le tiers.

**À trancher** : où l'achat se fait (entre deux manches ? pendant l'attente ?),
et si le bot a le droit d'acheter. S'il l'a, il lui faudra une règle — acheter
quand l'étude de bombe promet de faire passer le niveau, et seulement là.

---

## 3. Les diablotins

> Des petites saloperies qui piquent des morceaux de tuyaux.

### La règle qui les rend jouables

**Ils ne volent que du tuyau SEC** — jamais une case déjà traversée par le flux.
Sinon la conduite crève, le flux s'arrête net, et aucune anticipation ne protège
de ça : on meurt sans avoir rien pu faire.

### La règle qui les rend justes

Pour que le joueur ait un recours, il faut deux choses :

1. **Le type volé doit être dans la file visible**, la croix comptée comme joker
   d'un droit (`Trace::convient`). Sinon on voit le trou, on sait ce qu'il faut,
   et on ne l'a pas.
2. **Il doit être atteignable à temps.** Une pièce au rang *k* de la file coûte
   *k+1* gestes, soit (k+1)/2 secondes à la cadence du jeu. Le flux atteint un
   trou situé à *d* cases en *d* secondes. La réparation n'est possible que si :

   > **k < 2d − 1**

   Un trou à 2 cases du flux n'est réparable que si la pièce est en tête de
   file ; à 5 cases, elle peut être jusqu'au rang 8.

### La règle qui les rend crédibles

**`k < 2d − 1` est un VETO, pas un objectif.** S'il sert à choisir la victime,
le diablotin prend à chaque fois le vol le plus douloureux encore réparable — et
ça se verra : personne ne croira qu'une petite bestiole calcule la position du
flux et le rang de la file.

Le choix doit venir d'ailleurs : de **sa position**. Il a une case, une
trajectoire, et il pique ce qui est sous lui. Sa dangerosité vient de là où il
traîne, ce qui est visible à l'écran et anticipable :

- « il descend vers le flux, ça va faire mal » se lit ;
- « il a pris exactement la pièce qu'il ne fallait pas » se subit.

Le veto s'applique après coup : s'il est sur une case irréparable, il ne prend
pas, il passe son tour. Une hésitation qui, à l'image, ressemble à une bestiole
qui renifle.

Les molettes de difficulté deviennent alors toutes visuelles — **combien ils
sont, à quelle vitesse ils se déplacent, où ils apparaissent** — ce qui est bien
plus facile à régler à l'œil, et bien plus facile à croire, qu'un taux de vol
abstrait.

### Ce que ça coûte au bot

Un vol coûte **exactement un geste** (reposer la pièce). Le bot reçoit 2 gestes
par seconde et en dépense environ 1 par case, le flux avançant d'une case par
seconde : il peut donc absorber **environ un vol par seconde** avant que le
préfixe contigu ne décroche (voir TRACE.md §3).

Mais c'est *son* plafond, et un humain est très en dessous. Régler le taux sur
ce que le bot encaisse fabriquerait quelque chose d'insoutenable à regarder. Il
faut le régler sur le **surplus du niveau**, qui décroît avec N.

Bonne nouvelle côté code : le planificateur n'a rien à apprendre. Le tracé est un
chemin de *cases*, pas un inventaire de pièces posées — voler une pièce ne rend
pas la case moins libre. Et `BotTrace::caseAServir` relit le plateau à chaque
geste plutôt que de se souvenir de ce qu'il a posé : **une case dépouillée
redevient d'elle-même « à servir », au bon rang, sans une ligne de code.**

Ce qui changera, c'est la stratégie : avec des vols réguliers, la ressource rare
redevient le geste utile, et le bot devra probablement apprendre à garder de
l'avance sur le flux plutôt qu'à servir toujours le plus en amont (la règle du
§3 de TRACE.md). C'est un vrai chantier, et il n'existera que grâce aux
diablotins.

### Le piège d'ingénierie à ne pas rater

**Leurs déplacements doivent être tirés d'une graine dérivée de (graine de
partie, niveau)**, comme le plateau et la file (`Partie::grainePour`).

Tout ce qui a été construit repose sur le **rejeu parfait** : la mémoire du v4
et ses interdits, l'abandon sur rejeux identiques, l'étude de bombe qui promet
un tracé avant de le poser. Un diablotin qui se déplace au hasard réel casse les
trois d'un coup — et on ne s'en apercevrait que très tard, en cherchant ailleurs.

---

## 4. Ce qui reste avant

Dans l'ordre :

1. **Les croix** — l'auto-croisement. Prototype mesuré dans `banc/croix.cpp` :
   +12 passes en moyenne, 6 à 8 croix, une milliseconde, zéro tracé illégal sur
   72. Reste à l'intégrer, ce qui demande d'apprendre à `Trace` qu'une case peut
   porter deux rangs.
2. **La stabilisation des bombes** — la mesure appariée de la séance du 19
   septembre.
3. **La couverture** — la recherche trouve 123 cases sur une poche qui en offre
   155. Il reste une trentaine de cases sur la table, et c'est le dernier gros
   gisement du planificateur.
