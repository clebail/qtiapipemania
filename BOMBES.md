# Les bombes

Spécification arrêtée avec le user le 14 septembre 2026. **Rien n'est écrit :
ce fichier est le contrat, pas un journal.** Le §8 liste ce qui reste à
trancher — tout le reste est décidé.

---

## 1. Ce qui change

Le bot est bon, et il meurt quand même. Le diagnostic du user : **dans la quasi-
totalité des cas, il meurt par manque de place**, pas par mauvais choix — le
tirage ne lui donne pas les pièces qu'il faut là où il reste du terrain, et les
blocs fixes du niveau finissent de fermer les poches. Tous les outils du bot
(`culDeSac`, `tailleRegionVide`, `espaceApres`) savent *mesurer* la place ; aucun
ne sait en *créer*. La bombe est le premier moyen d'en créer.

Ce n'est donc pas la bombe du jeu original — une pièce qui arrive dans la file
et qu'il faut caser. C'est un **objet**, gagné, stocké, posé où l'on veut, au
même rang que les vies : une ressource qui se garde ou se dépense.

### Le fait technique qui porte tout

Le plateau d'un niveau se régénère **intégralement** depuis son tirage :

```cpp
// Partie::nouvelleManche()
plat->reinitialiser(nbCasesBloquees(), grainePour(grainePartie, niveauCourant, CANAL_PLATEAU));
```

`Game::reinitialiser()` repose les `nbCasesBloquees()` cases bloquées à partir de
la graine. Un bloc pulvérisé **revient donc de lui-même au rejeu**, et c'est
exactement ce qu'on ne veut pas. Le déminage devra vivre en dehors du tirage,
porté par `Partie` à travers les rejeux d'un même niveau (§2, *Rejeu*, et §4.A.6).

Deux bonnes nouvelles au passage : `tpBombe` existe déjà dans `ETypePiece`
(common.h:22), `dessinpiece.cpp:241` sait la dessiner, et le bot la traite déjà
comme un obstacle inerte (`bot.cpp:702`, `bot.cpp:728`). La pièce n'est jamais
tirée : `piecefile.cpp:54` borne le tirage à `tpCroix`, et **ça ne change pas**.

---

## 2. Les règles

### Stock

- **+1 bombe à chaque niveau comportant des blocs fixes**, plafond **10**, comme
  les vies. Aucune au départ. (Les deux plafonds sont passés de 9 à 10 le
  14 septembre 2026, avec l'affichage en deux rangées de cinq — §5, *Le stock*.)
- **Créditée à la réussite du niveau**, jamais à l'entrée : un niveau qu'on ne
  finit pas ne paie pas. Même règle que les vies, et même raison — sinon un
  niveau rejoué indéfiniment paierait à chaque tentative.
- `nbCasesBloquees() = min(24, (niveau − 1) × 2)` : le niveau 1 n'en donne donc
  pas, et c'est une bombe par niveau à partir du 2.
- Conséquence à retenir : **le stock sature au niveau 11** — une bombe par
  niveau à partir du 2, donc dix après le 11. Passé ce point, tout niveau
  traversé sans dépenser perd la bombe qu'il rapportait.

### Pose

- Sur une case **totalement vide**, et rien d'autre — pas sur un bloc, pas sur un
  tuyau, pas sur un rebut du tas.
- Joueur : **clic droit**. Bot : le même point d'entrée.
- Ne consomme pas la file, ne coûte aucun point.
- Une bombe posée **est un obstacle** tant qu'elle n'a pas sauté : le flux ne la
  traverse pas, on ne pose rien dessus. Poser une bombe devant sa propre tête,
  c'est se boucher — la conséquence est assumée, pas empêchée.

### Explosion

- **Un timer par bombe**, de durée **fixe**, décompté par le **timer général** —
  le `dt` de la simulation, dans `Partie::avancer()`, là où le flux avance.
  **2 à 3 secondes**, à confirmer aux premiers tests : il faut qu'un humain ait
  le temps de voir la bombe avant qu'elle saute.
- Il **suit donc l'accélération**, et c'est voulu. `facteurTemps()` n'agrandit
  jamais le `dt` : foncer, c'est jouer huit battements de 16 ms au lieu d'un. La
  bombe et le flux avancent au même rythme, donc elle saute toujours après le
  même nombre de cases remplies, à 1× comme à 8×. Sur l'horloge murale, foncer
  changerait la règle — on rattraperait une bombe mal posée en appuyant sur
  espace — et le banc, qui déroule sans horloge murale, ne pourrait plus
  reproduire une partie. Contrepartie assumée : à 8×, l'explosion dure ~0,3 s et
  ne se voit pas. C'est déjà le marché de l'accélération.
- Le souffle est le **3×3** : les 8 cases autour, plus la case de la bombe, qui
  disparaît avec.
- Il pulvérise **tout** : tuyaux vides, tuyaux pleins, rebuts du tas, et surtout
  **blocs fixes**.
- **Le réservoir est immunisé.** Il n'est ni détruit, ni déplacé, ni bombardable.
- **Une bombe prise dans un souffle saute en même temps que lui, et son propre
  souffle ne s'applique pas** : les cases qu'elle aurait touchées ne le sont
  pas. La zone détruite reste donc exactement le 3×3 de la bombe dont le timer
  est arrivé à terme, quel que soit le nombre de bombes prises dedans. Pas de
  chaîne, pas de récursion, pas de question de timing.
- Conséquence directe : **deux bombes à portée l'une de l'autre se gaspillent.**
  La seconde est consommée sans rien détruire de plus. Il faut les espacer d'au
  moins deux cases — au sens du 3×3, donc jamais dans le voisinage immédiat.

### Mort

- **Un tuyau plein dans le souffle = mort immédiate**, au même prix qu'une fuite
  ordinaire : une vie, et le niveau se rejoue.
- C'est une **défaite franche, même si l'objectif était déjà atteint** : la
  manche ne s'est pas arrêtée d'elle-même, on l'a fait sauter. La longueur ne
  décide plus. (Tranché à l'implémentation, `Partie::terminerManche(true)` — le
  seul cas où la règle ne découlait pas de la discussion.)
- Pendant `epAttente`, rien n'est encore plein : **bomber avant le départ du flux
  ne risque rien.** Tout le risque est dans `epEcoulement`.

### Rejeu

Une mort rejoue le même niveau, mêmes graines, comme aujourd'hui. S'y ajoute :

| ce qui a été détruit | revient au rejeu ? |
|---|---|
| blocs fixes, par une explosion **propre** | **non**, le déminage est acquis |
| blocs fixes, par l'explosion **qui tue** | **oui**, elle ne crédite rien |
| tuyaux et rebuts | sans objet, le plateau repart vide |

Et les **bombes dépensées ne sont jamais rendues**. Deux corollaires, confirmés
le 14 septembre 2026 :

- **une bombe posée ne survit pas à la fin de la manche.** Le plateau repart
  vide, et elle était déjà décomptée : celle qui n'a pas eu le temps de sauter
  est perdue comme les autres ;
- **le rejeu repart sans aucune bombe posée** — seul le déminage acquis le
  traverse.

Pourquoi l'explosion qui tue ne crédite rien : sans cette exception, bombarder
délibérément son propre tuyau plein deviendrait la façon la moins chère de
déminer — une vie contre du terrain, et la vie se regagne. Là, se tuer avec sa
bombe **annule exactement ce que la bombe rapportait**. Le farm est fermé.

Reste ouvert et assumé : **sacrifier une vie pour déminer** en explosant
proprement puis en mourant de sa mauvaise manche. Ça coûte une vie *et* les
bombes, c'est un vrai arbitrage, on le laisse au joueur.

Le déminage est **remis à zéro au passage au niveau suivant** : il vaut pour les
rejeux d'un niveau, jamais au-delà.

---

## 3. La stratégie du bot

Celle du user, et elle est sobre :

> **Garder les bombes pour les rejeux.** Premier essai sans rien : si la manche
> passe telle quelle, c'est tout bénéfice, les bombes sont encore là. Si elle est
> perdue, le rejeu se paie déjà d'une vie — autant y entrer avec du terrain
> dégagé.

Quatre conséquences.

1. **Le bot doit savoir qu'il rejoue.** Trivial : `numeroManche()` change à
   `niveau()` constant, et le bot tient déjà `mancheVue` pour repérer la nouvelle
   manche. Il lui suffit de compter les essais du niveau courant.
2. **Au-dessus du plafond, garder coûte.** Le stock sature au niveau 11 :
   à partir de là, une manche gagnée sans dépenser jette la bombe gagnée. Le bot
   aura intérêt à bomber dès le premier essai une fois saturé.
3. **Où bomber** — tranché par le user le 14 septembre 2026 par la règle du
   **grief** (les blocs qui l'ont fait galérer aux essais d'avant), et
   **remplacé le 17 septembre 2026** par le rendement du souffle après mesure.
   Voir §3 bis : le grief était une mesure honnête qui départageait mal, et qui
   ratait par construction le terrain muré.
4. **Quand bomber n'est plus le problème**, et c'est la conséquence heureuse du
   §3 : au rejeu, pendant `epAttente`, avant que le flux parte. Le timer fixe
   n'oblige plus à anticiper l'enfermement — on ne décide pas en cours de
   manche, on décide de la manche d'après. La limite du flood-fill (il ne voit
   pas que le bot va lui-même grignoter la zone ouverte avec son tuyau, le
   serpent qui se referme sur lui-même) reste entière ; la bombe ne la résout
   pas, elle la rend payante.

**Le motif à surveiller** : comme bomber pendant `epAttente` est gratuit,
l'optimum plat est de vider le stock au début de chaque manche rejouée et la
bombe devient un bouton « nettoyer le terrain », sans arbitrage. Le seul frein
est le délai — la zone est condamnée le temps que ça saute, donc bomber tôt coûte
du terrain au moment où on en a le plus besoin. **C'est le réglage de la durée
qui portera tout l'intérêt tactique**, et lui seul. Le bot, lui, n'en pose
**qu'une par essai** (choix du user) : le rejeu suivant en reposera une, sur le
terrain que celle-ci aura déjà ouvert.

---

## 3 bis. Où le bot bombe

La case **vide** dont le souffle emporte le plus de blocs (`peutMiner`, la
question du clic droit ; `Bot::choisirPaquetDeBlocs`). Le centre doit être libre,
donc le plafond est **huit**. Une bombe ouvre du terrain — c'est la seule chose
qu'elle sache faire, et la seule qu'on lui demande.

Ce n'est **pas** un paquet au sens de la connexité : quatre blocs éparpillés dans
le 3×3 valent quatre blocs collés. Ce qu'on mesure est le rendement du souffle.

À égalité, la première case rencontrée dans l'ordre de balayage — arbitraire,
mais reproductible, ce qui compte pour le rejeu.

### Ce que ça a remplacé : le grief, et pourquoi il est tombé

La première version suivait une règle du user : *viser les blocs qui l'ont fait
galérer les tours d'avant — tous les tours où il a été obligé de défausser une
pièce qu'il aurait pu mettre s'il n'y avait pas eu de bloc devant.*

Elle était mise en œuvre par un **contrefactuel**, et c'était la bonne façon de
la poser : on retirait un bloc du plateau pour de faux, on refaisait exactement
le test de pose, et s'il passait, ce bloc venait de coûter une pièce — +1 à son
compteur. Ça **nommait** le coupable au lieu de soupçonner le voisinage, ça
désignait le mur devant la sortie comme les deux blocs fermant une poche trois
cases plus loin, et ça acceptait de n'accuser personne.

**Mesuré le 17 septembre 2026, il a perdu contre le simple comptage de blocs :**

| | moyenne | ≥ 38 | ≥ 40 | ≥ 41 | max |
|---|---|---|---|---|---|
| grief | 37,54 | 361 | 89 | 18 | 42 |
| rendement du souffle | 37,85 | 407 | **129** | **35** | 43 |

600 parties appariées, écart **+0,31 de niveau moyen (t = +2,58)**, 264
améliorées contre 172 dégradées.

Deux défauts expliquent la chute, et aucun n'est réparable sans changer la
question posée :

- **il départage mal.** Sonde sur 277 bombes : **78 %** des poses avaient
  plusieurs cases à égalité au sommet — médiane cinq, jusqu'à quarante — et
  c'est alors l'ordre de balayage qui tranchait. Un bloc de grief zéro pèse
  zéro, donc la case qui pulvérise le coupable *plus sept innocents* marquait
  exactement comme celle qui ne prend que le coupable. Résultat : **1,76 bloc
  pulvérisé par bombe** quand le souffle en prend huit ;
- **il rate le terrain muré, par construction.** Le grief ne se déclenche que si
  `culDeSac` a refusé la pose, donc seulement quand le bot est déjà à l'étroit ;
  et `espaceApres` ne compte que la région atteignable **depuis la tête**, si
  bien qu'un bloc au cœur d'une zone murée n'est sur la frontière de rien. Il
  faut en plus que le retrait d'**un seul** bloc fasse basculer le test : plus le
  paquet est épais, moins il est coupable. Le grief mesurait donc la gêne locale
  au front de construction, jamais le coût structurel du terrain.

Le mécanisme entier a été retiré du code — `grief()`, `noterGriefs()`,
`choisirBombe()` et les deux compteurs. `noterGriefs` faisait un contrefactuel
complet sur les 225 cases **à chaque défausse**, soit 58 % des gestes du bot.

### Ce dont il se souvient, et jusqu'à quand

Plus rien. Le ciblage ne lit que le plateau du moment, donc il n'a aucun état à
tenir ni à remettre à zéro. La cible est connue **dès le premier battement** de
la manche : la bombe part tout de suite et saute à 2,5 s, au lieu d'exploser à la
dix-septième seconde au milieu de ce que le bot vient de construire — c'est ce
que faisait la toute première version.

### Quand il la dépense

Une par essai, et **jamais au premier essai d'un niveau** — la stratégie du user,
sobre : garder les bombes pour les rejeux. Le premier essai se joue sans rien ;
s'il passe, tout bénéfice.

Une exception, et c'est de l'arithmétique : **au plafond du stock**, un niveau
gagné sans dépenser jette la bombe qu'il rapportait. Garder coûte, donc le bot
bombe dès le premier essai. Mesuré sur une partie complète, le stock est collé au
plafond du niveau 12 au niveau 34 : c'est **23 bombes gratuites**, une par
niveau, et elles représentent la majorité du budget de la partie.

### Le garde-fou qui a coûté trois vies pour être trouvé

**`veutFoncer()` reste faux tant que notre bombe est en l'air.** Foncer lance le
flux tout de suite, le flux remplit du tuyau, et le souffle l'emporte : une vie,
et le déminage que la bombe venait de payer annulé avec elle (§2). Mesuré au banc
de comportement avant le correctif : le bot a demandé la barre d'espace sous sa
propre bombe **20 fois sur 149**, et s'est tué **3 fois**. L'accélération n'est
que retardée de 2,5 s au plus, pendant l'attente, là où il n'y a rien à regarder.

### Et le plan, qui date d'avant l'explosion

`construirePlan()` est refait **une fois, quand la bombe a sauté**. Il avait été
calculé au début de la manche, sur des blocs qui n'existent plus : sans ce
rappel, le terrain qu'on vient d'ouvrir n'entrerait jamais dans le réseau du tas.

---

## 4. Le plan, dans l'ordre

Au 14 septembre 2026, **les §A et §B sont entiers** : classe `Minage`, stock,
clic droit, compte à rebours, zone de souffle, prévisualisation, et l'explosion.
Restent le bot (§C) et le banc (§D), où est tout le rendement.

Le routage des boutons et le rendu sont vérifiés de la même façon : neuf
assertions sur les clics (droit sans stock, gauche, milieu, signaux), et des
captures offscreen — compte à rebours à trois fractions, souffle armé avec une
case pleine en rouge, fantôme au survol, fantôme au-dessus d'un tuyau plein,
puis le flash à quatre instants de sa vie, bombe posée sur un paquet de blocs.
La partie y est jouée par un bot jusqu'à ce qu'une bombe soit gagnée, ce qui
exerce au passage le crédit du stock en conditions réelles.

`Minage` est vérifié par un banc jetable de 32 assertions (pose, souffle,
réservoir immunisé, absence de chaîne, mort et déminage refusé). Il n'est pas
dans le dépôt : le projet n'a pas de cadre de test, et en poser un pour une
classe n'aurait pas été la bonne porte d'entrée.

### A. Le moteur

1. ✅ `Partie` : `bombesRestantes`, plafond `BOMBES_MAX` = 10 (common.h, parce
   que le panneau calibre ses rangées dessus), crédit en fin de niveau **réussi**
   dès que `nbCasesBloquees() > 0`, à côté de `crediterVies()` —
   `Partie::crediterBombes()`, 14 septembre 2026.
2. ✅ `Partie::poserBombe(col, row)` : refuse si le stock est vide, si la manche
   est finie, ou si la case n'est pas `tpNone` ; sinon décrémente et arme.
3. ✅ Les timers : `QVector<SBombe>` dans `Minage`, décrémenté depuis
   `Partie::avancer(dt)` avec le `dt` de la simulation — le même que le flux, et
   pour la même raison. En attente **et** en écoulement.
4. ✅ L'explosion, `Minage::exploser()` : le 3×3 sauf le réservoir — testé
   **avant** `estRempli`, sinon toute bombe voisine du réservoir tuerait, le
   réservoir étant rempli dès le départ du flux. Une bombe du souffle quitte la
   liste sans souffler : aucune récursion, un `tpNone` de plus.
5. ✅ `Partie::peutPoser()` refuse `tpBombe` comme `tpBloque` et `tpReservoir`.
6. ✅ Le déminage : `Minage::demine`, alimenté par les explosions **propres**
   seulement, vidé au changement de niveau. `Game` n'a pas bougé : la graine
   reposant les blocs aux mêmes cases, `appliquerDeminage()` les efface juste
   après `reinitialiser()`.
7. ✅ Le tas du bot : `Bot::oublierTasDetruit()`, appelée quand la signature du
   plateau change — même déclencheur que les obligations. Sans elle, l'overlay
   hachure du vide et `Bot::tete()` croit avoir un rebut à reprendre.

### B. L'interface

1. ✅ Clic droit sur `WGame` → `poserBombe`. Le bouton gauche pose un tuyau, le
   droit une bombe, **tout autre bouton ne fait rien** : avant, `mouseReleaseEvent`
   ne regardait pas le bouton et un clic du milieu déposait une pièce. Signal
   `bombePosee`, pour que le panneau se repeigne — il ne le fait pas de lui-même
   pendant l'écoulement.
2. ✅ Le stock dans `WPanneau` — `dessinerBombes()`, rangée sous les cœurs,
   14 septembre 2026.
3. ✅ Le compte à rebours : anneau, mèche qui se consume, pulsation du dernier
   tiers.
4. ✅ La zone de souffle des bombes armées, et la prévisualisation au survol —
   toutes deux adossées à `Partie::peutMiner()`.
5. ✅ L'explosion : flash blanc et onde ambre, sur l'horloge d'affichage.
   `Minage` tient un tampon d'événements (`preleverExplosions()`), `WGame` l'arme
   et l'anime lui-même — 14 septembre 2026.

Le détail de tout ça est au **§5**, qui est la partie du contrat où le jeu se
gagne ou se perd : la règle est invisible sans son habillage.

### C. Le bot — fait le 14 septembre 2026

1. ✅ `Bot::poserBombe(col, row)`, et `essaisNiveau` — 1 au premier passage, 2 au
   premier rejeu. Dans la classe de base : la bombe est une ressource, pas une
   stratégie, et tous les bots en héritent.
2. ✅ La décision, §3 bis : `choisirPaquetDeBlocs()` pour la cible,
   `gererBombes()` pour le moment (une par essai, pendant `epAttente`, dès le
   premier battement). Saturé au plafond, il bombe dès le premier essai —
   garder ne rapporterait plus rien (§3.2).

   *Au 14 septembre, la cible était choisie par le grief (`noterGriefs()` dans
   `defausser()`, `choisirBombe()`), `choisirPaquetDeBlocs` ne servant que de
   repli au plafond. Le 17 septembre, mesure à l'appui, le repli est devenu le
   seul critère et le grief a été retiré du code — §3 bis.*
3. ✅ Deux garde-fous trouvés au banc de comportement : ne pas foncer sous sa
   propre bombe (3 morts sur 149 avant), et refaire le plan quand elle a sauté.

**Ce que le banc de comportement dit, et rien de plus** — 10 parties, non
appariées, aucune comparaison possible avec un bras sans bombes : 361 manches,
84 bombes posées, 107 blocs ouverts, zéro bombe encore armée une fois le flux
parti. C'est une vérification, **pas une mesure de rendement** : celle-là, c'est
le §D, et il est ajourné.

### D. Le banc — ajourné

Mesure appariée, même graines, bras avec et sans bombes. Sans elle, on ne saura
pas si le §C paie — et le banc n'est pas dans le dépôt (voir VIES.md §C).

**Ajourné par le user le 14 septembre 2026** : « on verra plus tard ». Le §C
n'attend donc que sa spécification, pas une mesure.

---

## 5. L'habillage

Tout le jeu est vectoriel (`dessinpiece.cpp`) : teintes plates, alpha, hachures,
aucune image source — d'où la taille de case choisie à l'exécution. La bombe s'y
range sans rien inventer. Quatre rôles occupent déjà la palette ; **l'ambre est
libre, et c'est la couleur de la bombe.**

| rôle | teinte | où |
|---|---|---|
| vies | `0xe84858`, rouge vif | `WPanneau::dessinerVies()`, wpanneau.cpp:119 |
| tas de défausse | `0x6e7aa8`, bleu-gris terne | wgame.cpp |
| anticipation | `0x9b6ec8`, violet | wgame.cpp:209 |
| pari | `0xd85454`, rouge sourd | wgame.cpp:240 |
| **bombe, souffle, stock** | `0xf09a2c` ambre, corps `0x8e98b8` | `WPanneau::dessinerBombes()` |
| **urgence** (dernier tiers, case pleine menacée) | `0xe84838` rouge | `dessinerCompteARebours()`, `dessinerSouffle()`, wgame.cpp |

### Le stock

Une grille jumelle de celle des cœurs, juste en dessous : `dessinerBombes()`
calquée sur `dessinerVies()`, même pas, et les **dix du plafond en deux rangées
de cinq** — le pas se calcule sur une rangée, donc cinq par rangée au lieu de dix
double la taille des icônes à largeur de panneau égale. Les deux rangées sont
comptées même à stock nul, pour que le panneau ne sautille pas quand le stock
change ; une dernière rangée incomplète reste centrée sous le milieu
(`placeIcone()`, wpanneau.cpp). Mèche ambre et **corps clair** — la première version
disait « anthracite », mais le panneau est peint sur fond noir : l'anthracite n'y
survit pas. Ce qui compte est tenu : surtout pas un cœur de plus, l'œil doit lire
« objet », pas « vie ».

### Le compte à rebours

C'est le morceau qui compte, puisque le délai porte à lui seul tout l'intérêt
tactique (§3). Les trois signaux se cumulent, et ils ne disent pas la même chose.

1. Un **anneau qui se vide** sur le pourtour de la case, façon arc de cooldown :
   la mesure exacte, sans texte, et ça reste lisible à 24 px (`TAILLE_CASE_MIN`).
2. La **mèche qui raccourcit** : le même chiffre, mais dessiné dans l'objet.
3. Dans le **dernier tiers** (`SEUIL_URGENCE`, 0,34 — moins d'une seconde sur
   2,5), une pulsation qui s'accélère : le seul des trois qui attrape l'œil quand
   on regarde ailleurs sur le plateau. Elle se calcule sur la **fraction**, qui
   décroît linéairement, et jamais sur une horloge : le dessin reste sans état,
   et l'affolement suit l'accélération comme la bombe qu'il annonce.

#### Le piège du cache de pixmaps

`dessinerPiece()` met en cache une image par couple (type, sens) et par taille,
parce qu'« une pièce ne change pas d'aspect ». **La bombe est la seule qui en
change à type et sens égaux** : sa mèche se consume. Elle passe donc à côté du
cache et se trace directement, sinon la première image serait figée pour toute la
vie de la bombe. Le coût est d'une case sur 225, et seulement tant qu'une bombe
est posée.

La mèche est tronquée exactement, par De Casteljau — deux interpolations, et la
sous-courbe de 0 à *t* d'une quadratique est (P0, A, C). Qt ne sait pas couper un
`QPainterPath`, et l'approcher en segments se verrait sur une courbe aussi
courte.

### La zone de souffle, montrée dès la pose

Le 3×3 en surimpression ambre légère, dans la grammaire des marqueurs existants :
remplissage à alpha faible plus un liseré. Sans elle, le joueur **subit** la
règle au lieu de la jouer.

Et surtout : **les cases du souffle déjà pleines pulsent en rouge franc.** La
mort immédiate (§2) cesse d'être une punition pour devenir un avertissement.
Vaut autant quand c'est le bot qui joue — on voit ce qu'il a choisi de risquer.

Deux détails qui ne se devinent pas : **le réservoir n'est jamais teinté**, sous
peine de promettre une destruction qui n'aura pas lieu (§2, il est immunisé) ; et
une case couverte par plusieurs bombes retient **la plus urgente**, sinon deux
voisines la font battre à contretemps. La carte des menaces se reconstruit en
relisant le plateau, sans que `Minage` ait à publier sa liste.

### L'explosion

Un flash blanc sur le 3×3 qui s'efface, et un liseré qui se dilate. **250 ms**,
et c'est le seul morceau du jeu qui ne compte pas en temps simulé.

**Point de méthode** : la destruction est instantanée côté moteur, donc
l'animation peut vivre sur l'**horloge d'affichage** et non sur le `dt` de
simulation. Elle garde alors ses 0,25 s même à 8×, sans rien changer à la
partie — la simulation reste maîtresse, le décor est libre. C'est la seule façon
de ne pas perdre l'effet sous accélération, dont le §2 assume par ailleurs qu'il
comprime le timer.

#### Une explosion ne laisse aucune trace : il faut la relever au vol

C'est tout le problème de ce morceau-là. Le compte à rebours et le souffle se
**relisent** sur le plateau à chaque image ; l'explosion, elle, est un
**événement** — une fois passée, un 3×3 pulvérisé ressemble exactement à un 3×3
qui n'a jamais rien porté. D'où un tampon dans `Minage`, rempli par `exploser()`
avant même de toucher au plateau et vidé par `preleverExplosions()` : qui ne
relève pas ne paie rien, et le moteur n'apprend rien de l'affichage.

`MainWindow::battement()` le relève **après la rafale**, pas à chaque battement
unitaire : à 8×, huit pas de simulation ne font qu'un seul instant vécu, et deux
bombes de la même rafale doivent s'éteindre ensemble.

Ensuite `WGame` anime seul, sur **son propre `QTimer`** et un `QElapsedTimer`.
Ce n'est pas un luxe : la fenêtre cesse de repeindre dès que la manche est finie,
et la manche finie est justement le cas de l'explosion qui tue — son flash
resterait figé sur sa première image, la seule qu'on ne veut pas montrer seule.
Le timer ne tourne que tant qu'il reste un flash.

#### Les deux gestes ne décroissent pas pareil

Le flash s'éteint en `(1-t)²` — l'essentiel est passé à mi-course, là où une
décroissance linéaire donne l'impression de traîner. L'onde fait l'inverse,
`1-(1-t)²` : elle part vite et ralentit. Deux gestes qui décroîtraient de la
même façon se confondraient en un seul fondu.

L'onde **déborde** le 3×3 détruit, d'environ trois quarts de case : le souffle
s'arrête là, et l'œil a besoin de la voir y arriver. Elle est ambre, comme la
bombe et comme le souffle qu'elle vient d'accomplir — c'est la même chose qu'on
montre, une fois annoncée et une fois faite. Blanc pour le flash, et c'est le
seul moment où le plateau parle plus fort que l'ambre.

Deux bornes : le tout est **clippé à la grille**, puisque `Minage::exploser()`
borne déjà le 3×3 au plateau et qu'une onde sur la marge noire mentirait ; et le
**réservoir n'est pas blanchi**, même pris dans le souffle — il a survécu, le
couvrir de blanc dirait le contraire. C'est l'exception qui court déjà partout
ailleurs dans ce fichier.

### La prévisualisation à la pose

Au survol, le 3×3 en fantôme **avant** le clic droit : pointillé et sans
remplissage, pour qu'on ne confonde pas ce qui est armé avec ce qu'on envisage.
Le rouge, lui, est déjà franc — c'est justement l'erreur que la prévisualisation
existe pour éviter. Deux bombes à portée l'une de l'autre se gaspillant (§2),
c'est ce fantôme qui empêchera l'erreur qu'on commet sans comprendre pourquoi.

Le fantôme n'apparaît **que là où le clic droit aboutirait** : la question posée
au survol est exactement celle du clic, et c'est la même fonction qui répond —
`Partie::peutMiner()`, où sont réunis les trois refus (stock vide, manche finie,
case occupée). La grille ne réimplémente aucune règle, elle traduit un curseur.

**La case barrée sur case non vide a été écartée à l'écriture.** Sur une case
occupée, le clic *gauche* reste permis — c'est un remplacement — donc barrer la
case dirait faux la moitié du temps. L'absence de fantôme est le refus : elle ne
dit rien de plus que ce qui est vrai. À rediscuter si ça se révèle trop discret
manette en main.

### Écarté

**Le contour fantôme des blocs déminés**, qui montrerait au rejeu ce que les
explosions précédentes ont ouvert. Joli comme récit, mais le plateau porte déjà
trois overlays de bot, et l'information ne sert aucune décision : la case est
libre, ça se voit.

---

## 6. Ce qui n'est PAS au programme, et pourquoi

- **La bombe dans la file.** C'est la bombe du jeu original, et elle pose le
  problème inverse : une pièce imposée dont il faut se débarrasser. Le tirage
  reste borné à `tpCroix`.
- **La chaîne, sous toutes ses formes.** Une bombe atteinte saute sans souffle :
  la zone détruite ne dépend jamais que de la bombe qui arrive à terme. Une
  chaîne, même d'un cran, rendrait une pose illisible pour le joueur et
  incalculable pour le bot.
- **Le réservoir bombardable.** Manche perdue d'office, aucun intérêt.
- **Le farm par suicide**, fermé par la règle des blocs non acquis (§2).

---

## 7. Repères de code

| quoi | où |
|---|---|
| le type existe déjà, et n'est pas tiré | `tpBombe`, common.h:22 ; `piecefile.cpp:54` |
| déjà dessinée | `dessinpiece.cpp:241` |
| déjà traitée comme obstacle inerte par le bot | `bot.cpp:702`, `bot.cpp:728` |
| plateau régénéré depuis la graine (le point dur) | `Partie::nouvelleManche()`, `Game::reinitialiser()` |
| nombre de blocs du niveau | `Partie::nbCasesBloquees()`, `BLOQUEES_PAS`, `BLOQUEES_MAX`, partie.cpp:63 |
| plafond des deux ressources, partagé avec l'affichage | `VIES_MAX`, `BOMBES_MAX`, common.h |
| crédit d'une ressource | `Partie::crediterVies()`, `Partie::crediterBombes()`, partie.cpp |
| ce qui interdit une case à la pose | `Partie::peutPoser()`, partie.cpp |
| case remplie par le flux (la mort du §2) | `Ecoulement::estRempli()` |
| l'accélération : N battements de 16 ms, jamais un `dt` multiplié | `MainWindow::facteurTemps()`, `MainWindow::battement()`, mainwindow.cpp |
| où décompter les timers, avec le flux | `Partie::avancer()`, partie.cpp |
| le rejeu, et ce qu'il remet à zéro | VIES.md §4.A |
| mesure de la place, côté bot | `Bot::espaceApres()`, `Bot::tailleRegionVide()`, bot.cpp |
| la case à bomber, et quand la dépenser | `Bot::choisirPaquetDeBlocs()`, `Bot::gererBombes()`, bot.cpp |
| ne pas foncer sous sa propre bombe | `Bot::veutFoncer()`, bot.cpp |
| le tas à nettoyer après une explosion | `Bot::tas`, `Bot::estTas()`, bot.cpp |
| la rangée de cœurs à cloner pour le stock | `WPanneau::dessinerVies()`, wpanneau.cpp:118 |
| la bombe déjà dessinée | `dessinerBombe()`, dessinpiece.cpp:241 |
| la grammaire des overlays (alpha + hachures) | `dessinerMarqueur*()`, wgame.cpp |
| la règle du minage, posée une fois pour le clic et pour le survol | `Partie::peutMiner()`, partie.cpp |
| l'explosion comme événement, et son relevé | `Minage::preleverExplosions()`, minage.cpp ; `MainWindow::battement()` |
| le flash et son horloge, à part du `dt` | `WGame::releverExplosions()`, `DUREE_FLASH_MS`, `dessinerFlash()`, `dessinerOndeFlash()`, wgame.cpp |
| taille de case choisie à l'exécution, d'où le vectoriel | `tailleCase()`, `TAILLE_CASE_MIN`, common.h |

---

## 8. Tranché

1. **La durée du timer : 2,5 s**, retenue par le user le 14 septembre 2026 après
   l'avoir eue en main (`DUREE_BOMBE`, minage.h — 156 battements de 16 ms). Le
   contrat n'a donc plus de point ouvert : c'était le dernier, et c'est lui qui
   décide si la bombe est un choix ou un bouton (§3).
