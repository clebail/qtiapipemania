#include <QtGlobal>
#include <QtDebug>
#include <algorithm>
#include "bot.h"

// Plan de defausse : le reseau que le bot vise quand il jette une piece
// (Bot::defausser). Ce n'est plus un pavage fige mais un reseau recalcule a
// chaque manche, sur le plateau de cette manche-la, cases bloquees comprises.
//
// Ce qu'on lui demande, et ce qu'il garantit :
//
//   - il est CLOS. Toute ouverture d'une case du plan est rendue par sa
//     voisine ; aucune ne bute sur un bloc, sur une piece etrangere ni sur le
//     bord de la grille. Une piece defaussee a sa place ne peut donc pas etre
//     un rebut mort-ne : elle raccordera ses voisines des qu'elles seront la.
//     C'est la seule propriete qui compte vraiment, et c'est celle qu'un
//     pavage fige perdait des que le plateau se trouait.
//   - il couvre le plus de cases qu'il peut. Celles qu'il abandonne ne sont
//     pas des bouts morts : le plan n'y dit simplement rien (tpNone), et la
//     defausse les traite a part.
//
// Pourquoi le recalcul. Un pavage 15x15 clos existe et il est joli (planche 02
// : un circuit unique de 258 traversees, 33 croix, ecart de 2 entre le type le
// plus et le moins servi, ce qui est l'optimum demontre). Mais pose sur un
// plateau troue, il perd ses pieces sur les cases bloquees et laisse derriere
// lui une cinquantaine de bouts morts au plafond de 24 blocs. Recalculer donne
// 5 a 10 cases silencieuses au lieu de cela.
//
// Ce que le recalcul ne cherche PAS : l'optimum. L'objectif d'une manche
// plafonne a 60 traversees (LONGUEUR_MAX), et le plan en couvre environ 195 --
// trois fois plus que le trace pourra jamais consommer. Quelques cases perdues
// de plus ne se voient pas au jeu, alors qu'une recherche exhaustive se paierait
// en secondes. La borne PLAN_NOEUDS_MAX arrete donc la recherche bien avant.
#define PLAN_NOEUDS_MAX 1500000

// Un bit par ESens. L'enum vaut sHaut=0, sBas=1, sGauche=2, sDroite=3.
#define BIT(s) ((unsigned char)(1 << (int)(s)))

static const SDelta deltaSens[4] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
static const ESens opposeSens[4] = { sBas, sHaut, sDroite, sGauche };

// Le type de piece qui porte exactement ces ouvertures, tpNone pour aucune.
// Toute paire de directions est une piece du jeu -- les deux droits et les
// quatre coudes couvrent les six paires --, ce qui est ce qui rend le probleme
// traitable : la contrainte est "2 ou 4 ouvertures", pas "une forme permise".
static ETypePiece typeDesOuvertures(unsigned char masque) {
    switch(masque) {
    case BIT(sGauche) | BIT(sDroite): return tpHorizontal;
    case BIT(sHaut) | BIT(sBas):      return tpVertical;
    case BIT(sHaut) | BIT(sGauche):   return tpCoudeHautGauche;
    case BIT(sHaut) | BIT(sDroite):   return tpCoudeHautDroite;
    case BIT(sBas) | BIT(sGauche):    return tpCoudeBasGauche;
    case BIT(sBas) | BIT(sDroite):    return tpCoudeBasDroite;
    case 0x0F:                        return tpCroix;
    }

    return tpNone;
}

// Recherche du reseau clos d'une composante connexe de cases libres. Ligne par
// ligne : quand on decide une case, ses voisines gauche et haut le sont deja,
// donc elles imposent ou interdisent leurs cotes. Le reste se choisit, et
// l'abandon de la case est la derniere option -- on couvre d'abord.
class ChercheurPlan {
public:
    ChercheurPlan(int largeur, int hauteur, const QVector<unsigned char> &mort)
        : largeur(largeur), hauteur(hauteur), mort(mort) {
        ouv.fill(0, largeur * hauteur);
        rang.fill(-1, largeur * hauteur);
    }

    // Resout une composante et ecrit ses ouvertures dans `resultat`. Renvoie le
    // nombre de cases abandonnees.
    int resoudre(const QVector<int> &cases, QVector<unsigned char> &resultat) {
        ordre = cases;
        for(int i = 0; i < ordre.size(); i++) {
            rang[ordre[i]] = i;
        }

        noeuds = 0;

        // Un socle avant de chercher : on pave la composante de carres 2x2
        // disjoints. Quatre cases libres en carre forment toujours un circuit
        // ferme valide, et deux carres disjoints ne partagent aucune arete --
        // c'est donc une solution, immediatement, sans recherche.
        //
        // Elle n'est pas optimale (elle laisse les bandes de largeur impaire),
        // mais elle rend la suite sure : la recherche part avec une borne, ne
        // retient que ce qui fait strictement mieux, et le plan ne peut plus
        // se retrouver vide parce que le budget a manque. C'est ce qui arrivait
        // avant : sur les plateaux difficiles, la recherche epuisait ses noeuds
        // sans jamais atteindre une feuille, et toute la composante restait
        // muette alors qu'un pavage trivial couvrait les neuf dixiemes.
        socle();

        descendre(0, 0);

        for(int i = 0; i < ordre.size(); i++) {
            rang[ordre[i]] = -1;
        }

        for(int i = 0; i < ordre.size(); i++) {
            resultat[ordre[i]] = meilleures[i];
        }

        return meilleur;
    }

private:
    // Pavage par carres 2x2 disjoints : la solution de repli, toujours valide.
    void socle() {
        QVector<unsigned char> pris(largeur * hauteur, 0);
        meilleures.fill(0, ordre.size());
        meilleur = ordre.size();

        for(int i = 0; i < ordre.size(); i++) {
            int idx = ordre[i];
            int x = idx % largeur;
            int y = idx / largeur;

            if(pris[idx] || x + 1 >= largeur || y + 1 >= hauteur) {
                continue;
            }

            int coins[4] = { idx, idx + 1, idx + largeur, idx + largeur + 1 };
            bool bon = true;

            for(int k = 0; k < 4; k++) {
                if(rang[coins[k]] < 0 || pris[coins[k]]) {
                    bon = false;
                }
            }

            if(!bon) {
                continue;
            }

            // Le carre, dans le sens de lecture : bas-droite, bas-gauche,
            // haut-droite, haut-gauche.
            static const unsigned char forme[4] = {
                BIT(sBas) | BIT(sDroite), BIT(sBas) | BIT(sGauche),
                BIT(sHaut) | BIT(sDroite), BIT(sHaut) | BIT(sGauche)
            };

            for(int k = 0; k < 4; k++) {
                pris[coins[k]] = 1;
                meilleures[rang[coins[k]]] = forme[k];
                meilleur--;
            }
        }
    }

    void descendre(int i, int abandonnees) {
        if(++noeuds > PLAN_NOEUDS_MAX || abandonnees >= meilleur) {
            return;
        }

        if(i == ordre.size()) {
            meilleur = abandonnees;
            meilleures.resize(ordre.size());
            for(int k = 0; k < ordre.size(); k++) {
                meilleures[k] = ouv[ordre[k]];
            }
            return;
        }

        int idx = ordre[i];
        int x = idx % largeur;
        int y = idx / largeur;

        unsigned char impose = 0;
        unsigned char interdit = 0;

        for(int s = 0; s < 4; s++) {
            int nx = x + deltaSens[s].dx;
            int ny = y + deltaSens[s].dy;

            if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
                interdit |= BIT((ESens)s);
                continue;
            }

            int nidx = ny * largeur + nx;

            if(mort[nidx] || rang[nidx] < 0) {
                // Bloc, case epluchee, ou case d'une autre composante : mur.
                interdit |= BIT((ESens)s);
            } else if(rang[nidx] < i) {
                // Voisine deja decidee : elle impose son cote, ou l'interdit.
                if(ouv[nidx] & BIT(opposeSens[s])) {
                    impose |= BIT((ESens)s);
                } else {
                    interdit |= BIT((ESens)s);
                }
            }
        }

        if(impose & interdit) {
            return;
        }

        // Les seuls cotes encore libres sont ceux vers les cases non decidees :
        // droite et bas, l'ordre etant ligne par ligne.
        unsigned char ouverts = ~(impose | interdit) & (BIT(sDroite) | BIT(sBas));

        // On essaie d'abord de couvrir, et parmi les couvertures le MOINS
        // d'ouvertures d'abord. La croix passe en dernier : elle exige ses
        // quatre voisines, donc elle contraint tout ce qui suit, et l'essayer
        // en premier envoyait la recherche dans des branches qu'elle ne
        // refermait jamais dans son budget. L'abandon vient apres tout.
        for(int k = 0; k <= 2; k++) {
            for(int m = 0; m < 4; m++) {
                unsigned char ajout = (unsigned char)(((m & 1) ? BIT(sDroite) : 0)
                                                      | ((m & 2) ? BIT(sBas) : 0));

                if((ajout & ~ouverts) || compterBits(ajout) != k) {
                    continue;
                }

                unsigned char masque = impose | ajout;
                int n = compterBits(masque);

                if(n != 2 && !(n == 4 && interdit == 0)) {
                    continue;
                }

                ouv[idx] = masque;
                descendre(i + 1, abandonnees);
                ouv[idx] = 0;

                if(meilleur == 0) {
                    return;
                }
            }
        }

        // Abandonner la case : impossible si une voisine s'ouvre deja sur elle.
        if(impose == 0) {
            ouv[idx] = 0;
            descendre(i + 1, abandonnees + 1);
        }
    }

    static int compterBits(unsigned char m) {
        int n = 0;
        while(m) { n += m & 1; m >>= 1; }
        return n;
    }

    int largeur;
    int hauteur;
    QVector<unsigned char> mort;
    QVector<unsigned char> ouv;
    QVector<int> rang;
    QVector<int> ordre;
    QVector<unsigned char> meilleures;
    int noeuds = 0;
    int meilleur = 0;
};

// Longueur de chaque circuit du reseau, et nombre de cases non couvertes. Une
// croix porte deux passages independants, d'ou l'etat (case, axe).
static void mesurerCircuits(const QVector<unsigned char> &ouv, int largeur, int hauteur,
                            QVector<int> &longueurs, int &nonCouvertes) {
    longueurs.clear();
    nonCouvertes = 0;

    QVector<unsigned char> vu(largeur * hauteur * 2, 0);

    for(int idx = 0; idx < largeur * hauteur; idx++) {
        if(ouv[idx] == 0) {
            nonCouvertes++;
            continue;
        }

        bool croix = ouv[idx] == 0x0F;

        for(int axe = 0; axe < (croix ? 2 : 1); axe++) {
            if(vu[idx * 2 + axe]) {
                continue;
            }

            int n = 0;
            QVector<int> pile;
            pile << idx * 2 + axe;

            while(!pile.isEmpty()) {
                int etat = pile.takeLast();

                if(vu[etat]) {
                    continue;
                }

                vu[etat] = 1;
                n++;

                int cidx = etat / 2;
                int ca = etat % 2;
                int cx = cidx % largeur;
                int cy = cidx / largeur;
                bool ccroix = ouv[cidx] == 0x0F;

                for(int s = 0; s < 4; s++) {
                    if(!(ouv[cidx] & BIT((ESens)s))) {
                        continue;
                    }

                    // Dans une croix, le flux va tout droit : chaque axe est
                    // une conduite a part.
                    bool horiz = (s == (int)sGauche || s == (int)sDroite);

                    if(ccroix && (ca == 0) != horiz) {
                        continue;
                    }

                    int nx = cx + deltaSens[s].dx;
                    int ny = cy + deltaSens[s].dy;
                    int nidx = ny * largeur + nx;
                    int na = 0;

                    if(ouv[nidx] == 0x0F) {
                        na = horiz ? 0 : 1;
                    }

                    pile << nidx * 2 + na;
                }
            }

            longueurs << n;
        }
    }
}

// Ce qu'on cherche a faire baisser : les circuits trop courts pour etre surs,
// puis les cases que le plan ne couvre pas. Un circuit plus long que le seuil
// ne coute rien -- au-dela, allonger encore n'apporte plus de securite.
#define PLAN_LONGUEUR_SURE 40

static int coutReseau(const QVector<unsigned char> &ouv, int largeur, int hauteur) {
    QVector<int> longueurs;
    int nonCouvertes;

    mesurerCircuits(ouv, largeur, hauteur, longueurs, nonCouvertes);

    int cout = nonCouvertes;

    foreach(int n, longueurs) {
        cout += qMax(0, PLAN_LONGUEUR_SURE - n);
    }

    return cout;
}

// En deca de cette longueur, un circuit est completable dans une manche : la
// defausse finit par en poser toutes les cases, et il devient un piege. Au-dela,
// le trace n'aura jamais le temps de le refermer.
#define PLAN_LONGUEUR_MINIMALE 16

// Dernier filet, apres la fusion. Un circuit encore trop court est efface --
// ses cases retournent au silence. C'est une perte de couverture minuscule
// contre une mort certaine : le trace qui rejoint un anneau complet le suit,
// revient a son entree, et la manche s'arrete sans qu'aucune pose n'ait pu
// etre refusee. Le bot ne peut pas s'en proteger tout seul, parce que ce n'est
// pas lui qui referme l'anneau : c'est la defausse, apres coup.
static void effacerCircuitsCourts(QVector<unsigned char> &ouv, int largeur, int hauteur) {
    QVector<int> longueurs;
    int nonCouvertes;

    // On recommence tant qu'on efface : effacer un circuit n'en raccourcit
    // aucun autre, mais la mesure est simple et le plateau est petit.
    for(;;) {
        mesurerCircuits(ouv, largeur, hauteur, longueurs, nonCouvertes);

        bool efface = false;
        QVector<unsigned char> vu(largeur * hauteur * 2, 0);

        for(int idx = 0; idx < largeur * hauteur && !efface; idx++) {
            if(ouv[idx] == 0) {
                continue;
            }

            // On parcourt le circuit qui passe par cette case, et on l'efface
            // s'il est trop court.
            QVector<int> cases;
            QVector<int> pile;
            pile << idx * 2;
            QVector<unsigned char> marque(largeur * hauteur * 2, 0);
            int n = 0;

            while(!pile.isEmpty()) {
                int etat = pile.takeLast();

                if(marque[etat]) {
                    continue;
                }

                marque[etat] = 1;
                n++;

                int cidx = etat / 2;
                int ca = etat % 2;
                int cx = cidx % largeur;
                int cy = cidx / largeur;
                bool ccroix = ouv[cidx] == 0x0F;

                if(!cases.contains(cidx)) {
                    cases << cidx;
                }

                for(int s = 0; s < 4; s++) {
                    if(!(ouv[cidx] & BIT((ESens)s))) {
                        continue;
                    }

                    bool horiz = (s == (int)sGauche || s == (int)sDroite);

                    if(ccroix && (ca == 0) != horiz) {
                        continue;
                    }

                    int nx = cx + deltaSens[s].dx;
                    int ny = cy + deltaSens[s].dy;
                    int nidx = ny * largeur + nx;

                    pile << nidx * 2 + (ouv[nidx] == 0x0F ? (horiz ? 0 : 1) : 0);
                }
            }

            if(n < PLAN_LONGUEUR_MINIMALE) {
                foreach(int k, cases) {
                    ouv[k] = 0;
                }

                efface = true;
            }
        }

        if(!efface) {
            return;
        }
    }
}

void Bot::fusionnerCircuits(QVector<unsigned char> &ouv, const QVector<unsigned char> &mort) {
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();
    int cout = coutReseau(ouv, largeur, hauteur);

    // Descente simple : on ne garde une bascule que si elle ameliore. Bornee
    // par securite, mais elle converge bien avant.
    for(int passe = 0; passe < 40 && cout > 0; passe++) {
        bool progres = false;

        for(int y = 0; y + 1 < hauteur; y++) {
            for(int x = 0; x + 1 < largeur; x++) {
                int coins[4] = { y * largeur + x, y * largeur + x + 1,
                                 (y + 1) * largeur + x, (y + 1) * largeur + x + 1 };

                if(mort[coins[0]] || mort[coins[1]] || mort[coins[2]] || mort[coins[3]]) {
                    continue;
                }

                // Les quatre aretes du carre, vues de chacun de ses coins.
                static const unsigned char bascule[4] = {
                    BIT(sDroite) | BIT(sBas), BIT(sGauche) | BIT(sBas),
                    BIT(sDroite) | BIT(sHaut), BIT(sGauche) | BIT(sHaut)
                };

                for(int k = 0; k < 4; k++) {
                    ouv[coins[k]] ^= bascule[k];
                }

                int neuf = coutReseau(ouv, largeur, hauteur);

                if(neuf < cout) {
                    cout = neuf;
                    progres = true;
                } else {
                    for(int k = 0; k < 4; k++) {
                        ouv[coins[k]] ^= bascule[k];
                    }
                }
            }
        }

        if(!progres) {
            break;
        }
    }

    effacerCircuitsCourts(ouv, largeur, hauteur);
}

void Bot::construirePlan() {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();
    int taille = largeur * hauteur;

    plan.fill(tpNone, taille);

    // 1. Epluchage. Une case libre qui n'a plus que zero ou une voisine vivante
    // ne peut pas porter deux ouvertures : elle est incouvrable, quoi qu'on
    // fasse. La retirer en decouvre d'autres, d'ou l'iteration. C'est ce qui
    // enleve a la recherche ses branches les plus profondes et les plus steriles.
    QVector<unsigned char> mort(taille, 0);

    for(int i = 0; i < taille; i++) {
        if(plateau->getTypePiece(i % largeur, i / largeur) == tpBloque) {
            mort[i] = 1;
        }
    }

    bool change = true;

    while(change) {
        change = false;

        for(int y = 0; y < hauteur; y++) {
            for(int x = 0; x < largeur; x++) {
                if(mort[y * largeur + x]) {
                    continue;
                }

                int vivantes = 0;

                for(int s = 0; s < 4; s++) {
                    int nx = x + deltaSens[s].dx;
                    int ny = y + deltaSens[s].dy;

                    if(nx >= 0 && nx < largeur && ny >= 0 && ny < hauteur
                       && !mort[ny * largeur + nx]) {
                        vivantes++;
                    }
                }

                if(vivantes < 2) {
                    mort[y * largeur + x] = 1;
                    change = true;
                }
            }
        }
    }

    // 2. Composantes connexes. Les blocs decoupent souvent le plateau en
    // regions independantes, et les resoudre separement change l'exposant :
    // c'est le meme travail, mais sur des grilles bien plus petites.
    QVector<unsigned char> ouvertures(taille, 0);
    QVector<unsigned char> vue(taille, 0);
    ChercheurPlan chercheur(largeur, hauteur, mort);

    for(int depart = 0; depart < taille; depart++) {
        if(mort[depart] || vue[depart]) {
            continue;
        }

        QVector<int> pile;
        QVector<int> composante;

        pile << depart;
        vue[depart] = 1;

        while(!pile.isEmpty()) {
            int idx = pile.takeLast();
            composante << idx;

            int x = idx % largeur;
            int y = idx / largeur;

            for(int s = 0; s < 4; s++) {
                int nx = x + deltaSens[s].dx;
                int ny = y + deltaSens[s].dy;

                if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
                    continue;
                }

                int nidx = ny * largeur + nx;

                if(!mort[nidx] && !vue[nidx]) {
                    vue[nidx] = 1;
                    pile << nidx;
                }
            }
        }

        // Moins de quatre cases ne tiennent aucun circuit : le plus court est
        // le carre. Inutile de chercher.
        if(composante.size() < 4) {
            continue;
        }

        std::sort(composante.begin(), composante.end());
        chercheur.resoudre(composante, ouvertures);
    }

    // 3. Fusion des circuits courts.
    //
    // Un circuit du plan devient un piege le jour ou il est COMPLET : le trace
    // y entre, Bot::tete() l'absorbe case apres case, le suit, et revient a son
    // point d'entree. Circuit referme, plus de tete, manche perdue -- et le bot
    // ne peut rien y refuser, puisqu'il ne pose rien, il se contente de courir
    // dans ce que la defausse a bati.
    //
    // Ce qui protege n'est donc pas la cloture, c'est la LONGUEUR. Un anneau de
    // quatre cases se complete en quatre defausses ; un circuit de deux cents ne
    // se complete jamais dans une manche. Le socle 2x2, qui sauvait la
    // couverture, fabriquait exactement le piege le plus court possible.
    //
    // La fusion se fait par bascule de carres unitaires : inverser la presence
    // des quatre aretes d'un carre change chaque coin de -2, 0 ou +2 ouvertures,
    // donc ne sort jamais de {0, 2, 4} -- l'operation est toujours licite. Elle
    // recolle deux circuits voisins en un seul, et peut au passage couvrir une
    // case qui ne l'etait pas.
    fusionnerCircuits(ouvertures, mort);

    // 4. Les ouvertures deviennent des types de pieces.
    int couvertes = 0;

    for(int i = 0; i < taille; i++) {
        plan[i] = typeDesOuvertures(ouvertures[i]);

        if(plan[i] != tpNone) {
            couvertes++;
        }
    }

    marquerObligations();

    qDebug() << "plan de defausse :" << couvertes << "cases couvertes sur"
             << taille - nbBloquees() << "libres";
}

// Une issue ne compte que si le trace peut s'en servir. ISSUE_MINIMALE est le
// nombre de traversees qu'elle doit encore offrir : a 1 on se contenterait de
// "ce n'est pas un mur", et on retomberait sur meneALaMort, qui ne regarde
// qu'une case en avant et prend une poche d'une case pour une sortie.
//
// Le cas qui a fait relever ce seuil : une case libre cernee de trois blocs.
// meneALaMort la voit vide et declare la sortie vivante ; le flux y entre et
// meurt au coup suivant. Trois cases suffisent a distinguer une vraie issue
// d'un cul-de-sac, et rester bas evite d'inventer des obligations la ou le
// terrain est seulement etroit.
#define ISSUE_MINIMALE 3

// De quoi voir en un entier que le plateau a bouge. Le compte des cases
// occupees ne suffirait pas : un remplacement change le type sans changer le
// compte, et il rouvre ou referme des issues comme une pose.
quint32 Bot::signaturePlateau() const {
    quint32 h = 2166136261u;

    for(int i = 0; i < p->getLargeur() * p->getHauteur(); i++) {
        h ^= (quint32)p->plateau()->getTypePiece(i % p->getLargeur(), i / p->getLargeur());
        h *= 16777619u;
    }

    return h;
}

bool Bot::memeRoutage(const ETypePiece& pose, const ETypePiece& voulu) {
    if(pose == voulu) {
        return true;
    }

    // Le reservoir n'est pas un tuyau : ses ouvertures dependent de son sens,
    // et rien ne le remplace.
    if(pose == tpReservoir || voulu == tpReservoir) {
        return false;
    }

    foreach(ESens entree, Ecoulement::ouvertures(voulu, sHaut)) {
        // sorties() suppose la compatibilite acquise : il faut d'abord que
        // `pose` sache seulement recevoir ce flux.
        if(!Ecoulement::piecesCompatibles(entree).contains(pose)) {
            return false;
        }

        QVector<ESens> a = Ecoulement::sorties(pose, sHaut, entree);
        QVector<ESens> b = Ecoulement::sorties(voulu, sHaut, entree);

        if(a.isEmpty() || b.isEmpty() || a.first() != b.first()) {
            return false;
        }
    }

    return true;
}

bool Bot::coteAlimentable(int col, int row, ESens cote) const {
    Game *plateau = p->plateau();
    int nx, ny;
    ESens ne;

    Ecoulement::voisine(col, row, cote, nx, ny, ne);

    if(nx < 0 || nx >= plateau->getLargeur() || ny < 0 || ny >= plateau->getHauteur()) {
        return false;
    }

    ETypePiece t = plateau->getTypePiece(nx, ny);

    return t != tpBloque && t != tpBombe;
}

void Bot::marquerMortes() {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();
    int n = largeur * hauteur;

    mortes.fill(0, n);

    static const ESens cotes[] = {sHaut, sBas, sGauche, sDroite};

    // --- socle : les etats ou le flux ne peut meme pas entrer --------------
    //
    // Un bloc ou une bombe n'accueille rien. Une piece deja posee n'accueille
    // que par ses ouvertures : entrer par un cote ferme n'arrivera jamais, et
    // declarer cet etat mort est ce qui permet au point fixe de mordre -- c'est
    // par la que l'impasse remonte.
    for(int i = 0; i < n; i++) {
        ETypePiece t = plateau->getTypePiece(i % largeur, i / largeur);

        if(t == tpNone) {
            continue;
        }

        if(t == tpBloque || t == tpBombe) {
            mortes[i] = 0x0f;
            continue;
        }

        QVector<ESens> ouv = Ecoulement::ouvertures(t, plateau->getSens(i % largeur,
                                                                       i / largeur));

        for(int c = 0; c < 4; c++) {
            if(!ouv.contains(cotes[c])) {
                mortes[i] |= (unsigned char)(1 << c);
            }
        }
    }

    // --- iteration jusqu'a stabilite ---------------------------------------
    //
    // On part de "tout vivant" et on ne tue que ce qu'on demontre : un etat
    // meurt quand TOUTES ses sorties sont impossibles ou deja mortes. Le point
    // fixe est donc le plus petit, et il ne peut pas inventer une mort -- au
    // pire il en rate une, jamais l'inverse.
    for(bool change = true; change; ) {
        change = false;

        for(int i = 0; i < n; i++) {
            int x = i % largeur;
            int y = i / largeur;
            ETypePiece pose = plateau->getTypePiece(x, y);

            for(int c = 0; c < 4; c++) {
                unsigned char bit = (unsigned char)(1 << c);

                if(mortes.at(i) & bit) {
                    continue;
                }

                ESens entree = cotes[c];

                // Une case libre accepte toutes les pieces compatibles ; une
                // case posee n'a que la sienne, son routage est deja decide.
                QVector<ETypePiece> candidats;

                if(pose == tpNone) {
                    candidats = Ecoulement::piecesCompatibles(entree);
                } else {
                    candidats << pose;
                }

                bool vivant = false;

                foreach(ETypePiece type, candidats) {
                    ESens sens = pose == tpNone ? sHaut : plateau->getSens(x, y);
                    QVector<ESens> sortie = Ecoulement::sorties(type, sens, entree);

                    if(sortie.isEmpty()) {
                        continue;
                    }

                    int nx, ny;
                    ESens ne;

                    Ecoulement::voisine(x, y, sortie.first(), nx, ny, ne);

                    // Hors grille : le tuyau fuit, c'est une fin.
                    if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
                        continue;
                    }

                    if(!etatMort(nx, ny, ne)) {
                        vivant = true;
                        break;
                    }
                }

                if(!vivant) {
                    mortes[i] |= bit;
                    change = true;
                }
            }
        }
    }
}

void Bot::setCalculerMortes(bool calculer) {
    if(calculerMortes == calculer) {
        return;
    }

    calculerMortes = calculer;

    // Le marquage ne se refait qu'au changement de plateau : sans ca, allumer
    // l'affichage en cours de manche ne montrerait rien avant la pose suivante.
    if(calculerMortes) {
        marquerMortes();
    } else {
        mortes.clear();
    }
}

bool Bot::etatMort(int col, int row, ESens entree) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return true;
    }

    if(mortes.size() != p->getLargeur() * p->getHauteur()) {
        return false;
    }

    return (mortes.at(row * p->getLargeur() + col) & (1 << (int)entree)) != 0;
}

bool Bot::caseMorte(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return false;
    }

    if(mortes.size() != p->getLargeur() * p->getHauteur()) {
        return false;
    }

    // Les blocs et les bombes ont les quatre bits, mais ce ne sont pas des
    // cases perdues : elles n'ont jamais ete a nous.
    ETypePiece t = p->plateau()->getTypePiece(col, row);

    if(t != tpNone) {
        return false;
    }

    return mortes.at(row * p->getLargeur() + col) == 0x0f;
}

void Bot::propagerDepuisTete(int besoin) {
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    entreeTete.fill(0, largeur * hauteur);

    int cx, cy;
    ESens ce;

    // Pas de tete : le trace fuit hors grille ou bute sur une piece qui ne
    // presente rien en face. Rien a propager, on retombe sur le voisinage pose.
    if(!p->ecoulement()->tete(cx, cy, ce)) {
        return;
    }

    // La tete elle-meme : son entree est celle du tuyau qui l'alimente, connue
    // et definitive. C'est le premier maillon.
    entreeTete[cy * largeur + cx] = 1 + (unsigned char)ce;

    for(int pas = 0; pas < largeur * hauteur; pas++) {
        QVector<ESens> issues;

        foreach(ETypePiece type, Ecoulement::piecesCompatibles(ce)) {
            QVector<ESens> sortie = Ecoulement::sorties(type, sHaut, ce);

            if(sortie.isEmpty()) {
                continue;
            }

            // Meme raison qu'au marquage : la mort ne remplace pas le seuil.
            if(espaceApres(type, cx, cy, ce, besoin) < besoin) {
                continue;
            }

            if(!issues.contains(sortie.first())) {
                issues << sortie.first();

                // Meme raccourci qu'au marquage : deux directions suffisent a
                // conclure, les espaceApres suivants ne serviraient a rien.
                if(issues.size() > 1) {
                    break;
                }
            }
        }

        // Deux issues : le couloir s'arrete la, la suite redevient un choix.
        // Zero : la case est morte, il n'y a plus de suite du tout.
        if(issues.size() != 1) {
            return;
        }

        int nx, ny;
        ESens ne;

        Ecoulement::voisine(cx, cy, issues.first(), nx, ny, ne);

        if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
            return;
        }

        // Une case deja pleine ne se marque pas : la question de ce qu'il faut
        // y mettre n'a plus d'objet, et le couloir s'arrete a ce qui est libre.
        if(p->plateau()->getTypePiece(nx, ny) != tpNone) {
            return;
        }

        // Deja vue : une croix peut refermer le couloir sur lui-meme.
        if(entreeTete.at(ny * largeur + nx) != 0) {
            return;
        }

        entreeTete[ny * largeur + nx] = 1 + (unsigned char)ne;

        cx = nx;
        cy = ny;
        ce = ne;
    }
}

QVector<ESens> Bot::entreesDecidees(int col, int row) const {
    Game *plateau = p->plateau();
    QVector<ESens> decidees;

    // La propagation avant l'emporte : elle dit par ou le flux ARRIVERA, pas
    // seulement par ou il pourrait arriver. Une voisine posee qui s'ouvre sur
    // nous n'est qu'une porte ; le couloir issu de la tete est un itineraire.
    if(entreeTete.size() == plateau->getSize()) {
        unsigned char e = entreeTete.at(row * plateau->getLargeur() + col);

        if(e != 0) {
            decidees << (ESens)(e - 1);
            return decidees;
        }
    }

    static const ESens cotes[] = {sHaut, sBas, sGauche, sDroite};

    for(int i = 0; i < 4; i++) {
        if(!coteAlimentable(col, row, cotes[i])) {
            continue;
        }

        int nx, ny;
        ESens ne;

        Ecoulement::voisine(col, row, cotes[i], nx, ny, ne);

        ETypePiece t = plateau->getTypePiece(nx, ny);

        if(t == tpNone) {
            continue;
        }

        // `ne` est le cote de la voisine tourne vers nous. Elle ne nous
        // alimente que si elle s'ouvre par la : une voisine posee qui nous
        // tourne le dos ferme ce cote au lieu de le decider.
        if(Ecoulement::ouvertures(t, plateau->getSens(nx, ny)).contains(ne)) {
            decidees << cotes[i];
        }
    }

    return decidees;
}

void Bot::marquerObligations() {
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    // Le seuil de l'issue vivante, calcule UNE FOIS par marquage : placeExigee
    // compte les cases libres du plateau, hors de question de le refaire pour
    // chacun des quatre types de chacune des quatre entrees de 225 cases.
    //
    // Pourquoi il n'est plus constant. ISSUE_MINIMALE ne savait tuer que les
    // poches d'une case. Une poche de cinq cases a porte unique la passait, et
    // le tuyau qui y entrait mourait quand meme -- il ne pouvait pas y gagner
    // les soixante traversees qui restaient. La question n'est pas "y a-t-il
    // de la place ?" mais "y en a-t-il ASSEZ ?", et c'est celle que la pose se
    // pose deja via culDeSac. Le marquage exige desormais la meme chose.
    //
    // Jamais moins qu'ISSUE_MINIMALE : quand l'objectif est acquis, il ne reste
    // que le garde-fou des poches.
    int besoin = objectifRestant() > 0 ? qMax(ISSUE_MINIMALE, placeExigee())
                                       : ISSUE_MINIMALE;

    // Le point fixe des etats morts : purement descriptif depuis que la mesure
    // a tranche, donc calcule seulement si quelqu'un le regarde.
    if(calculerMortes) {
        marquerMortes();
    } else {
        mortes.clear();
    }

    // Le couloir issu de la tete : il fixe l'entree de chaque case qu'il
    // traverse, et entreesDecidees s'en sert ensuite.
    propagerDepuisTete(besoin);

    oblige.fill(0, largeur * hauteur);

    int marquees = 0;

    for(int y = 0; y < hauteur; y++) {
        for(int x = 0; x < largeur; x++) {
            ETypePiece voulu = planType(x, y);

            // Une case deja pleine n'attend plus rien : la question "que
            // faut-il y mettre" n'a plus d'objet.
            if(voulu == tpNone || p->plateau()->getTypePiece(x, y) != tpNone) {
                continue;
            }

            // Par quelles entrees le flux peut-il REELLEMENT arriver ici ?
            //
            // On enumerait les ouvertures du type voulu par le plan : des
            // entrees hypothetiques, sans rapport avec le plateau. La ou une
            // voisine posee s'ouvre deja sur nous, l'entree n'est plus une
            // hypothese, elle est decidee -- et le plan pouvait reclamer un
            // type incapable de la recevoir. C'est le cas qui se voyait a la
            // tete : un horizontal marque obligatoire sous un flux qui monte,
            // alors qu'un horizontal ne recoit rien par le bas.
            QVector<ESens> entrees = entreesDecidees(x, y);

            if(entrees.isEmpty()) {
                // Rien de pose autour : l'entree reste ouverte, et le type du
                // plan dit par ou il compte etre traverse. On garde ses
                // ouvertures, moins celles qu'aucun voisin ne pourra alimenter.
                foreach(ESens s, Ecoulement::ouvertures(voulu, sHaut)) {
                    if(coteAlimentable(x, y, s)) {
                        entrees << s;
                    }
                }
            }

            if(entrees.isEmpty()) {
                continue;
            }

            // TOUTES les entrees, pas une seule. L'ancien `break` s'arretait a
            // la premiere qui marchait : la marque disait "il existe une facon
            // d'arriver ici qui force la suite", et s'affichait comme une
            // certitude. Une obligation, c'est "par ou que le flux arrive, il
            // passe la" -- donc un ET, pas un OU.
            bool obligee = true;

            foreach(ESens entree, entrees) {
                // sorties() suppose la compatibilite acquise (voir le
                // commentaire d'espaceApres) : un vertical "entre" par la
                // gauche en ressort par le haut et par le bas. Il faut donc la
                // verifier ici, et une entree que le type voulu ne peut pas
                // recevoir condamne la case au lieu de l'ignorer.
                if(!Ecoulement::piecesCompatibles(entree).contains(voulu)) {
                    obligee = false;
                    break;
                }

                QVector<ESens> issues;

                // Les DIRECTIONS encore vivantes, pas les types : la croix
                // traverse tout droit, elle et le tuyau droit de son axe ne
                // font qu'un seul chemin. Compter les types verrait un choix
                // la ou il n'y en a pas.
                foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
                    QVector<ESens> sortie = Ecoulement::sorties(type, sHaut, entree);

                    if(sortie.isEmpty()) {
                        continue;
                    }

                    // Le point fixe des morts n'entre PAS ici, et c'est mesure :
                    // le brancher coutait -0,380 niveau (t = -1,86) sur 400
                    // parties appariees. Il dit "ce tuyau finit par buter",
                    // ce qui est exact -- mais buter n'est fatal que si
                    // l'objectif n'est pas atteint d'ici la, et une impasse
                    // peut etre un long serpent qui donne largement les
                    // traversees qui restent. La mort ne dit donc pas "pas
                    // assez de traversees" ; seul le seuil repond a ca.
                    if(espaceApres(type, x, y, entree, besoin) < besoin) {
                        continue;
                    }

                    if(!issues.contains(sortie.first())) {
                        issues << sortie.first();

                        // Deux directions vivantes : la case n'est plus forcee
                        // pour cette entree, et rien de ce qui suit ne peut le
                        // changer. Inutile de payer les espaceApres restants --
                        // c'est le cas de l'immense majorite des cases (4
                        // obligees sur ~200), donc c'est la que le temps passe.
                        if(issues.size() > 1) {
                            break;
                        }
                    }
                }

                // Une seule issue, et le plan la prend deja : il n'invente rien
                // ici, il constate. Le trace passera par la, ou il ne passera
                // nulle part.
                QVector<ESens> voulue = Ecoulement::sorties(voulu, sHaut, entree);

                if(issues.size() != 1 || voulue.isEmpty()
                   || voulue.first() != issues.first()) {
                    obligee = false;
                    break;
                }
            }

            if(obligee) {
                oblige[y * largeur + x] = 1;
                marquees++;
            }
        }
    }

    qDebug() << "cases obligees :" << marquees;
}

int Bot::nbBloquees() const {
    int n = 0;

    for(int i = 0; i < p->getLargeur() * p->getHauteur(); i++) {
        if(p->plateau()->getTypePiece(i % p->getLargeur(), i / p->getLargeur()) == tpBloque) {
            n++;
        }
    }

    return n;
}

Bot::Bot(Partie *p, float cadence, quint32 seed) {
    this->p = p;
    this->cadence = cadence;
    this->seed = seed;

    tas.fill(0, p->getLargeur() * p->getHauteur());
    mancheVue = p->numeroManche();
    construirePlan();
}

void Bot::avancer(float dt) {
    // Le numero de manche, et surtout pas la graine du plateau : depuis les
    // vies, une defaite rejoue le MEME niveau, donc la meme graine derivee. Se
    // fier a la graine faisait manquer le rejeu au bot -- il gardait son tas,
    // son plan, et son "fonce" de la manche d'avant, lancait donc le flux sur
    // un plateau vide et brulait ses vies d'affilee sans poser une piece.
    int manche = p->numeroManche();

    if(manche != mancheVue) {
        mancheVue = manche;
        tas.fill(0, p->getLargeur() * p->getHauteur());
        fonce = false;

        // Le plateau a change : le plan aussi. Il est calcule sur les cases
        // bloquees de cette manche-la, donc il ne survit pas a la suivante.
        construirePlan();

        // La graine affichee ici est celle du PLATEAU, derivee de (graine de
        // partie, niveau) : elle ne se repasse pas a --graine. La commande de
        // rejeu, elle, est affichee par la fenetre a chaque manche.
        qDebug() << "=== nouvelle manche === niveau=" << p->niveau()
                 << "vies=" << p->vies()
                 << "objectif=" << p->longueurMinimale()
                 << "graine plateau (derivee)=" << p->plateau()->getGraine()
                 << "reservoir=(" << p->getXDepart() << "," << p->getYDepart() << ")";
    }

    // Les obligations ne sont PAS acquises une fois pour toutes. Chaque piece
    // posee ferme des issues aux cases voisines, et une case qui offrait encore
    // un choix se retrouve forcee. C'est meme la le gros du gisement : le
    // plateau vide n'en montre qu'une vingtaine, la manche en fabrique tout du
    // long.
    //
    // On les refait donc des que le plateau bouge -- et seulement alors : la
    // signature coute 225 lectures par battement, le marquage bien davantage.
    quint32 signature = signaturePlateau();

    if(signature != signatureVue) {
        signatureVue = signature;
        marquerObligations();
    }

    jouer(dt);
}

void Bot::demanderFoncer() {
    fonce = true;
}

void Bot::sortieReservoir(int &col, int &row) const {
    col = p->getXDepart();
    row = p->getYDepart();

    // Le sens stocke sur un reservoir est sa sortie, pas une entree.
    switch(p->plateau()->getSens(col, row)) {
    case sHaut:   row--; break;
    case sBas:    row++; break;
    case sGauche: col--; break;
    case sDroite: col++; break;
    }
}

ETypePiece Bot::planType(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return tpNone;
    }

    return (ETypePiece)plan.at(row * p->getLargeur() + col);
}

bool Bot::planObligatoire(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return false;
    }

    return oblige.at(row * p->getLargeur() + col) == 1;
}

int Bot::ouvertureLocale(int col, int row, ESens entree) const {
    int ouvertes = 0;

    foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
        if(!meneALaMort(type, col, row, entree)) {
            ouvertes++;
        }
    }

    return ouvertes;
}

bool Bot::teteCondamnee(int col, int row, ESens entree) const {
    return ouvertureLocale(col, row, entree) == 0;
}

int Bot::objectifRestant() const {
    return qMax(0, p->longueurMinimale() - p->longueurTracee());
}

// Place minimale qu'on exige devant soi tant que l'objectif n'est pas acquis,
// meme quand il ne reste presque plus rien a parcourir.
//
// Sans ce plancher, l'exigence s'eteignait exactement quand mourir coutait le
// plus cher : a cinq cases du but, culDeSac n'en reclamait plus que quatre, et
// la moindre poche passait. C'etait defendable au sens strict -- quatre cases
// suffisent a finir -- et absurde en pratique : ca jouait la manche entiere
// sur une marge de zero. Les enfermements releves mouraient tous ainsi, a
// quelques cases du but.
//
// La valeur vient d'un balayage au banc, 300 parties par point : 10,99 de
// niveau moyen sans plancher, 11,75 a 12, puis un plateau plat de 30 a 90
// (12,07 a 12,11) et une chute a 140 (11,58), ou l'exigence devient si haute
// que le bot passe son temps a defausser. On se pose au milieu du plateau.
//
// Ce que le plateau raconte : passe une trentaine de cases, le terme
// "objectif restant" ne pese plus rien. Ce qui protege n'est pas d'en garder
// juste assez pour finir, c'est d'avoir de la place tout court.
#ifndef MARGE_CUL_DE_SAC
#define MARGE_CUL_DE_SAC 40
#endif

// Ce qu'on garde en reserve AU-DELA du strict necessaire, quand l'objectif est
// presque atteint. Voir culDeSac.
//
// Valeur choisie au banc, 300 parties par point : 15,64 de niveau moyen sans
// coussin (l'exigence restait bloquee a 40), 15,78 a 5, 16,04 a 10, 15,88 a 20,
// et retour a 15,64 des 40 -- au-dela du plancher le coussin ne borne plus
// rien. Le creux a 5 dit qu'il faut garder une vraie reserve ; le repli a 20
// dit qu'en exiger trop revient au defaut d'origine.
#ifndef COUSSIN_CUL_DE_SAC
#define COUSSIN_CUL_DE_SAC 10
#endif

// La place qu'un coup doit laisser derriere lui. Extraite de culDeSac pour que
// le marquage des obligations exige la meme chose que la pose : une sortie qui
// n'offre pas de quoi finir la manche n'est pas une sortie, quelle que soit sa
// taille. Suppose objectifRestant() > 0, l'appelant traite le cas acquis.
int Bot::placeExigee() const {
    int restant = objectifRestant();

    // Ce qui doit tenir derriere la piece qu'on pose : le reste de l'objectif
    // (la piece elle-meme en est deja une), et jamais moins que la marge.
    //
    // Mais la marge se plafonne a ce que le plateau offre encore. Exiger 40
    // cases quand il n'en reste que 25 revient a refuser tous les coups par
    // construction : culDeSac rend vrai partout, ouvertureUtile tombe a zero,
    // et le bot bascule en permanence sur le critere lache -- l'exigence
    // s'annule elle-meme au moment ou elle devrait mordre le plus.
    //
    // La moitie de la place restante : assez pour garder du choix, jamais plus
    // que ce qui existe.
    int libres = 0;

    for(int i = 0; i < p->getLargeur() * p->getHauteur(); i++) {
        int x = i % p->getLargeur();
        int y = i / p->getLargeur();

        if(p->plateau()->getTypePiece(x, y) == tpNone || estTas(x, y)) {
            libres++;
        }
    }

    // La marge est bornee deux fois. Par la place du plateau -- exiger 40 cases
    // quand il n'en reste que 25 refuserait tout par construction. Et surtout
    // par CE QU'IL RESTE A FAIRE : a huit traversees du but, reclamer quarante
    // cases de reserve n'a aucun sens. Le bot se retrouvait avec trente-neuf
    // cases devant lui, besoin de huit, et refusait tous ses coups faute d'une
    // case -- puis defaussait en boucle jusqu'a se faire rattraper par le flux.
    //
    // Le coussin est ce qu'on veut GARDER EN PLUS du strict necessaire. Au
    // debut d'une manche l'objectif restant domine et la marge ne sert a rien ;
    // c'est en fin de manche qu'elle mordait, et c'est la qu'elle nuisait.
    int marge = qMin(MARGE_CUL_DE_SAC, libres / 2);
    marge = qMin(marge, restant - 1 + COUSSIN_CUL_DE_SAC);

    return qMax(marge, restant - 1);
}

bool Bot::culDeSac(const ETypePiece& type, int col, int row, ESens entree) const {
    if(objectifRestant() <= 0) {
        // Objectif acquis : ce qui reste a construire n'est que du bonus, et
        // mourir n'y coute plus une manche. On revient au seul "ne pas mourir
        // sur le coup".
        return meneALaMort(type, col, row, entree);
    }

    int besoin = placeExigee();

    // La borne fait tout le prix de la mesure : on demande "au moins ce qu'il
    // faut ?", pas la taille de la poche. Une pose mortelle ou qui ne raccorde
    // pas rend zero, donc elle tombe ici aussi.
    return espaceApres(type, col, row, entree, besoin) < besoin;
}

int Bot::ouvertureUtile(int col, int row, ESens entree) const {
    int ouvertes = 0;

    foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
        if(!culDeSac(type, col, row, entree)) {
            ouvertes++;
        }
    }

    return ouvertes;
}

int Bot::tailleRegionVide(int col, int row, int maxi, bool inclureTas) const {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    // Franchissable = vide, ou case du tas quand on l'y autorise.
    auto franchissable = [&](int x, int y) {
        ETypePiece t = plateau->getTypePiece(x, y);
        return t == tpNone || (inclureTas && estTas(x, y));
    };

    if(col < 0 || col >= largeur || row < 0 || row >= hauteur
       || !franchissable(col, row)) {
        return 0;
    }

    // Les quatre directions, independamment de tout sens de piece : c'est de
    // la place qu'on mesure, pas un tuyau.
    static const SDelta voisines[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    QVector<unsigned char> vue(largeur * hauteur, 0);
    QVector<int> aVoir;
    int depart = row * largeur + col;

    vue[depart] = 1;
    aVoir << depart;

    int compte = 0;

    while(!aVoir.isEmpty()) {
        int idx = aVoir.takeLast();
        compte++;

        if(maxi > 0 && compte >= maxi) {
            return compte;
        }

        int x = idx % largeur;
        int y = idx / largeur;

        for(int d = 0; d < 4; d++) {
            int nx = x + voisines[d].dx;
            int ny = y + voisines[d].dy;

            if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
                continue;
            }

            int nidx = ny * largeur + nx;

            if(vue[nidx] || !franchissable(nx, ny)) {
                continue;
            }

            vue[nidx] = 1;
            aVoir << nidx;
        }
    }

    return compte;
}

int Bot::espaceApres(const ETypePiece& type, int col, int row, ESens entree,
                     int maxi, QVector<unsigned char> *cases,
                     const QVector<int> *reservees) const {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    if(cases != nullptr) {
        cases->fill(0, largeur * hauteur);
    }

    // La compatibilite se verifie ici, a la difference de sorties() qui la
    // suppose acquise : un vertical "entre" par la gauche s'en sort quand meme
    // par le haut et par le bas, ce qui n'a aucun sens sur le plateau.
    QVector<ESens> sort = Ecoulement::sorties(type, sHaut, entree);

    if(!Ecoulement::piecesCompatibles(entree).contains(type) || sort.isEmpty()) {
        return 0;
    }

    // Deux facons d'avoir deja vu une case : par l'axe horizontal ou par le
    // vertical. Les distinguer sert deux fois -- a laisser une croix compter
    // pour ses deux traversees, et a reconnaitre un circuit qui se referme.
    static const unsigned char vuAxe = 0x01;   // decale de l'axe : 0x01 ou 0x02
    static const unsigned char vuLibre = 0x04;

    QVector<unsigned char> vu(largeur * hauteur, 0);
    // Rebuts que la marche a deja traverses : Bot::tete() leur retirera leur
    // marque de tas au passage, ils cessent d'etre reprenables.
    QVector<unsigned char> absorbees(largeur * hauteur, 0);
    // Rebuts traverses par la marche, avec le cote d'entree : ce sont des
    // portes de sortie. Bot::tete() sait desormais y revenir quand le trace
    // bute -- il faut qu'espaceApres compte la meme chose, sinon le bot refuse
    // par avance le coup que sa propre tete saurait rattraper.
    QVector<int> reprises;
    QVector<unsigned char> entreesReprises;
    int compte = 0;

    // Jumeau de Bot::reculerSurUnRebut, cote evaluation. Le tuyau deja pose
    // bute ; mais s'il a traverse un rebut en chemin, ce rebut est encore a
    // nous et se remplace. On repart de la plus profonde de ces cases : tout ce
    // qui precede reste acquis, et la place se compte a partir d'elle.
    //
    // Sans ce repli, espaceApres rendait zero et le bot refusait de s'engager
    // dans une chaine de rebuts finissant sur un mur -- alors qu'il lui
    // suffisait d'en reprendre l'avant-derniere case pour repartir ailleurs.
    auto replier = [&](int &rx, int &ry, ESens &re) {
        if(reprises.isEmpty()) {
            return false;
        }

        int idx = reprises.last();

        // La case redevient disponible : c'est la qu'on reconstruira.
        vu[idx] = 0;
        absorbees[idx] = 0;

        rx = idx % largeur;
        ry = idx / largeur;
        re = (ESens)entreesReprises.last();

        return true;
    };

    auto compter = [&](int idx) {
        compte++;

        if(cases != nullptr) {
            (*cases)[idx] = mrAtteignable;
        }
    };

    // Les trous de la chaine projetee sont deja pris : le flux y passera. Les
    // compter comme de la place libre revient a compter deux fois le meme
    // terrain -- une fois comme chemin, une fois comme reserve.
    if(reservees != nullptr) {
        foreach(int idx, *reservees) {
            if(idx >= 0 && idx < vu.size()) {
                vu[idx] |= vuLibre;
            }
        }
    }

    // La piece qu'on pose occupe sa case : le parcours ne doit pas la
    // recompter comme de la place s'il y revient.
    vu[row * largeur + col] = vuLibre | (vuAxe << Ecoulement::axe(type, entree));

    // --- 1. suivre le tuyau deja en place ---------------------------------
    //
    // Derriere la sortie, le trace n'a aucun choix : les pieces posees le
    // conduisent, case apres case, jusqu'a la premiere ou il faudra reposer
    // quelque chose. C'est la, et pas avant, que la place disponible commence
    // a compter. Sauter cette marche -- ce que faisait la premiere version --
    // laissait passer la pose qui referme le circuit sur lui-meme : elle se
    // raccorde, elle laisse tout le plateau libre devant, et le flux n'en
    // sortira jamais.
    int x, y;
    ESens e;
    Ecoulement::voisine(col, row, sort.first(), x, y, e);

    for(;;) {
        if(x < 0 || x >= largeur || y < 0 || y >= hauteur) {
            if(!replier(x, y, e)) {
                return 0;   // un mur, et aucun rebut ou revenir
            }

            break;
        }

        int idx = y * largeur + x;
        ETypePiece t = plateau->getTypePiece(x, y);

        if(t == tpNone) {
            break;      // case libre : la tete a venir
        }

        ESens sens = plateau->getSens(x, y);
        bool raccorde = Ecoulement::ouvertures(t, sens).contains(e);

        // Un rebut qui ne raccorde pas n'arrete pas le trace : le bot le
        // reprendra, comme le fait Bot::tete(). C'est donc lui, la tete.
        //
        // Mais seulement s'il est ENCORE reprenable. Bot::tete() efface la
        // marque de tas de chaque rebut qu'elle traverse : raccorde au flux, il
        // sert le trace, on ne le reprend plus. Une case peut donc etre
        // absorbee sur un passage puis revue sur l'autre -- une croix, ou un
        // circuit qui repasse -- et au second passage elle n'est plus a nous.
        //
        // Sans cette memoire, la marche croyait pouvoir reprendre une case que
        // la pose evaluee venait justement de lui retirer : elle annoncait une
        // tete a venir et 181 cases derriere, la ou le circuit se refermait.
        // Le filtre mentait sur exactement ce point.
        if(estTas(x, y) && !raccorde && !absorbees[idx]) {
            break;
        }

        if(!raccorde) {
            if(!replier(x, y, e)) {
                return 0;   // bloc ou piece etrangere, et rien ou revenir
            }

            break;
        }

        QVector<ESens> st = Ecoulement::sorties(t, sens, e);

        if(st.isEmpty()) {
            if(!replier(x, y, e)) {
                return 0;
            }

            break;
        }

        unsigned char bit = vuAxe << Ecoulement::axe(t, e);

        if(vu[idx] & bit) {
            return 0;   // le circuit se referme : plus de tete, manche morte
        }

        vu[idx] |= bit;

        if(estTas(x, y) && !absorbees[idx]) {
            reprises << idx;
            entreesReprises << (unsigned char)e;
        }

        absorbees[idx] = 1;

        // Cette case-la, le flux la traversera pour de bon : elle compte.
        //
        // Et `maxi` ne borne pas cette marche-ci, seulement le comptage de
        // place qui suit. S'arreter ici des que le compte suffit reviendrait a
        // repondre "assez de place" sans etre alle voir si le tuyau debouche
        // quelque part -- c'est precisement le circuit referme qu'on cherche a
        // reconnaitre. La boucle se termine de toute facon : chaque case n'a
        // que deux axes, et le marquage les epuise.
        compter(idx);

        Ecoulement::voisine(x, y, st.first(), x, y, e);
    }

    // --- 2. compter la place a partir de la tete --------------------------
    //
    // Une case vide ou du tas se propage aux quatre cotes -- on y mettra la
    // piece qu'on veut ; une croix deja posee ne se traverse que tout droit ;
    // tout le reste arrete le parcours.
    QVector<int> aVoir;

    auto tenter = [&](int nx, int ny, ESens ne) {
        if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
            return;
        }

        int idx = ny * largeur + nx;
        ETypePiece t = plateau->getTypePiece(nx, ny);
        unsigned char marque;

        if(t == tpNone || estTas(nx, ny)) {
            marque = vuLibre;
        } else if(t == tpCroix) {
            marque = vuAxe << Ecoulement::axe(t, ne);
        } else {
            return;
        }

        if(vu[idx] & marque) {
            return;
        }

        vu[idx] |= marque;
        compter(idx);
        aVoir << idx * 4 + ne;
    };

    tenter(x, y, e);

    while(!aVoir.isEmpty() && (maxi <= 0 || compte < maxi)) {
        int etat = aVoir.takeLast();
        ESens se = (ESens)(etat % 4);
        int idx = etat / 4;
        int sx = idx % largeur;
        int sy = idx / largeur;

        if(plateau->getTypePiece(sx, sy) == tpCroix && !estTas(sx, sy)) {
            // Croix posee : tout droit et rien d'autre. On demande sa sortie au
            // moteur plutot que de retourner le sens a la main -- la geometrie
            // des tuyaux n'a qu'un seul proprietaire.
            QVector<ESens> st = Ecoulement::sorties(tpCroix, sHaut, se);

            if(!st.isEmpty()) {
                int nx, ny;
                ESens ne;
                Ecoulement::voisine(sx, sy, st.first(), nx, ny, ne);
                tenter(nx, ny, ne);
            }

            continue;
        }

        for(int s = 0; s < 4; s++) {
            int nx, ny;
            ESens ne;
            Ecoulement::voisine(sx, sy, (ESens)s, nx, ny, ne);
            tenter(nx, ny, ne);
        }
    }

    return compte;
}

void Bot::unGeste() {
    // Le jeton plein plutot qu'un dt calcule : un geste sort quelle que soit la
    // cadence, et le reste de la mecanique -- comptes de manche, strategie --
    // est celui de tous les jours. jouer() n'utilise dt que pour ce jeton.
    jetons = 1.0f;
    avancer(0.0f);
}

bool Bot::poserSurTete() {
    int col, row;
    ESens entree;

    if(!tete(col, row, entree)) {
        return false;
    }

    return p->poserPiece(col, row);
}

QVector<unsigned char> Bot::regionApresPoseSurTete() {
    QVector<unsigned char> region;
    int col, row;
    ESens entree;

    if(!tete(col, row, entree)) {
        return region;
    }

    espaceApres(p->file()->getPiece(0).type, col, row, entree, 0, &region);
    region[row * p->getLargeur() + col] = mrPose;

    return region;
}

void Bot::abandonner(int col, int row, ESens entree) {
    Piece haut = p->file()->getPiece(0);

    if(Ecoulement::piecesCompatibles(entree).contains(haut.type)) {
        p->poserPiece(col, row);
    } else {
        defausser();
    }

    demanderFoncer();
}

bool Bot::acculeParLeFlux(float margeGestes) const {
    if(p->etat() != epEcoulement) {
        return false;
    }

    const Ecoulement *e = p->ecoulement();

    // casesEnAval : tuyau deja pose que le flux doit encore parcourir avant la
    // tete. Une case lui prend dureeRemplissage secondes ; le bot joue cadence
    // gestes par seconde.
    return e->casesEnAval() * e->getDureeRemplissage() * cadence <= margeGestes;
}

bool Bot::veutFoncer() const {
    return fonce;
}

void Bot::coinLePlusEloigne(int &x, int &y) const {
    x = 0;
    y = 0;

    if(p->getXDepart() < p->getLargeur() - p->getXDepart()) {
        x = p->getLargeur() - 1;
    }

    if(p->getYDepart() < p->getHauteur() - p->getYDepart()) {
        y = p->getHauteur() - 1;
    }
}

void Bot::defausser() {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();
    int taille = largeur * hauteur;

    Piece piece = p->file()->getPiece(0);

    int choix = -1;
    int rangChoix = 0;
    int distChoix = -1;

    // La tete de construction est interdite a la defausse. Le plan la reclame
    // comme n'importe quelle autre case, et il a raison sur le fond -- c'est
    // bien cette piece-la qu'il y veut -- mais la defausse pose sans regarder
    // ou en est le trace. Elle scelle alors la case par laquelle il devait
    // continuer : le flux y entre, n'en ressort pas, et la manche est perdue
    // avec le plateau encore aux trois quarts vide.
    //
    // Le tas est un reseau PARALLELE. Ecrire sur la tete, ce n'est plus
    // defausser, c'est jouer -- et jouer sans aucun des tests que la tete
    // s'impose. On laisse donc la case au trace : le plan reclame le meme type
    // ailleurs, la defausse ira la.
    int teteCol, teteRow;
    ESens teteEntree;
    bool aTete = tete(teteCol, teteRow, teteEntree);
    int caseTete = aTete ? teteRow * largeur + teteCol : -1;

    // Distance de chaque case au rebut le plus proche, en parcourant la grille
    // depuis toutes les cases du tas a la fois. C'est le critere principal du
    // choix : on defausse AU PLUS PRES de ce qui est deja defausse.
    //
    // Pourquoi. Une piece isolee ne sert a rien tant que ses voisines du plan ne
    // sont pas arrivees, et rien ne garantit qu'elles arrivent. Serrees, elles
    // se raccordent tout de suite : le tas devient du tuyau utilisable au lieu
    // d'un semis de pieces esperant leurs voisines. Et le reste du plateau
    // reste d'un seul tenant pour le trace, au lieu d'etre mite de rebuts.
    QVector<int> distTas(taille, -1);
    QVector<int> file;

    for(int i = 0; i < taille; i++) {
        if(estTas(i % largeur, i / largeur)) {
            distTas[i] = 0;
            file << i;
        }
    }

    for(int t = 0; t < file.size(); t++) {
        int idx = file.at(t);
        int x = idx % largeur;
        int y = idx / largeur;

        for(int s = 0; s < 4; s++) {
            int nx = x + deltaSens[s].dx;
            int ny = y + deltaSens[s].dy;

            if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur) {
                continue;
            }

            int nidx = ny * largeur + nx;

            if(distTas[nidx] < 0) {
                distTas[nidx] = distTas[idx] + 1;
                file << nidx;
            }
        }
    }

    // Tas vide : toutes les cases se valent, c'est l'eloignement de la tete qui
    // tranchera seul, comme au premier geste d'une manche.
    int procheChoix = taille + 1;
    bool rang2Choix = false;
    bool paireChoix = false;
    bool obligeChoix = false;

    // Au plus loin de la TETE, pas du reservoir. Ce qui compte n'est pas d'ou
    // le flux est parti mais ou en est la construction : c'est autour de la
    // tete que le trace a besoin de place, et c'est la que le flux arrivera le
    // plus tot. Defausser au loin laisse donc a la piece le plus de temps pour
    // voir ses voisines du plan la rejoindre, et degage le terrain devant le
    // trace au lieu de degager celui qu'il a deja quitte.
    //
    // Le reservoir sert de repli quand il n'y a plus de tete -- il n'y a alors
    // plus de trace a menager, mais il faut bien un point de reference.
    int rx = aTete ? teteCol : p->getXDepart();
    int ry = aTete ? teteRow : p->getYDepart();

    for(int y = 0; y < hauteur; y++) {
        for(int x = 0; x < largeur; x++) {
            // Le plan decide seul de l'endroit : une piece ne se defausse que
            // la ou il reclame exactement ce type. Le sens tire avec la piece
            // n'entre pas dans la comparaison, et c'est correct -- pour tout ce
            // qui n'est pas un reservoir, Ecoulement::ouvertures() ignore le
            // sens, l'orientation d'un coude est portee par son type.
            //
            // Une exception, et une seule : sur une case OBLIGEE, ce qui compte
            // n'est pas le type mais le chemin. Une croix traverse tout droit,
            // elle vaut donc le tuyau droit de son axe -- le refuser laissait
            // passer une pose parfaite faute d'egalite de types. Ailleurs on ne
            // substitue pas : le plan est une intention, et depenser une croix
            // pour tenir un droit n'a de sens que la ou la case est certaine
            // d'etre traversee.
            ETypePiece voulu = planType(x, y);

            if(voulu != piece.type
               && !(planObligatoire(x, y) && memeRoutage(piece.type, voulu))) {
                continue;
            }

            if(!p->peutPoser(x, y)) {
                continue;
            }

            if(y * largeur + x == caseTete) {
                continue;
            }

            ETypePiece actuelle = plateau->getTypePiece(x, y);

            // Rang 0 : case vide, la pose est gratuite.
            // Rang 1 : une case du tas que le plan veut differente -- on
            //   corrige notre propre reseau, rien d'autre n'en depend.
            // Rang 2 : une case deja conforme. Y reposer la meme piece coute un
            //   remplacement pour ne rien changer au plateau -- mais ca ne
            //   casse rien, et c'est ce qui fait descendre la file quand le plan
            //   n'a plus de case libre pour ce type.
            //
            // Une piece du TRACE principal, elle, n'est plus touchee. Elle
            // l'etait en dernier recours, et la mesure a tranche : sur 80 000
            // defausses, ce recours ne servait que 11 fois, mais coupait le
            // trace une fois sur deux -- jusqu'a lui faire perdre 39 cases d'un
            // coup. Le rang 2 rend le meme service 9 000 fois sans rien casser,
            // et une defausse qui n'a nulle part ou aller a toujours le filet.
            if(actuelle != tpNone && actuelle != piece.type && !estTas(x, y)) {
                continue;
            }

            int rang = actuelle == tpNone ? 0 : (actuelle == piece.type ? 2 : 1);
            int dist = qAbs(x - rx) + qAbs(y - ry);
            int proche = distTas.at(y * largeur + x);

            if(proche < 0) {
                proche = taille;   // aucun rebut sur le plateau
            }

            // Le rang d'abord -- une pose gratuite vaut mieux qu'un
            // remplacement --, puis l'eloignement de la tete, et la proximite
            // du tas ne departage que les ex aequo.
            //
            // Cet ordre-la est mesure, et l'inverse coute cher : serrer les
            // rebuts les uns contre les autres complete les circuits du plan
            // bien plus vite, et un circuit complet est un piege -- le trace y
            // entre, en fait le tour, revient a son entree et meurt. Disperser
            // sur des circuits differents evite ca ; la proximite ne sert qu'a
            // choisir entre des cases deja equivalentes.
            // La case de rang 2 du trajet anticipe passe devant tout le reste.
            // C'est un pari -- elle ne sera sur le trajet que si le rang 1
            // recoit le type suppose, ce qui arrive 39 % du temps -- mais un
            // pari SANS PERTE : on ne le prend que sur une case que le plan
            // reclame deja pour cette piece, donc un pari perdu reste une
            // defausse ordinaire. A 39 % contre les 31 % que rapporte une
            // defausse, l'esperance est favorable.
            bool rang2 = (y * largeur + x) == ancrageDefausse;

            // Case OBLIGEE : le plan n'y propose pas un type, il constate le
            // seul routage qui survive. Y poser n'est plus une defausse mais de
            // la construction : le trace passera par la, ou il ne passera pas.
            //
            // C'est pour ca qu'elle passe devant l'eloignement de la tete. Ce
            // critere-la protege les defausses SPECULATIVES -- une piece isolee
            // qui attend ses voisines du plan a besoin de temps et de place. Une
            // case obligee n'attend personne : elle sera traversee.
            bool oblige = planObligatoire(x, y);

            // Une PAIRE : une voisine encore libre que le plan reclame pour le
            // MEME type, et qui se raccorde a celle-ci. Y poser, c'est amorcer
            // un troncon de deux -- il suffira de retirer une seconde piece du
            // meme type pour le fermer, et le trace ramassera alors deux cases
            // d'un coup au lieu d'une.
            //
            // Seuls trois types forment de telles paires : les deux droits,
            // cote a cote dans leur axe, et la croix qui se raccorde a tout.
            // Deux coudes identiques se tournent toujours le dos. C'est peu,
            // mais ces paires-la sont nombreuses -- une trentaine par geste
            // pour un horizontal -- et le bot les ignorait toutes : mesure, le
            // tas etait a 94 % de cases isolees, ce qui plafonnait la
            // recuperation a 31 %.
            bool paire = false;

            for(int s = 0; s < 4 && !paire; s++) {
                if(!Ecoulement::ouvertures(piece.type, sHaut).contains((ESens)s)) {
                    continue;
                }

                int nx = x + deltaSens[s].dx;
                int ny = y + deltaSens[s].dy;

                if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur
                   || plateau->getTypePiece(nx, ny) != tpNone
                   || !p->peutPoser(nx, ny)
                   || planType(nx, ny) != piece.type) {
                    continue;
                }

                // Le raccord doit etre rendu : deux horizontaux empiles se
                // touchent sans se raccorder.
                if(Ecoulement::ouvertures(piece.type, sHaut).contains(opposeSens[s])) {
                    paire = true;
                }
            }

            bool mieux = choix < 0
                         || rang < rangChoix
                         || (rang == rangChoix && oblige && !obligeChoix)
                         || (rang == rangChoix && oblige == obligeChoix
                             && rang2 && !rang2Choix)
                         || (rang == rangChoix && oblige == obligeChoix
                             && rang2 == rang2Choix && paire && !paireChoix)
                         // L'eloignement de la tete s'INVERSE sur une case
                         // obligee. Le "au plus loin" protege les defausses
                         // speculatives : une piece isolee a besoin de temps
                         // pour voir ses voisines du plan la rejoindre, et de
                         // place devant le trace. Une case obligee n'attend
                         // personne et ne parie sur rien -- elle sera traversee.
                         // La poser au plus PRES, c'est du tuyau que le flux
                         // prend tout de suite, et une case gagnee maintenant
                         // plutot qu'une esperance a l'autre bout du plateau.
                         || (rang == rangChoix && oblige == obligeChoix
                             && rang2 == rang2Choix && paire == paireChoix
                             && (oblige ? dist < distChoix : dist > distChoix))
                         || (rang == rangChoix && oblige == obligeChoix
                             && rang2 == rang2Choix
                             && paire == paireChoix && dist == distChoix
                             && proche < procheChoix);

            if(mieux) {
                choix = y * largeur + x;
                rangChoix = rang;
                obligeChoix = oblige;
                rang2Choix = rang2;
                paireChoix = paire;
                procheChoix = proche;
                distChoix = dist;
            }
        }
    }

    // Filet de securite. Le plan peut ne rien reclamer : plus une seule case de
    // ce type a corriger, ou un plateau qui n'est pas le 15x15 pour lequel il
    // est calcule. Une piece doit partir a chaque defausse -- sinon la file ne
    // descend pas et le bot rejoue le meme geste indefiniment --, alors on la
    // range sur la case vide la plus eloignee de la tete.
    if(choix < 0) {
        for(int y = 0; y < hauteur; y++) {
            for(int x = 0; x < largeur; x++) {
                if(plateau->getTypePiece(x, y) != tpNone || !p->peutPoser(x, y)
                   || y * largeur + x == caseTete) {
                    continue;
                }

                int dist = qAbs(x - rx) + qAbs(y - ry);

                if(choix < 0 || dist > distChoix) {
                    choix = y * largeur + x;
                    distChoix = dist;
                }
            }
        }
    }

    if(choix >= 0) {
        // Poser sur la case visee par l'anticipation, c'est prendre le pari :
        // on le marque pour pouvoir le distinguer a l'ecran.
        poserCaseTas(choix % largeur, choix / largeur,
                     choix == ancrageDefausse ? 3 : 1);
    }
}

bool Bot::poserCaseTas(int col, int row, unsigned char origine) {
    if(!p->poserPiece(col, row)) {
        return false;
    }

    tas[row * p->getLargeur() + col] = origine;
    return true;
}

bool Bot::estAnticipee(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return false;
    }

    return tas.at(row * p->getLargeur() + col) == 2;
}

bool Bot::estPari(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return false;
    }

    return tas.at(row * p->getLargeur() + col) == 3;
}

// Recul sur un rebut : appele quand le trace, suivi depuis le reservoir, bute.
// Plutot que de rendre "plus de tete", on regarde les rebuts qu'on a traverses
// en chemin -- ils sont encore a nous, donc encore remplacables -- et on rend
// le plus profond qui ait encore une issue.
//
// Pourquoi c'est necessaire. Bot::tete() avalait la chaine du tas d'un bloc et
// ne rendait que son extremite. Quand cette extremite butait sur un mur, la
// manche etait declaree perdue -- alors que trois cases en arriere, un rebut
// bien a nous n'attendait qu'un remplacement pour repartir ailleurs. On perdait
// une manche entiere, objectif encore loin, pour economiser 25 points de
// penalite. Le cas type : une chaine de quatre rebuts le long du bord, qui
// finit sur une piece du trace principal en travers.
//
// On prend le plus profond et non le premier : tout ce qui est avant reste
// acquis au trace, et c'est autant de traversees deja gagnees.
bool Bot::reculerSurUnRebut(const QVector<int> &reprises,
                            const QVector<unsigned char> &entrees,
                            int &col, int &row, ESens &entree) {
    int largeur = p->getLargeur();

    for(int k = reprises.size() - 1; k >= 0; k--) {
        int cx = reprises.at(k) % largeur;
        int cy = reprises.at(k) / largeur;
        ESens ce = (ESens)entrees.at(k);

        // Une issue veut dire : une piece existe qui s'y raccorde sans
        // condamner. Sinon ce rebut-la ne vaut pas mieux que le mur, et on
        // remonte encore.
        if(ouvertureLocale(cx, cy, ce) > 0) {
            // Ce qui precede le point de reprise est definitivement au trace.
            for(int j = 0; j < k; j++) {
                tas[reprises.at(j)] = 0;
            }

            col = cx;
            row = cy;
            entree = ce;

            return true;
        }
    }

    // Aucun rebut ne sauve la mise : la manche est bien finie.
    for(int j = 0; j < reprises.size(); j++) {
        tas[reprises.at(j)] = 0;
    }

    return false;
}

bool Bot::tete(int& col, int& row, ESens& entree) {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int x = p->getXDepart();
    int y = p->getYDepart();
    // Reservoir : le sens stocke est sa sortie, pas une entree.
    ESens e = plateau->getSens(x, y);

    // Rebuts traverses depuis le reservoir, avec le cote par lequel le flux y
    // entre. On ne les sort du tas qu'une fois la tete connue : tant qu'on
    // n'a pas trouve ou construire, ils restent des points de reprise.
    QVector<int> reprises;
    QVector<unsigned char> entrees;

    // Borne de securite : une croix peut refermer un circuit sur lui-meme.
    //
    // Deux passages par case, pas un : la croix se traverse une fois par axe,
    // et un plan de defausse forme a lui seul un trace de plus de 250
    // traversees pour 225 cases. Borner au nombre de cases faisait rendre
    // "plus de tete" sur un trace long mais parfaitement vivant.
    for(int pas = 0; pas < 2 * plateau->getSize(); pas++) {
        int sx, sy;
        ESens se;

        if(!p->ecoulement()->suivante(x, y, e, sx, sy, se)) {
            // Sortie hors grille : fuite programmee. Reste a voir si un rebut
            // traverse en chemin offre une porte de sortie.
            return reculerSurUnRebut(reprises, entrees, col, row, entree);
        }

        ETypePiece t = plateau->getTypePiece(sx, sy);

        if(t == tpNone) {
            // Case vide : c'est la ou l'on construit -- sauf si elle est
            // condamnee et qu'on a traverse des rebuts pour y arriver.
            //
            // Une tete libre mais sans issue n'est pas une reussite : le bot y
            // posera une piece qui meurt, et la manche est finie. Or les rebuts
            // franchis en chemin sont encore a nous. Les suivre jusqu'au bout
            // etait un choix, pas une fatalite : mieux vaut en remplacer un et
            // repartir ailleurs, contre 25 points, que de se laisser conduire
            // dans le mur par sa propre defausse. Une fois la case absorbee,
            // elle sort du tas et l'occasion est perdue pour de bon.
            if(!reprises.isEmpty() && ouvertureLocale(sx, sy, se) == 0
               && reculerSurUnRebut(reprises, entrees, col, row, entree)) {
                return true;
            }

            for(int j = 0; j < reprises.size(); j++) {
                tas[reprises.at(j)] = 0;
            }

            col = sx;
            row = sy;
            entree = se;

            return true;
        }

        if(!Ecoulement::ouvertures(t, plateau->getSens(sx, sy)).contains(se)) {
            if(estTas(sx, sy)) {
                // Un rebut en travers du chemin : on construira ici meme, en le
                // reprenant. Ce n'est pas un cul-de-sac -- sauf s'il n'a lui
                // non plus aucune issue, auquel cas on recule comme ci-dessus.
                if(!reprises.isEmpty() && ouvertureLocale(sx, sy, se) == 0
                   && reculerSurUnRebut(reprises, entrees, col, row, entree)) {
                    return true;
                }

                for(int j = 0; j < reprises.size(); j++) {
                    tas[reprises.at(j)] = 0;
                }

                col = sx;
                row = sy;
                entree = se;

                return true;
            }

            // Bloc ou piece etrangere : le trace bute. Meme question qu'au-dessus.
            return reculerSurUnRebut(reprises, entrees, col, row, entree);
        }

        // On ne prend pas la chaine pour argent comptant : a CHAQUE rebut
        // traverse, on refait le calcul de place. Suivre la chaine jusqu'au
        // bout pour decouvrir la qu'elle ne menait nulle part, c'est arriver
        // trop tard -- les cases d'avant sont deja derriere nous, et le rebut
        // absorbe n'est plus remplacable. Des que la suite ne laisse plus de
        // quoi finir, on s'arrete ICI : cette case devient la tete, on
        // l'ecrase contre 25 points, et on repart ailleurs.
        //
        // La condition d'issue evite de s'arreter sur une case qui ne vaut pas
        // mieux que la suite ; celle de peutPoser, sur une case ou le fluide
        // est deja passe.
        if(estTas(sx, sy) && p->peutPoser(sx, sy)
           && culDeSac(t, sx, sy, se)
           && ouvertureLocale(sx, sy, se) > 0) {
            for(int j = 0; j < reprises.size(); j++) {
                tas[reprises.at(j)] = 0;
            }

            col = sx;
            row = sy;
            entree = se;

            return true;
        }

        // La case raccorde : le flux la traversera. Si c'est un rebut, on le
        // note comme point de reprise possible plutot que de le sortir du tas
        // tout de suite -- on ne sait pas encore si le trace ira au bout.
        if(estTas(sx, sy)) {
            reprises << sy * largeur + sx;
            entrees << (unsigned char)se;
        }

        x = sx;
        y = sy;
        e = se;
    }

    return reculerSurUnRebut(reprises, entrees, col, row, entree);
}

bool Bot::meneALaMort(const ETypePiece& type, int col, int row, ESens entree) const {
    // Exactement la question d'espaceApres, arretee au premier signe de vie :
    // le trace a-t-il encore une case ou aller ? Zero veut dire non -- mur,
    // piece qu'on ne peut ni traverser ni reprendre, ou circuit referme.
    return espaceApres(type, col, row, entree, 1) == 0;
}

bool Bot::estTas(int col, int row) const {
    if(col < 0 || col >= p->getLargeur() || row < 0 || row >= p->getHauteur()) {
        return false;
    }

    return tas.at(row * p->getLargeur() + col) != 0;
}

bool Bot::prendreJeton(float dt) {
    jetons = qMin(1.0f, jetons + dt * cadence);

    if(jetons < 1.0f) {
        return false;
    }

    jetons -= 1.0f;
    return true;
}
