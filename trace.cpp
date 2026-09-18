#include <climits>
#include <algorithm>

#include "trace.h"
#include "ecoulement.h"

Trace::Trace() {
}

void Trace::vider() {
    cases.clear();
    types.fill(tpNone);
    entrees.fill((unsigned char)sHaut);
    rangs.fill(-1);
    graineCalcul = 0;
}

bool Trace::estCalcule() const {
    return !cases.isEmpty();
}

int Trace::longueur() const {
    return cases.size();
}

quint32 Trace::graine() const {
    return graineCalcul;
}

ETypePiece Trace::type(int col, int row) const {
    if(col < 0 || col >= largeur || row < 0 || row >= hauteur) {
        return tpNone;
    }

    return (ETypePiece)types.at(row * largeur + col);
}

int Trace::rang(int col, int row) const {
    if(col < 0 || col >= largeur || row < 0 || row >= hauteur) {
        return -1;
    }

    return rangs.at(row * largeur + col);
}

ESens Trace::entree(int col, int row) const {
    if(col < 0 || col >= largeur || row < 0 || row >= hauteur) {
        return sHaut;
    }

    return (ESens)entrees.at(row * largeur + col);
}

int Trace::garde() const {
    return gardeTenue;
}

int Trace::caseAuRang(int rang) const {
    if(rang < 0 || rang >= cases.size()) {
        return -1;
    }

    return cases.at(rang);
}

bool Trace::convient(const ETypePiece &enMain, const ETypePiece &voulu) {
    if(enMain == voulu) {
        return true;
    }

    return enMain == tpCroix && (voulu == tpHorizontal || voulu == tpVertical);
}

QVector<int> Trace::histogramme() const {
    QVector<int> compte(tpBloque + 1, 0);

    foreach(int idx, cases) {
        compte[types.at(idx)]++;
    }

    return compte;
}

// Le nom d'un coude designe ses COTES OUVERTS, jamais le coin vide -- c'est la
// convention de common.h, et elle rend la table triviale : le type d'une case
// est celui dont les ouvertures sont exactement {entree, sortie}.
ETypePiece Trace::typePour(ESens entree, ESens sortie) {
    if(entree == sortie) {
        // N'arrive pas sur un trace, mais autant ne pas rendre n'importe quoi.
        return tpNone;
    }

    // Cotes opposes : le flux traverse tout droit.
    if((entree == sHaut && sortie == sBas) || (entree == sBas && sortie == sHaut)) {
        return tpVertical;
    }

    if((entree == sGauche && sortie == sDroite) || (entree == sDroite && sortie == sGauche)) {
        return tpHorizontal;
    }

    bool haut = (entree == sHaut || sortie == sHaut);
    bool gauche = (entree == sGauche || sortie == sGauche);

    if(haut) {
        return gauche ? tpCoudeHautGauche : tpCoudeHautDroite;
    }

    return gauche ? tpCoudeBasGauche : tpCoudeBasDroite;
}

// --- la recherche -----------------------------------------------------------
//
// Etat de travail, vivant le temps d'un calculer() et pas plus. Hors de la
// classe : rien de tout ca n'a de sens une fois le trace rendu, et Trace n'a
// aucune raison de le porter.
namespace {

struct Recherche {
    const Game *plateau = nullptr;
    int largeur = 0;
    int hauteur = 0;
    int visee = 0;
    long budget = 0;
    // Une case par octet : 1 quand le chemin courant l'occupe.
    QVector<char> vu;
    // Le chemin courant, et le plus long rencontre.
    QVector<int> chemin;
    QVector<int> meilleur;
    // Pour le flood-fill, reutilise d'un appel a l'autre : le remettre a zero
    // coute 225 octets, l'allouer coute davantage.
    QVector<char> atteint;
    // Combien de fois le chemin courant reclame chaque type. Sert a departager
    // les candidates de meme score de Warnsdorff : voir le commentaire dans
    // etendre(). Tenu a jour par les poses et les retours arriere.
    int parType[tpBloque + 1] = {0};

    // --- la main ---------------------------------------------------------
    //
    // Les pieces connues de la file, rangees par type et par ordre d'arrivee :
    // `arrivees[t]` liste les indices de tirage des pieces de ce type, et
    // `utilises[t]` dit combien le chemin courant en a deja depensees. La plus
    // ancienne encore libre est donc arrivees[t][utilises[t]] -- et c'est elle
    // qu'on prend, ce qui est optimal avec des echeances croissantes.
    //
    // Tenu a jour comme parType : on paie en posant, on rembourse au retour
    // arriere. `contrainte` est le nombre de rangs concernes, zero quand on
    // cherche sans main.
    int contrainte = 0;
    QVector<int> arrivees[tpBloque + 1];
    int utilises[tpBloque + 1] = {0};
    // Echeance du rang k : base + k * pas. A zero, pas d'echeance du tout.
    int base = 0;
    int pas = 0;
};

// L'indice de tirage limite pour le rang k : au-dela, la piece arrive apres le
// flux et ne sert plus a rien.
int echeance(const Recherche &r, int rang) {
    if(r.base <= 0) {
        return INT_MAX;
    }

    return r.base + rang * r.pas;
}

// La plus ancienne piece de ce type encore libre, ou -1.
int prochaine(const Recherche &r, ETypePiece type) {
    if(r.utilises[type] >= r.arrivees[type].size()) {
        return -1;
    }

    return r.arrivees[type].at(r.utilises[type]);
}

// Une case de ce type peut-elle etre payee, et a temps ? Le type exact d'abord,
// la croix ensuite : elle ne sert que les droits, donc la depenser sur un droit
// qu'on avait en propre gacherait le joker.
bool payable(const Recherche &r, ETypePiece type, int rang) {
    int limite = echeance(r, rang);
    int exact = prochaine(r, type);

    if(exact >= 0 && exact < limite) {
        return true;
    }

    if(type != tpHorizontal && type != tpVertical) {
        return false;
    }

    int joker = prochaine(r, tpCroix);

    return joker >= 0 && joker < limite;
}

bool libre(const Recherche &r, int col, int row) {
    if(col < 0 || col >= r.largeur || row < 0 || row >= r.hauteur) {
        return false;
    }

    if(r.plateau->getTypePiece(col, row) != tpNone) {
        return false;
    }

    return !r.vu.at(row * r.largeur + col);
}

int voisinesLibres(const Recherche &r, int col, int row) {
    int n = 0;

    for(int s = 0; s < 4; s++) {
        int vCol, vRow;
        ESens vEntree;

        Ecoulement::voisine(col, row, (ESens)s, vCol, vRow, vEntree);

        if(libre(r, vCol, vRow)) {
            n++;
        }
    }

    return n;
}

// Combien de cases vides sont atteignables depuis (col, row), en s'arretant a
// `maxi`. Meme mesure et meme raison que Bot::tailleRegionVide : on ne veut pas
// explorer toute la poche, seulement savoir s'il y a la place de finir.
int placeAtteignable(Recherche &r, int col, int row, int maxi) {
    r.atteint.fill(0, r.largeur * r.hauteur);

    QVector<int> pile;
    pile.reserve(64);
    pile << row * r.largeur + col;
    r.atteint[row * r.largeur + col] = 1;

    int n = 0;

    while(!pile.isEmpty()) {
        int idx = pile.takeLast();
        n++;

        if(maxi > 0 && n >= maxi) {
            return n;
        }

        int cx = idx % r.largeur;
        int cy = idx / r.largeur;

        for(int s = 0; s < 4; s++) {
            int vCol, vRow;
            ESens vEntree;

            Ecoulement::voisine(cx, cy, (ESens)s, vCol, vRow, vEntree);

            if(!libre(r, vCol, vRow)) {
                continue;
            }

            int vIdx = vRow * r.largeur + vCol;

            if(!r.atteint.at(vIdx)) {
                r.atteint[vIdx] = 1;
                pile << vIdx;
            }
        }
    }

    return n;
}

// Ce que chaque type du trace peut absorber du tirage, en quatorziemes.
//
// Un histogramme PLAT serait le mauvais objectif, et c'est la subtilite : la
// croix traverse tout droit, donc elle se pose partout ou un droit est demande.
// Les sept types tires se repartissent donc sur six types reclames -- quatre
// coudes qui recoivent 1/7 chacun, et deux droits qui se partagent 3/7 (leur
// propre tirage plus celui de la croix). Le trace ideal reclame donc 21,4 % de
// chaque droit et 14,3 % de chaque coude, et non 16,7 % partout.
//
// A quota egal, la comparaison count[a]/quota[a] < count[b]/quota[b] se fait en
// produits croises : pas de division, pas d'arrondi, et l'ordre reste
// parfaitement reproductible -- ce qu'exige le rejeu d'un niveau.
static int quotaDuType(ETypePiece type) {
    return (type == tpHorizontal || type == tpVertical) ? 3 : 2;
}

// Les rangs du trace ou un coude coute trop cher.
//
// Le tirage est uniforme sur sept types, mais un DROIT est servi 3 fois sur 7
// -- le sien, plus la croix qui traverse tout droit -- quand un COUDE ne l'est
// que 1 fois sur 7. Trois fois plus exposee, donc, et l'exposition ne se paie
// pas au meme prix partout : le flux atteint le rang k a t = 22 + k secondes,
// le bot ayant tire 44 + 2k pieces d'ici la. Un trou au rang 1 tue tout de
// suite, un trou au rang 100 a eu deux cents tirages pour se combler.
//
// Mesure du 18 septembre 2026, sur les douze effondrements du bot trace : les
// DOUZE trous fatals sont des coudes, aux rangs 1, 1, 2, 4, 7, 7, 8, 10, 14,
// 14, 15 et 45. D'ou le seuil, qui couvre les onze premiers.
//
// Au-dela on rend la main au quota : le trace entier doit rester equilibre,
// sans quoi la fin -- ou le nombre de types encore reclames s'effondre deja --
// n'aurait plus que des coudes a demander.
#define DROITS_EN_TETE 16

static bool estDroit(ETypePiece type) {
    return type == tpHorizontal || type == tpVertical;
}

// Une candidate, son score de Warnsdorff et le type qu'elle imposerait a la
// case qu'on quitte.
struct Candidate {
    int col;
    int row;
    int score;
    ETypePiece type;
};

// Vrai des que la visee est atteinte : on ne cherche pas mieux, on cherche
// assez. Le chemin courant inclut deja (col, row).
bool etendre(Recherche &r, int col, int row, ESens entree) {
    if(r.chemin.size() > r.meilleur.size()) {
        r.meilleur = r.chemin;
    }

    if(r.chemin.size() >= r.visee) {
        return true;
    }

    if(r.budget <= 0) {
        return false;
    }

    int restant = r.visee - r.chemin.size();
    QVector<Candidate> candidates;

    for(int s = 0; s < 4; s++) {
        int vCol, vRow;
        ESens vEntree;

        Ecoulement::voisine(col, row, (ESens)s, vCol, vRow, vEntree);

        if(!libre(r, vCol, vRow)) {
            continue;
        }

        // On marque la candidate avant de la mesurer : sans ca elle se compte
        // elle-meme comme voisine libre de ses propres voisines.
        int vIdx = vRow * r.largeur + vCol;
        r.vu[vIdx] = 1;

        int score = voisinesLibres(r, vCol, vRow);
        // La place se mesure depuis la candidate, candidate comprise : d'ou le
        // restant - 1 une fois qu'on y aura pose le pied.
        bool assez = (restant <= 1)
                     || placeAtteignable(r, vCol, vRow, restant - 1) >= restant - 1;

        r.vu[vIdx] = 0;

        // Un cul-de-sac termine la marche : on ne l'accepte qu'en derniere
        // case. Sinon on abandonne cette case, ce qui est sans importance --
        // on cherche une longueur, pas une couverture.
        if(score == 0 && restant > 1) {
            continue;
        }

        // Dans les premiers rangs, le type est impose par ce qu'on tient : ces
        // cases-la seront servies avec les pieces de la file, sans rien
        // attendre du tirage.
        ETypePiece impose = Trace::typePour(entree, (ESens)s);

        if((int)r.chemin.size() <= r.contrainte
           && !payable(r, impose, (int)r.chemin.size() - 1)) {
            continue;
        }

        if(!assez) {
            continue;
        }

        candidates.append({ vCol, vRow, score, impose });
    }

    // Warnsdorff d'abord : le moins de voisines libres, parce que c'est ce qui
    // evite de se couper du terrain.
    //
    // A EGALITE, le type le plus en retard sur son quota. Sans ce second
    // critere c'est l'ordre de l'enumeration ESens qui tranchait, et il
    // tranchait toujours dans le meme sens : mesure le 18 septembre 2026, le
    // trace reclamait 34,7 % de vertical pour 7,2 % d'horizontal.
    //
    // Ce n'est pas une coquetterie. Le tirage est uniforme sur sept types ; un
    // type reclame plus souvent qu'il n'arrive devient le goulot du prefixe
    // contigu -- le flux bute sur la premiere case trouee, et c'est toujours
    // celle du type en retard -- pendant que les types sur-servis partent a la
    // defausse. Egaliser ne coute pas une case de plus, un trace ayant la meme
    // longueur quelle que soit sa forme. Voir TRACE.md, §3.
    std::stable_sort(candidates.begin(), candidates.end(),
                     [&r](const Candidate &a, const Candidate &b) {
                         // WARNSDORFF S'INVERSE SUR LA DERNIERE CASE. Partout
                         // ailleurs on prend l'etranglement d'abord, pour ne
                         // pas se couper du terrain. Mais la derniere case du
                         // trace n'a plus de suite a se menager : c'est de la
                         // QU'ON REPART une fois l'objectif acquis, et c'est le
                         // v3 qui construit le bonus a partir d'elle. Finir
                         // dans un cul-de-sac lui coute toute la manche de
                         // rabiot -- 50 points par case qu'il n'ira pas
                         // chercher. On finit donc au large.
                         if(r.visee - (int)r.chemin.size() == 1) {
                             if(a.score != b.score) {
                                 return a.score > b.score;
                             }
                         } else if(a.score != b.score) {
                             return a.score < b.score;
                         }

                         // LES DROITS EN TETE. Avant le quota, et seulement
                         // dans le prefixe expose : voir DROITS_EN_TETE.
                         if((int)r.chemin.size() < DROITS_EN_TETE) {
                             bool aDroit = estDroit(a.type);
                             bool bDroit = estDroit(b.type);

                             if(aDroit != bDroit) {
                                 return aDroit;
                             }
                         }

                         return r.parType[a.type] * quotaDuType(b.type)
                                < r.parType[b.type] * quotaDuType(a.type);
                     });

    foreach(const Candidate &c, candidates) {
        int idx = c.row * r.largeur + c.col;

        // Le paiement de la main, et avec quoi : le type exact tant qu'il en
        // reste, la croix ensuite. On garde le choix sur la pile pour rendre
        // exactement ce qu'on a pris au retour arriere.
        bool sousContrainte = (int)r.chemin.size() <= r.contrainte;
        int exact = sousContrainte ? prochaine(r, c.type) : -1;
        bool avecCroix = sousContrainte
                         && (exact < 0 || exact >= echeance(r, (int)r.chemin.size() - 1));

        if(sousContrainte) {
            r.utilises[avecCroix ? tpCroix : c.type]++;
        }

        r.budget--;
        r.vu[idx] = 1;
        r.chemin.append(idx);
        r.parType[c.type]++;

        int vCol, vRow;
        ESens vEntree;

        Ecoulement::voisine(col, row, (ESens)0, vCol, vRow, vEntree);

        // L'entree de la candidate est le cote reciproque de la sortie prise.
        for(int s = 0; s < 4; s++) {
            Ecoulement::voisine(col, row, (ESens)s, vCol, vRow, vEntree);

            if(vRow * r.largeur + vCol == idx) {
                break;
            }
        }

        if(etendre(r, c.col, c.row, vEntree)) {
            return true;
        }

        r.parType[c.type]--;
        r.chemin.removeLast();
        r.vu[idx] = 0;

        if(sousContrainte) {
            r.utilises[avecCroix ? tpCroix : c.type]--;
        }

        if(r.budget <= 0) {
            return false;
        }
    }

    return false;
}

} // namespace

void Trace::finaliser(ESens premiereEntree, const Game *plateauFinal) {
    // Le plateau sert a juger la sortie de la derniere case ; il n'est retenu
    // que le temps de finaliser.
    ESens entreeCourante = premiereEntree;

    for(int i = 0; i < cases.size(); i++) {
        int idx = cases.at(i);
        int col = idx % largeur;
        int row = idx / largeur;

        // UNE CASE PEUT PORTER DEUX RANGS. `cases` est une suite, pas un
        // ensemble : depuis l'auto-croisement, la meme case y figure deux
        // fois -- une par axe. Le reste de la classe, lui, n'a qu'une valeur
        // par case, et c'est suffisant a condition de choisir la bonne :
        //
        //   - le RANG retenu est le PREMIER. C'est lui qui porte l'echeance,
        //     puisque la piece doit etre posee avant le premier passage ;
        //   - le TYPE devient la CROIX des le second passage. Elle est le seul
        //     type qui traverse les deux axes, donc le seul qui serve les deux
        //     rangs -- et Trace::convient refusera tout autre.
        //
        // Un trace sans croisement ne change pas d'un iota : la condition ne
        // se declenche jamais.
        bool secondPassage = rangs.at(idx) >= 0;

        if(!secondPassage) {
            rangs[idx] = i;
            entrees[idx] = (unsigned char)entreeCourante;
        }

        if(i + 1 >= cases.size()) {
            // DERNIERE CASE : sa sortie n'est pas planifiee, mais elle n'est pas
            // indifferente pour autant. C'est de la que le v3 repart pour
            // construire le bonus une fois l'objectif acquis, et une sortie qui
            // bute sur un mur lui coute toute la manche de rabiot.
            //
            // On l'ouvre donc vers la place : parmi les cotes qui donnent sur
            // une case libre, celui qui en offre le plus. Le droit de son axe
            // ne sert plus que de repli, quand aucun cote n'est libre -- et la,
            // au moins, le flux fuit sans rien couter, l'objectif etant atteint.
            //
            // Le type de cette case n'est paye par aucune garde : la recherche
            // type une case en la QUITTANT, et celle-ci n'est jamais quittee.
            // On peut donc la choisir librement.
            ESens meilleure = sHaut;
            int mieux = -1;

            for(int s = 0; s < 4; s++) {
                int vCol, vRow;
                ESens vEntree;

                Ecoulement::voisine(col, row, (ESens)s, vCol, vRow, vEntree);

                if(vCol < 0 || vCol >= largeur || vRow < 0 || vRow >= hauteur) {
                    continue;
                }

                if((ESens)s == entreeCourante || rangs.at(vRow * largeur + vCol) >= 0) {
                    continue;
                }

                if(plateauFinal == nullptr
                   || plateauFinal->getTypePiece(vCol, vRow) != tpNone) {
                    continue;
                }

                // Combien de voisines libres a son tour : une sortie qui donne
                // sur une case elle-meme enfermee ne vaut guere mieux qu'un mur.
                int place = 0;

                for(int t = 0; t < 4; t++) {
                    int wCol, wRow;
                    ESens wEntree;

                    Ecoulement::voisine(vCol, vRow, (ESens)t, wCol, wRow, wEntree);

                    if(wCol >= 0 && wCol < largeur && wRow >= 0 && wRow < hauteur
                       && plateauFinal->getTypePiece(wCol, wRow) == tpNone
                       && rangs.at(wRow * largeur + wCol) < 0) {
                        place++;
                    }
                }

                if(place > mieux) {
                    mieux = place;
                    meilleure = (ESens)s;
                }
            }

            types[idx] = secondPassage
                         ? tpCroix
                         : (mieux >= 0
                            ? typePour(entreeCourante, meilleure)
                            : ((entreeCourante == sHaut || entreeCourante == sBas)
                               ? tpVertical : tpHorizontal));
            break;
        }

        // La sortie est le cote par lequel on rejoint la case suivante.
        int suivant = cases.at(i + 1);
        ESens sortie = sHaut;

        for(int s = 0; s < 4; s++) {
            int vCol, vRow;
            ESens vEntree;

            Ecoulement::voisine(col, row, (ESens)s, vCol, vRow, vEntree);

            if(vRow * largeur + vCol == suivant) {
                sortie = (ESens)s;
                entreeCourante = vEntree;
                break;
            }
        }

        // La croix l'emporte : elle seule traverse les deux axes. Le type de
        // la premiere passe -- un droit, forcement, puisqu'une croix ne tourne
        // pas -- est donc remplace au second passage.
        types[idx] = secondPassage ? tpCroix
                                   : typePour((ESens)entrees[idx], sortie);
    }
}

// --- la reecriture locale ---------------------------------------------------
//
// Voir Trace::ameliorer pour ce qu'elle fait et pourquoi. Ici, la mecanique.
namespace {

// Cases d'un vivier, au plus. La recherche locale est exhaustive : c'est ce
// nombre qui fixe son prix, et 22 la tient sous la milliseconde.
#define VIVIER_LOCAL 22
// Noeuds par fenetre. Une fenetre qui ne se resout pas la-dedans n'avait rien
// d'evident a rendre.
#define BUDGET_LOCAL 200000
// Passes de reecriture au plus. On s'arrete des qu'une passe entiere ne
// rapporte rien, ce qui arrive bien avant.
#define PASSES_LOCALES 12

ESens oppose(ESens s) {
    switch(s) {
    case sHaut:   return sBas;
    case sBas:    return sHaut;
    case sGauche: return sDroite;
    default:      return sGauche;
    }
}

bool axeVertical(ESens s) {
    return s == sHaut || s == sBas;
}

// Le cote par lequel on passe de `depuis` a `vers`, si elles se touchent.
bool coteVers(int depuis, int vers, int largeur, int hauteur, ESens &cote) {
    for(int s = 0; s < 4; s++) {
        int vc, vr;
        ESens ve;

        Ecoulement::voisine(depuis % largeur, depuis / largeur, (ESens)s, vc, vr, ve);

        if(vc >= 0 && vc < largeur && vr >= 0 && vr < hauteur
           && vr * largeur + vc == vers) {
            cote = (ESens)s;
            return true;
        }
    }

    return false;
}

// L'etat d'une reecriture de fenetre : le vivier, les passes deja faites sur
// chaque case, et la marche courante.
struct Locale {
    int largeur = 0;
    int hauteur = 0;
    QVector<char> dansVivier;
    QVector<char> passes;
    // La premiere passe d'une case, pour savoir si elle peut en accueillir une
    // seconde : il faut qu'elle soit DROITE, et la seconde perpendiculaire.
    QVector<ESens> entree1;
    QVector<ESens> sortie1;

    // Ou la marche doit finir et par ou elle en sort. arrivee < 0 : c'est la
    // queue du trace, elle finit ou elle veut.
    int arrivee = -1;
    ESens sortieFinale = sHaut;

    long budget = 0;
    int visitesMax = 0;
    int croixMax = 0;

    QVector<int> chemin;
    QVector<int> meilleur;
};

// La case accepte-t-elle une traversee de plus par cette entree ?
bool traversable(const Locale &l, int idx, ESens entree) {
    if(idx < 0 || !l.dansVivier.at(idx)) {
        return false;
    }

    if(l.passes.at(idx) == 0) {
        return true;
    }

    if(l.passes.at(idx) >= 2) {
        return false;
    }

    // Seconde passe : la premiere doit etre droite et celle-ci perpendiculaire.
    // La croix ne tourne pas -- Ecoulement::sorties lui rend le sens
    // reciproque de l'entree, c'est sa definition meme.
    if(l.sortie1.at(idx) != oppose(l.entree1.at(idx))) {
        return false;
    }

    return axeVertical(entree) != axeVertical(l.entree1.at(idx));
}

// La sortie est-elle jouable depuis cette entree, sur cette case ?
bool sortieJouable(const Locale &l, int idx, ESens entree, ESens s) {
    if(Trace::typePour(entree, s) == tpNone) {
        return false;
    }

    // Seconde passe : elle doit etre droite.
    return l.passes.at(idx) == 0 || s == oppose(entree);
}

void garderSiMieux(Locale &l, int visites, int croix) {
    if(visites < l.visitesMax) {
        return;
    }

    // A visites egales, le MOINS de croix. Une croix se paie en joker -- elle
    // ne remplace plus un droit -- donc a gain egal on n'en pose pas une de
    // plus. C'est la regle du user : la croix seulement quand il le faut.
    if(visites == l.visitesMax && croix >= l.croixMax) {
        return;
    }

    l.visitesMax = visites;
    l.croixMax = croix;
    l.meilleur = l.chemin;
}

void marcher(Locale &l, int idx, ESens entree, int visites, int croix) {
    if(l.budget-- <= 0) {
        return;
    }

    // Peut-on s'arreter ici ? En queue de trace, partout. Ailleurs il faut
    // etre sur la case de sortie ET pouvoir en sortir du bon cote, sans quoi
    // la suite du trace ne se raccorde plus.
    if(l.arrivee < 0) {
        l.passes[idx]++;
        l.chemin << idx;
        garderSiMieux(l, visites + 1, croix + (l.passes.at(idx) == 2 ? 1 : 0));
        l.chemin.removeLast();
        l.passes[idx]--;
    } else if(idx == l.arrivee && sortieJouable(l, idx, entree, l.sortieFinale)) {
        char avant = l.passes.at(idx);
        ESens e1 = l.entree1.at(idx), s1 = l.sortie1.at(idx);

        l.passes[idx]++;

        if(avant == 0) {
            l.entree1[idx] = entree;
            l.sortie1[idx] = l.sortieFinale;
        }

        l.chemin << idx;
        garderSiMieux(l, visites + 1, croix + (avant == 1 ? 1 : 0));
        l.chemin.removeLast();
        l.passes[idx] = avant;
        l.entree1[idx] = e1;
        l.sortie1[idx] = s1;
    }

    for(int s = 0; s < 4; s++) {
        if(!sortieJouable(l, idx, entree, (ESens)s)) {
            continue;
        }

        int vCol, vRow;
        ESens vEntree;

        Ecoulement::voisine(idx % l.largeur, idx / l.largeur, (ESens)s,
                            vCol, vRow, vEntree);

        if(vCol < 0 || vCol >= l.largeur || vRow < 0 || vRow >= l.hauteur) {
            continue;
        }

        int vIdx = vRow * l.largeur + vCol;
        char avant = l.passes.at(idx);
        ESens e1 = l.entree1.at(idx), s1 = l.sortie1.at(idx);

        // On pose la passe AVANT de tester la voisine : une case qu'on vient
        // de croiser n'est plus disponible pour la suite du meme pas.
        l.passes[idx]++;

        if(avant == 0) {
            l.entree1[idx] = entree;
            l.sortie1[idx] = (ESens)s;
        }

        if(traversable(l, vIdx, vEntree)) {
            l.chemin << idx;
            marcher(l, vIdx, vEntree, visites + 1, croix + (avant == 1 ? 1 : 0));
            l.chemin.removeLast();
        }

        l.passes[idx] = avant;
        l.entree1[idx] = e1;
        l.sortie1[idx] = s1;
    }
}

}   // namespace

void Trace::ameliorer(const Game *plateau, int fenetre) {
    if(cases.size() < 2 || plateau == nullptr) {
        return;
    }

    ESens premiereEntree = (ESens)entrees.at(cases.first());
    QVector<char> surTrace(largeur * hauteur, 0);

    foreach(int idx, cases) {
        surTrace[idx] = 1;
    }

    for(int passe = 0; passe < PASSES_LOCALES; passe++) {
        int gain = 0;

        // ON NE TOUCHE PAS AU PREFIXE GARANTI. Les `gardeTenue` premiers rangs
        // sont ceux dont la recherche a promis qu'ils seraient servis a temps
        // par la file connue, et cette promesse porte sur les TYPES exacts de
        // ces cases-la. Les reecrire les changerait sans refaire la promesse :
        // on garderait un chiffre de garde qui ne garantit plus rien, ce qui
        // est pire que pas de garde du tout.
        //
        // La garde vaut 20 a 100 rangs selon les plateaux, et le trace en fait
        // une centaine : il reste largement de quoi ameliorer derriere.
        for(int debut = gardeTenue; debut + 1 < cases.size(); debut++) {
            int fin = qMin(debut + fenetre - 1, cases.size() - 1);

            if(fin <= debut) {
                break;
            }

            // UNE CROIX NE SE COUPE PAS EN DEUX. Des la seconde passe le trace
            // en contient, et une case croisee apparait a DEUX rangs. Si la
            // fenetre n'en attrape qu'un, la recherche la croit libre et peut
            // la reutiliser -- trois passes sur une case, et le trace ne se
            // joue plus.
            bool coupee = false;

            for(int r = debut; r <= fin && !coupee; r++) {
                int dedans = 0, total = 0;

                for(int k = 0; k < cases.size(); k++) {
                    if(cases.at(k) != cases.at(r)) {
                        continue;
                    }

                    total++;

                    if(k >= debut && k <= fin) {
                        dedans++;
                    }
                }

                coupee = dedans != total;
            }

            if(coupee) {
                continue;
            }

            Locale l;

            l.largeur = largeur;
            l.hauteur = hauteur;
            l.dansVivier.fill(0, largeur * hauteur);
            l.passes.fill(0, largeur * hauteur);
            l.entree1.fill(sHaut, largeur * hauteur);
            l.sortie1.fill(sHaut, largeur * hauteur);

            int taille = 0;

            for(int r = debut; r <= fin; r++) {
                if(!l.dansVivier.at(cases.at(r))) {
                    l.dansVivier[cases.at(r)] = 1;
                    taille++;
                }
            }

            // Plus le terrain libre qui touche la fenetre : c'est la que se
            // logent les croix qui coutent une case, et la couverture qu'on
            // vient chercher.
            for(int r = debut; r <= fin && taille < VIVIER_LOCAL; r++) {
                int idx = cases.at(r);

                for(int s = 0; s < 4 && taille < VIVIER_LOCAL; s++) {
                    int vc, vr;
                    ESens ve;

                    Ecoulement::voisine(idx % largeur, idx / largeur, (ESens)s,
                                        vc, vr, ve);

                    if(vc < 0 || vc >= largeur || vr < 0 || vr >= hauteur) {
                        continue;
                    }

                    int v = vr * largeur + vc;

                    if(l.dansVivier.at(v) || surTrace.at(v)
                       || plateau->getTypePiece(vc, vr) != tpNone) {
                        continue;
                    }

                    l.dansVivier[v] = 1;
                    taille++;
                }
            }

            // Entree dans la fenetre : celle du trace, figee. C'est le cote
            // de la premiere case qui REGARDE la precedente -- le flux entre
            // par la. Au rang 0 il n'y a pas de precedente : c'est le
            // reservoir, et son cote est celui que finaliser a retenu.
            ESens entree = premiereEntree;

            if(debut > 0 && !coteVers(cases.at(debut), cases.at(debut - 1),
                                      largeur, hauteur, entree)) {
                continue;
            }

            // Sortie : vers le rang suivant, figee elle aussi. En queue de
            // trace, pas de contrainte.
            l.arrivee = -1;

            if(fin + 1 < cases.size()) {
                l.arrivee = cases.at(fin);

                if(!coteVers(l.arrivee, cases.at(fin + 1), largeur, hauteur,
                             l.sortieFinale)) {
                    continue;
                }
            }

            l.budget = BUDGET_LOCAL;
            l.visitesMax = fin - debut + 1;
            l.croixMax = 0;

            marcher(l, cases.at(debut), entree, 0, 0);

            if(l.meilleur.isEmpty()) {
                continue;
            }

            gain += l.meilleur.size() - (fin - debut + 1);

            QVector<int> neuf;

            for(int r = 0; r < debut; r++) {
                neuf << cases.at(r);
            }

            neuf += l.meilleur;

            for(int r = fin + 1; r < cases.size(); r++) {
                neuf << cases.at(r);
            }

            cases = neuf;

            foreach(int idx, cases) {
                surTrace[idx] = 1;
            }

            // La fenetre a grandi : on reprend apres elle.
            debut += l.meilleur.size() - 1;
        }

        if(gain == 0) {
            break;
        }
    }

    // Le trace a change : types, entrees et rangs sont a refaire de zero --
    // des cases l'ont quitte, d'autres l'ont rejoint.
    types.fill(tpNone, largeur * hauteur);
    entrees.fill((unsigned char)sHaut, largeur * hauteur);
    rangs.fill(-1, largeur * hauteur);

    finaliser(premiereEntree, plateau);
}

// Une passe de recherche. Rend le meilleur chemin trouve dans `cases` s'il est
// plus long que ce qui s'y trouve deja -- le repli sans contrainte ne doit pas
// pouvoir raccourcir ce que la passe contrainte avait obtenu.
bool Trace::chercher(const Game *plateau, int longueurVisee, long budget,
                     const QVector<ETypePiece> &enMain, int garde,
                     int gestesAvantDepart, int gestesParCase,
                     ESens &premiereEntree) {
    Recherche r;
    r.plateau = plateau;
    r.largeur = largeur;
    r.hauteur = hauteur;
    r.budget = budget;
    r.vu.fill(0, largeur * hauteur);
    r.atteint.fill(0, largeur * hauteur);

    // La main : un compte par type, la croix a part puisqu'elle tient lieu de
    // n'importe quel droit. Le VIVIER est toujours la file entiere ; `garde`
    // ne dit que combien de RANGS sont contraints. Les deux sont distincts, et
    // les confondre ne marche pas : reduire le vivier aux premieres pieces
    // impose l'ordre de la file au trace, alors que le bot sert la case la plus
    // en amont quelle que soit la place de la piece dans la file.
    for(int i = 0; i < enMain.size(); i++) {
        ETypePiece type = enMain.at(i);

        if(type > tpNone && type <= tpBloque) {
            r.arrivees[type] << i;
        }
    }

    r.contrainte = garde;
    r.base = gestesAvantDepart;
    r.pas = gestesParCase;

    // Le reservoir n'a qu'une ouverture, celle que son sens indique, et pas
    // d'entree : c'est la source. Le trace commence donc sur sa voisine.
    int depart = plateau->getIdxDepart();
    int col, row;

    Ecoulement::voisine(depart % largeur, depart / largeur,
                        plateau->getSens(depart % largeur, depart / largeur),
                        col, row, premiereEntree);

    if(!libre(r, col, row)) {
        // Le reservoir debouche sur un mur ou un bloc : rien a planifier.
        return false;
    }

    r.visee = longueurVisee;
    r.vu[row * largeur + col] = 1;
    r.chemin << (row * largeur + col);

    bool atteinte = etendre(r, col, row, premiereEntree);

    // A longueur EGALE le dernier gagne : c'est la passe de confirmation de
    // calculer(), et c'est son trace qu'il faut rendre. Une passe qui echoue
    // est toujours strictement plus courte, elle ne peut donc pas voler la
    // place a une passe qui a atteint la visee.
    if(r.meilleur.size() >= cases.size()) {
        cases = r.meilleur;
    }

    return atteinte;
}

void Trace::calculer(const Game *plateau, int longueurVisee, long budget,
                     const QVector<ETypePiece> &enMain,
                     int gestesAvantDepart, int gestesParCase) {
    largeur = plateau->getLargeur();
    hauteur = plateau->getHauteur();

    cases.clear();
    gardeTenue = 0;
    types.fill(tpNone, largeur * hauteur);
    entrees.fill((unsigned char)sHaut, largeur * hauteur);
    rangs.fill(-1, largeur * hauteur);
    graineCalcul = plateau->getGraine();

    // Viser tout le terrain libre quand on ne demande rien : c'est la seule
    // facon de mesurer ce que le plateau permet.
    if(longueurVisee <= 0) {
        for(int k = 0; k < largeur * hauteur; k++) {
            if(plateau->getTypePiece(k % largeur, k / largeur) == tpNone) {
                longueurVisee++;
            }
        }
    }

    ESens premiereEntree = sHaut;

    // La contrainte est DEGRESSIVE : on demande d'abord que les cinq premiers
    // rangs soient servis par la main, puis quatre, puis trois... et enfin
    // aucun. Exiger les cinq d'un coup echoue le plus souvent, et pour une
    // raison purement geometrique : l'entree du reservoir ne laisse que trois
    // types possibles au rang 0 (le droit de son axe et deux coudes), et chaque
    // rang servi restreint le suivant de la meme facon. Une main de cinq pieces
    // tirees au hasard tombe rarement pile sur une chaine realisable.
    //
    // Trois rangs servis d'avance valent tout de meme mieux que zero, et
    // l'essai qui echoue ne coute rien -- il meurt en un noeud, faute de
    // candidate payable. On descend donc jusqu'a ce que ca passe.
    // On cherche la plus GRANDE garde qui passe, par dichotomie.
    //
    // La faisabilite est monotone : toute solution qui sert les k+1 premiers
    // rangs sert les k premiers, on ne fait que retirer une contrainte. Il n'y
    // a donc rien a balayer -- et ca compte depuis que la main peut valoir
    // quarante pieces au rejeu, ou la descente une par une paierait quarante
    // recherches au lieu de six.
    //
    // Les passes contraintes ont un budget REDUIT, la passe libre garde le
    // sien. Sans ca le pire cas se paie six recherches pleines -- mesure au
    // niveau 46 : 6,9 s dans un seul battement, soit un tiers du delai de
    // depart, et l'ecran fige d'autant. Une passe contrainte qui a besoin de
    // plus que ce quart-la n'a de toute facon pas trouve un debut facile.
    int bas = 0;
    // On ne garde jamais plus de rangs que le trace n'en aura.
    int haut = qMin(enMain.size(), longueurVisee);

    while(bas < haut) {
        int milieu = (bas + haut + 1) / 2;

        if(chercher(plateau, longueurVisee, budget / 4, enMain, milieu,
                    gestesAvantDepart, gestesParCase, premiereEntree)) {
            bas = milieu;
        } else {
            haut = milieu - 1;
        }
    }

    // La passe qui fait foi. La dichotomie a pu s'arreter sur un essai qui
    // n'etait pas le dernier, et `cases` garde le plus long rencontre : on
    // rejoue la garde retenue pour que le trace rendu soit bien celui-la.
    chercher(plateau, longueurVisee, bas > 0 ? budget / 4 : budget,
             enMain, bas, gestesAvantDepart, gestesParCase, premiereEntree);
    gardeTenue = bas;

    finaliser(premiereEntree, plateau);
}
