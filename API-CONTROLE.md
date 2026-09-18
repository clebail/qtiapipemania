# Protocole de contrôle de qtiapipemania

Ce document spécifie le protocole permettant de jouer une partie de qtiapipemania depuis un
programme extérieur : lire l'état du plateau, y poser des pièces et des bombes, et enchaîner
les parties.

## 1. Conventions

Les mots-clés **DOIT**, **NE DOIT PAS**, **DEVRAIT**, **NE DEVRAIT PAS**, **PEUT**,
**REQUIS** et **OPTIONNEL** sont à interpréter au sens de la RFC 2119.

« Le serveur » désigne l'implémentation exposant ce protocole, « le client » le programme qui
s'y connecte. Le fonctionnement interne du serveur est hors du champ de ce document.

## 2. Transport

Le serveur écoute en **TCP sur `127.0.0.1`, port `9000`**.

Le serveur **N'ÉCOUTE PAS** sur une autre interface. Un client s'exécutant sur une autre
machine **DOIT** établir un tunnel vers la boucle locale du serveur, par exemple :

```bash
ssh -L 9000:127.0.0.1:9000 utilisateur@machine
```

Le client **DEVRAIT** positionner `TCP_NODELAY`. Les messages sont courts et fréquents ; sans
cette option, l'algorithme de Nagle peut ajouter jusqu'à 40 ms de latence par message.

Le serveur n'accepte **qu'un seul client** à la fois, sans file d'attente. Une connexion
surnuméraire reçoit un refus puis est fermée :

```json
{"type":"reponse","ok":false,"erreur":"un client est deja connecte"}
```

### 2.1 Cycle de vie de la session

Tant qu'aucun client n'est connecté, la partie est **figée** : un client qui se connecte
hérite d'un plateau intact.

L'établissement de la connexion **démarre la partie**. À partir de cet instant elle s'écoule
en temps réel et **ne peut plus être figée** (§5.6). À la fermeture de la connexion, elle est
figée de nouveau.

Pendant toute la session, le client est **le seul joueur** : aucune autre source de gestes
n'agit sur la partie.

## 3. Format des messages

Le protocole est en **NDJSON** : chaque message est un objet JSON sérialisé sans saut de
ligne, terminé par `\n` (U+000A). Les deux sens utilisent ce cadrage.

Le client **NE DOIT PAS** supposer un ordre entre les réponses et les états poussés : les
deux flux sont **entrelacés sur la même socket**.

### 3.1 Champs communs

Tout message émis par le serveur porte :

| Champ | Type | Sens |
|---|---|---|
| `type` | chaîne | `"reponse"` ou `"etat"` |
| `t` | nombre | horodatage Unix en secondes |
| `seq` | entier | numéro de séquence croissant, propre à la connexion |

### 3.2 Requête

```json
{"cmd": "pose", "args": {"x": 7, "y": 5}, "id": 42}
```

| Champ | Présence | Sens |
|---|---|---|
| `cmd` | **REQUIS** | nom de la commande (§5) |
| `args` | selon la commande | objet d'arguments |
| `id` | **OPTIONNEL**, RECOMMANDÉ | valeur quelconque, reprise dans la réponse |

Une ligne vide est ignorée. Une ligne qui n'est pas un objet JSON valide, ou dépourvue de
`cmd`, produit une réponse d'erreur.

Le client **DEVRAIT** fournir `id` : c'est le seul moyen d'apparier une réponse à sa requête
lorsque plusieurs commandes sont en vol.

### 3.3 Réponse

Émise pour **chaque** requête, dans l'ordre de réception.

```json
{"type":"reponse", "cmd":"pose", "id":42, "ok":true,
 "type_piece":"croix", "etat":{…}, "t":1789663741.42, "seq":128}
```

| Champ | Présence | Sens |
|---|---|---|
| `ok` | toujours | `true` si la commande a pris effet |
| `cmd` | sauf erreur de format | nom repris de la requête |
| `id` | si fourni | valeur reprise de la requête |
| `erreur` | si `ok` vaut `false` | description de l'échec |
| `motif` | selon la commande | cause précise du refus |
| `etat` | commandes agissant sur la partie | état résultant (§6) |

Un refus **n'est pas** une erreur de protocole : une pose refusée est une réponse normale
portant `ok: false` et un `motif`.

### 3.4 État poussé

Le serveur émet spontanément un état toutes les **100 ms** tant qu'un client est connecté.

```json
{"type":"etat", "etat":{…}, "t":1789663741.52, "seq":129}
```

## 4. Garanties

Le serveur garantit que :

- toute requête reçoit exactement une réponse ;
- le client est le seul joueur pendant la session (§2.1) ;
- `seq` croît strictement sur une connexion donnée.

Le serveur **ne garantit pas** que :

- tous les états périodiques sont émis. Si le client cesse de lire, des états **PEUVENT** être
  omis, et `seq` présente alors une discontinuité. Les réponses ne sont jamais omises ;
- le rythme des états est exactement de 100 ms.

Le client **NE DOIT PAS** déduire l'état de la partie de ses propres actions : seul l'objet
`etat` fait foi.

## 5. Commandes

| Commande | Arguments | Effet |
|---|---|---|
| `etat` | — | renvoie l'état, sans effet de bord |
| `pose` | `x`, `y` | pose la pièce en tête de file |
| `bombe` | `x`, `y` | pose une bombe |
| `espace` | — | lance le flux, enchaîne la fin de manche, accélère |
| `reset` | `graine`, `niveau`, `vies`, `bombes` — tous OPTIONNELS | nouvelle partie |
| `aide` | — | liste des commandes disponibles |

Une commande inconnue produit `ok: false` et la liste des commandes.

### 5.1 `pose`

Pose **la pièce en tête de file** (`etat.file[0]`) sur la case donnée.

`x` et `y` sont **REQUIS** et **DOIVENT** être des entiers JSON ; une chaîne est refusée.
L'origine est en haut à gauche. Hors du plateau, la réponse porte
`erreur: "hors du plateau"` et aucun `motif`.

La réponse porte `type_piece`, le type de la pièce posée — la file ayant déjà avancé lorsque
le client relit l'état.

**Cadence.** Une pose **NE PEUT PAS** être acceptée moins de **0,5 s de temps de jeu** après
la précédente : on ne joue pas plus vite que le jeu ne l'autorise. Une pose trop précoce est
refusée avec `motif: "trop tot"` ; la réponse porte alors `attendre_secondes` et
`attendre_battements`. Le champ `etat.prochaine_pose` donne à tout instant le délai restant.

Une pose refusée **ne consomme pas** de crédit, quel qu'en soit le motif.

| `motif` | Cause |
|---|---|
| `trop tot` | la cadence n'est pas respectée |
| `case deja traversee par le flux` | le flux est déjà passé sur cette case |
| `bloc`, `reservoir`, `bombe armee` | la case porte un élément intouchable |
| `case interdite` | autre cause |
| `manche finie` | la partie n'est ni en `attente` ni en `ecoulement` |

Une case portant un **tuyau non traversé PEUT** être écrasée : la pose est acceptée et coûte
la pénalité de remplacement (§7.3).

### 5.2 `bombe`

Pose une bombe sur la case donnée. Acceptée **à tout moment** d'une manche — en `attente`
comme en `ecoulement` — si le stock n'est pas vide et si la case est **totalement vide** :
ni bloc, ni tuyau, ni rebut, ni bombe.

Une bombe **NE PEUT PAS** être posée sur un bloc : un bloc n'est jamais le centre d'une
explosion, seulement une cible de son souffle.

Motifs : `stock vide`, `la case doit etre totalement vide`.

Comportement de la bombe : §7.5.

### 5.3 `espace`

Équivaut à la barre d'espace du jeu. Trois effets, selon l'état courant :

| État | Effet |
|---|---|
| `attente` | le flux part immédiatement ; **+200 points**, crédités à l'instant de l'appel |
| `ecoulement` | le temps de jeu passe à **×8** |
| `reussie`, `perdue`, `gameover` | la pause de fin de manche est sautée |

Hors de `attente`, `espace` ne rapporte aucun point.

L'accélération **NE PEUT PAS** être annulée : elle tient jusqu'à la fin de la manche. Elle
porte sur le temps de jeu et non sur la cadence de pose (§5.1), qui reste de 0,5 s de temps
de jeu — le client dispose donc de huit fois moins de gestes par case traversée.

### 5.4 `reset`

Démarre une partie neuve.

| Argument | Défaut | Effet |
|---|---|---|
| `graine` | aléatoire | fixe la partie : à graine égale, plateau et file sont identiques |
| `niveau` | 1 | niveau de départ |
| `vies` | 3 | borné à 10 |
| `bombes` | 0 | borné à 10 |

`graine` **DOIT** être un entier JSON si elle est fournie. Le crédit de pose est rendu
immédiatement.

### 5.5 `aide`

Renvoie `commandes`, le tableau des noms acceptés **dans l'état courant**. Le client
**DEVRAIT** s'y référer plutôt qu'à une liste figée.

### 5.6 Commandes indisponibles

`pause` et `pas` existent dans le jeu mais **ne sont pas disponibles** pour un client : la
partie s'écoule en temps réel et son horloge n'est pas pilotable. Elles répondent
`motif: "horloge verrouillee"` et n'apparaissent pas dans `aide`.

## 6. Objet d'état

```json
{
  "partie":   {"graine":547312671, "manche":12, "niveau":39, "etat":"ecoulement"},
  "compteurs":{"score":4650, "vies":5, "bombes":3,
               "objectif":162, "tracee":88, "traversees":41, "remplacements":12},
  "depart":   {"x":7, "y":8, "sens":"haut", "fraction":0.42, "secondes":9.2},
  "file":     ["croix","coudeBG","horizontal","vertical","coudeHD"],
  "plateau":  ["...#..R........", …],
  "remplies": ["...............", …],
  "tete":     {"x":7, "y":5, "entree":"bas"},
  "aval":     6,
  "prochaine_pose": 0.0,
  "en_pause": false,
  "bombes_armees":[{"x":3,"y":0,"reste":0.61}]
}
```

### 6.1 `partie`

| Champ | Sens |
|---|---|
| `graine` | rejoue la partie à l'identique via `reset` |
| `manche` | incrémenté à chaque manche : réussie, perdue **ou rejouée** |
| `niveau` | niveau courant |
| `etat` | `attente`, `ecoulement`, `reussie`, `perdue`, `gameover` |

Les poses ne sont acceptées qu'en `attente` et `ecoulement`.

Au passage en `gameover`, `niveau` **revient au niveau de départ**. Un client qui veut
connaître le niveau atteint **DOIT** le relever en cours de partie.

### 6.2 `compteurs`

| Champ | Sens |
|---|---|
| `score` | cumulé sur toute la partie ; remis à zéro par `reset` seulement |
| `vies`, `bombes` | stocks courants, bornés à 10 |
| `objectif` | traversées requises pour gagner la manche |
| `traversees` | traversées **réalisées** par le flux dans la manche courante |
| `tracee` | longueur de la chaîne **bâtie** depuis le réservoir |
| `remplacements` | nombre de poses sur case occupée |

`traversees` et `tracee` ne sont pas la même grandeur, et l'issue ne se juge que sur la
première (§7.2) :

- `traversees` n'évolue qu'en `ecoulement` et repart de zéro à chaque manche ;
- `tracee` est calculée sur le plateau courant, indépendamment du flux. Elle **augmente
  pendant `attente`** à mesure que le client construit, et vaut ce que `traversees` atteindra
  si plus rien n'est posé. Elle **PEUT diminuer** : écraser une pièce de la chaîne la coupe,
  un souffle de bombe aussi.

On a toujours `traversees <= tracee`.

### 6.3 `depart`

Position, orientation et compte à rebours du réservoir. `secondes` est le temps de jeu
restant avant le départ du flux ; `fraction` la même valeur ramenée à `[0,1]`.

### 6.4 `file`

Les cinq prochaines pièces, dans l'ordre. **`file[0]` est celle que `pose` déposera.**

Types : `horizontal`, `vertical`, `coudeHG`, `coudeHD`, `coudeBG`, `coudeBD`, `croix`.

### 6.5 `plateau`

Quinze chaînes de quinze caractères. `plateau[y][x]` désigne la colonne `x`, ligne `y`.

| Car. | Pièce | Ouvertures |
|---|---|---|
| `.` | vide | — |
| `-` | horizontal | gauche, droite |
| `\|` | vertical | haut, bas |
| `J` | coude haut-gauche | haut, gauche |
| `L` | coude haut-droite | haut, droite |
| `7` | coude bas-gauche | bas, gauche |
| `F` | coude bas-droite | bas, droite |
| `+` | croix | les quatre ; traversée **tout droit**, une fois par axe |
| `R` | réservoir | son orientation, donnée par `depart.sens` |
| `B` | bombe armée | aucune |
| `#` | bloc | aucune |

Aucune orientation n'est transmise pour les tuyaux : un coude porte la sienne dans son type.

### 6.6 `remplies`

Même géométrie que `plateau`. Indique ce que le flux a traversé, **par axe** :

| Car. | Sens |
|---|---|
| `.` | intacte |
| `h` | axe horizontal traversé |
| `v` | axe vertical traversé |
| `X` | les deux |

Les deux axes sont distingués parce qu'une croix se traverse une fois par axe.

### 6.7 `tete`

Case où le flux débouchera, et côté par lequel il y entrera. Vaut `null` lorsque le tracé n'a
plus de suite.

```json
{"x": 7, "y": 5, "entree": "bas"}
```

Une pièce posée sur cette case **DOIT** avoir une ouverture du côté `entree` pour que le flux
la traverse. Le serveur **N'IMPOSE PAS** cette contrainte : une pièce qui ne se raccorde pas
est acceptée, et le flux s'arrêtera là.

### 6.8 Autres champs

| Champ | Sens |
|---|---|
| `aval` | cases de tuyau déjà posé que le flux doit parcourir avant d'atteindre la tête ; `0` en `attente` |
| `prochaine_pose` | temps de jeu avant la prochaine pose acceptée ; `0` si immédiate |
| `en_pause` | vrai lorsque la partie est figée |
| `bombes_armees` | bombes posées non explosées, avec `reste` dans `[0,1]` |

## 7. Règles du jeu

Rappelées dans la mesure où elles sont nécessaires à l'interprétation de l'état.

### 7.1 Déroulement

Le flux part du réservoir après le délai porté par `depart.secondes`, et parcourt le tuyau
sans jamais s'arrêter. La manche s'achève lorsqu'il ne peut plus avancer.

Tout ce que le jeu compte en secondes — cadence de pose, compte à rebours, mèche d'une bombe
— est du **temps de jeu**. Il s'écoule au rythme réel, et l'accélération de `espace` le fait
défiler huit fois plus vite.

### 7.2 Issue d'une manche

À l'arrêt du flux, la manche est **réussie** si `traversees >= objectif`, **perdue** sinon.

Une manche perdue coûte une vie et **rejoue le même niveau** : plateau et file sont
identiques. La partie s'achève lorsque la dernière vie est perdue.

Une manche est perdue **quelles que soient les traversées** si un souffle de bombe emporte
une case déjà traversée (§7.5).

### 7.3 Barème

| Événement | Points |
|---|---|
| Case traversée par le flux | +50, crédités à la fin de la manche |
| Pose sur case occupée | −25, immédiat, plancher à 0 |
| Départ anticipé (`espace` en `attente`) | +200, immédiat |

Les deux gains n'arrivent pas au même moment : un écart de `score` mesuré sur la durée d'une
manche ne contient pas la prime si l'échantillon initial a été pris après l'appel à `espace`.

Gains de stock : +1 vie tous les 20 000 points, +1 vie pour une manche réussie d'au moins
110 traversées, +1 bombe par niveau réussi comportant des blocs. Les deux stocks sont bornés
à 10.

### 7.4 Progression

| Grandeur | Valeur |
|---|---|
| Objectif | `4 × niveau + 6`, borné à 220 |
| Cases bloquées | `2 × (niveau − 1)`, borné à 24 |
| Délai avant départ | 22 s, constant |
| Remplissage d'une case | 1,50 s au niveau 1, décroissant jusqu'à 1,00 s |

Le plateau compte 225 cases, dont une occupée par le réservoir.

Deux règles conditionnent la longueur atteignable :

- **la croix se traverse deux fois**, une fois par axe : le tracé peut se croiser lui-même ;
- **un tuyau posé reste remplaçable** tant que le flux ne l'a pas traversé.

### 7.5 Bombes

Une bombe explose **2,5 s de temps de jeu** après sa pose. Le souffle couvre les **neuf
cases** de son 3×3, tronqué au bord du plateau.

- toutes les cases du souffle sont vidées, **y compris les tuyaux posés par le client** ;
- le réservoir est **immunisé** ;
- une bombe prise dans un souffle est **annulée sans exploser** : il n'y a pas de réaction en
  chaîne ;
- si une case du souffle a **déjà été traversée par le flux**, la manche est perdue et le
  déminage **n'est pas crédité** ;
- sinon les blocs détruits le restent pour **tous les rejeux du niveau**.

En `attente`, aucune case n'est traversée : une bombe ne peut donc pas y provoquer de défaite.

## 8. Exemple de client

```python
import json, socket

OUVERTURES = {
    "horizontal": {"gauche", "droite"},   "vertical": {"haut", "bas"},
    "coudeHG": {"haut", "gauche"},        "coudeHD": {"haut", "droite"},
    "coudeBG": {"bas", "gauche"},         "coudeBD": {"bas", "droite"},
    "croix": {"haut", "bas", "gauche", "droite"},
}

s = socket.create_connection(("127.0.0.1", 9000), timeout=10)
s.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
f = s.makefile("rw")
n = 0

def commander(cmd, **args):
    global n
    n += 1
    f.write(json.dumps({"cmd": cmd, "args": args, "id": n}) + "\n")
    f.flush()

def case_libre_eloignee(e):
    t = e["tete"]
    loin, best = None, -1
    for y, ligne in enumerate(e["plateau"]):
        for x, c in enumerate(ligne):
            d = abs(x - t["x"]) + abs(y - t["y"])
            if c == "." and d > best:
                loin, best = (x, y), d
    return loin

# Tout message porteur d'un `etat` est exploitable, qu'il soit reponse ou etat
# spontane. L'`id` ne sert qu'a retrouver un `motif` de refus.
for ligne in f:
    e = json.loads(ligne).get("etat")

    if not e or e["partie"]["etat"] not in ("attente", "ecoulement"):
        continue
    if e["prochaine_pose"] > 0 or not e["tete"]:
        continue

    tete, piece = e["tete"], e["file"][0]

    if tete["entree"] in OUVERTURES[piece]:
        commander("pose", x=tete["x"], y=tete["y"])
    else:
        x, y = case_libre_eloignee(e)
        commander("pose", x=x, y=y)
```

Cet exemple illustre le cadrage des messages et l'usage de `prochaine_pose`. Les décisions de
jeu qu'il prend n'engagent que lui.

## 9. Erreurs fréquentes

| Erreur | Conséquence |
|---|---|
| Lire la réponse par un simple `readline` | on obtient un état spontané ; démultiplexer sur `type` et `id` |
| Lire `niveau` après `gameover` | il est revenu au niveau de départ |
| Comparer `objectif` à `tracee` | l'issue se juge sur `traversees`, à l'arrêt du flux |
| Croire `traversees` cumulé sur la partie | il repart de zéro à chaque manche |
| Poser sans vérifier `tete.entree` | la pose est acceptée, le flux ne passe pas |
| Poser deux fois sans délai | la seconde est refusée `trop tot` |
| Attendre chaque réponse avant d'agir | le flux avance pendant l'attente |
| Omettre `TCP_NODELAY` | jusqu'à 40 ms de latence par message |
