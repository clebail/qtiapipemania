#ifndef TRACE_H
#define TRACE_H

#include <QVector>
#include "game.h"

// Le trace planifie : le chemin complet du reservoir jusqu'a l'objectif,
// calcule une fois sur le plateau de la manche. Voir TRACE.md.
//
// Rien a voir avec le plan de defausse de Bot (construirePlan), qui pave le
// plateau de circuits fermes pour que les pieces jetees tombent sur quelque
// chose de raccordable. Celui-ci est UN chemin, oriente, du reservoir a la fin,
// et il fixe le type de chacune de ses cases -- puisque le type se deduit
// entierement du couple (entree, sortie).
//
// Range comme Minage et Ecoulement : geometrie pure, aucune horloge, aucun
// etat de jeu. Il ne connait ni la file, ni le flux, ni les bots.
//
// Gabarit : recherche en profondeur, ordonnee par la regle de Warnsdorff --
// a chaque pas, essayer d'abord la case qui a le MOINS de voisines encore
// libres. C'est la regle des parcours de cavalier, et elle vaut ici pour les
// memes raisons : elle traite les etranglements d'abord, donc elle evite de se
// couper un morceau de terrain, et elle longe les bords. Elle part de n'importe
// ou, ce qui compte : le reservoir tombe au milieu du plateau aussi souvent
// qu'au bord, et un serpentin de gabarit exigerait d'abord de rejoindre un coin.
//
// Gloutonne, elle ne suffit pas : mesuree le 18 septembre 2026 sur douze
// plateaux de niveau 35, elle couvre 19 % du terrain libre et n'atteint jamais
// l'objectif. Une grille a quatre voisins offre trop peu d'echappatoires, et le
// premier mauvais choix coupe le plateau en deux. Il lui faut donc :
//
//   - le RETOUR ARRIERE, avec un budget de noeuds ; l'ordre de Warnsdorff sert
//     alors de bonne premiere intuition plutot que de decision definitive ;
//   - l'ELAGAGE par la place restante : depuis la case candidate, la region
//     vide atteignable doit contenir au moins ce qu'il reste a parcourir. Meme
//     mesure que Bot::tailleRegionVide, meme raison -- ne pas s'engager dans
//     une poche trop petite pour finir ;
//   - le traitement des CULS-DE-SAC : une candidate sans aucune voisine libre
//     termine la marche, donc on ne l'accepte que comme derniere case. La
//     laisser de cote abandonne une case, ce qui est sans importance : on
//     cherche une longueur, pas une couverture complete.
//
// Le budget epuise, on rend le plus long chemin rencontre -- et c'est une
// information en soi : un trace plus court que l'objectif dit que la manche est
// perdue d'avance, ce qu'aucun bot actuel ne sait voir (TRACE.md, §5).
class Trace
{
public:
    Trace();

    // Calcule le trace sur ce plateau, a partir du reservoir et de son sens.
    // `longueurVisee` est la longueur voulue : la recherche s'arrete des
    // qu'elle l'atteint, sans chercher mieux. 0 = viser tout le terrain libre,
    // ce dont on se sert pour mesurer ce qui est atteignable.
    // `budget` borne le nombre de cases empilees par la recherche.
    //
    // `gestesAvantDepart` / `gestesParCase` : l'ECHEANCE de chaque rang. Le
    // flux atteint le rang k a t = delai + k x duree, et le bot aura tire d'ici
    // la `gestesAvantDepart + k * gestesParCase` pieces. Une piece ne peut donc
    // servir le rang k que si elle arrive avant cette echeance -- et la borne
    // est large : 44 pieces au rang 0, 182 au rang 69.
    //
    // C'est ce qui fait la difference entre garantir le DEBUT du trace et le
    // garantir en ENTIER. Mesure du 18 septembre 2026 : le bot mourait au
    // niveau 20 avec un trace de 86 pour un objectif de 86, sur un unique trou
    // au rang 69 -- vingt-quatre tirages d'affilee sans le coude voulu pendant
    // que le flux marchait dessus. Le rang 69 avait 182 tirages devant lui ; la
    // garde, elle, s'arretait a 44 faute de compter les echeances.
    //
    // A zero, aucune echeance : toutes les pieces servent tous les rangs, ce
    // qui est le bon modele quand on ne connait que les cinq visibles.
    //
    // Les ranges sont traitees dans l'ordre et les echeances croissent avec le
    // rang : servir chaque rang avec la piece la PLUS ANCIENNE qui convient est
    // alors optimal (c'est l'ordonnancement a echeances, l'argument d'echange
    // habituel). Le glouton n'est donc pas une heuristique ici.
    //
    // `enMain` : les pieces VISIBLES dans la file, dans l'ordre. Quand elle
    // n'est pas vide, la recherche impose aux premiers rangs du trace -- autant
    // qu'il y a de pieces -- de reclamer exactement ces types-la, la croix
    // comptee comme un droit puisqu'elle en tient lieu. Le debut du trace est
    // alors servi d'avance : les pieces qu'on tient ont leur case, et le
    // prefixe contigu demarre sans attendre un tirage.
    //
    // Ce n'est pas un raffinement. Mesure du 18 septembre 2026 sur les douze
    // effondrements du bot : les DOUZE trous fatals sont des coudes, aucun
    // droit. Un coude n'arrive que 1 fois sur 7 quand un droit est servi 3 fois
    // sur 7 (son tirage plus la croix) -- une case de coude en tete de trace
    // est donc trois fois plus exposee, et le quota du generateur, qui
    // equilibre sur le trace ENTIER, ignore que le debut coute plus cher que la
    // fin. La contrainte supprime l'exposition la ou on sait deja ce qu'on a ;
    // le biais des droits (voir DROITS_EN_TETE dans trace.cpp) couvre la suite.
    //
    // La contrainte est DEGRESSIVE : on demande d'abord que les cinq premiers
    // rangs soient servis par la main, puis quatre, puis trois, et au pire
    // aucun. Exiger les cinq echoue le plus souvent, pour une raison purement
    // geometrique -- l'entree ne laisse que trois types possibles a chaque
    // rang, et il faut que la main en couvre un a chaque fois. Trois rangs
    // servis d'avance valent mieux que zero, et l'essai qui echoue ne coute
    // rien : il meurt en un a quatre noeuds, faute de candidate payable.
    //
    // Ne marche que sur les cases VIDES : une piece deja posee est un obstacle
    // au meme titre qu'un bloc. Le trace se calcule donc sur un plateau neuf,
    // au debut de la manche.
    void calculer(const Game *plateau, int longueurVisee = 0, long budget = 400000,
                  const QVector<ETypePiece> &enMain = QVector<ETypePiece>(),
                  int gestesAvantDepart = 0, int gestesParCase = 0);
    void vider();

    // AMELIORE LE TRACE PAR REECRITURE LOCALE, croisements compris.
    //
    // Une fenetre glisse le long des rangs ; chacune est re-resolue par une
    // recherche exhaustive qui, elle, a le droit de repasser sur une case --
    // entree et sortie de la fenetre figees, vivier borne aux cases de la
    // fenetre plus le terrain libre qui les touche, et remplacement seulement
    // si ca rend STRICTEMENT plus de passes. On repasse jusqu'au point fixe.
    //
    // Deux gains, et ils viennent ensemble parce que c'est la meme recherche :
    //
    //   - la COUVERTURE : la fenetre ramasse des cases libres que le tracage
    //     global avait laissees de cote ;
    //   - les CROIX : une case traversee deux fois compte double
    //     (Ecoulement indexe ses conduites par axe), et c'est la seule facon
    //     d'encaisser la croix a plein.
    //
    // Une loi a connaitre avant de lire le code. Damier : un trace alterne les
    // couleurs a chaque pas, donc une marche de L visites a ses deux bouts de
    // meme couleur si et seulement si L est impair, et une croix ajoute une
    // visite DE SA PROPRE COULEUR. A encombrement et extremites figes on ne
    // peut donc ajouter des croix que PAR PAIRES -- une seule retournerait la
    // parite et aucune marche n'existerait. Une croix isolee exige une case de
    // terrain en plus, d'ou le melange couverture/croisement.
    //
    // Mesure du prototype (banc/croix.cpp, 2026-09-19) : +12 passes en
    // moyenne, 6 a 8 croix, une milliseconde, zero trace illegal sur 72.
    void ameliorer(const Game *plateau, int fenetre = 14);

    bool estCalcule() const;
    // Nombre de cases du trace. C'est aussi ce que le flux traversera s'il va
    // au bout -- une case, une traversee (la croix comptee deux fois est hors
    // sujet ici : le trace ne se croise pas encore, cf. TRACE.md §7).
    int longueur() const;
    // Combien de rangs du debut la main a pu servir d'avance -- zero quand la
    // geometrie n'en a laisse aucun. Sert a savoir ce que la contrainte a
    // vraiment obtenu, ce qui ne se devine pas du dehors.
    int garde() const;
    // Graine du plateau sur lequel il a ete calcule : sert a savoir qu'il est
    // perime, c'est le meme signal de nouvelle manche que pour les bots.
    quint32 graine() const;

    // Type que le trace reclame sur cette case, tpNone hors trace.
    ETypePiece type(int col, int row) const;
    // Rang de la case sur le trace (0 = la premiere apres le reservoir),
    // -1 hors trace.
    int rang(int col, int row) const;
    // Sens par lequel le flux entre dans cette case. Indefini hors trace.
    ESens entree(int col, int row) const;
    // La case de ce rang, -1 hors bornes. C'est l'ordre du parcours, donc
    // l'ordre dans lequel le flux passera -- et celui dans lequel il faut
    // remplir (TRACE.md, §3, L'ordre de remplissage).
    int caseAuRang(int rang) const;

    // Une piece de ce type fait-elle l'affaire la ou le trace en reclame un
    // autre ? Oui dans un seul cas, et il compte : la CROIX traverse tout
    // droit, donc elle se pose partout ou un droit est demande. C'est ce qui
    // donne aux sept types tires un emploi sur un trace qui n'en reclame que
    // six.
    static bool convient(const ETypePiece &enMain, const ETypePiece &voulu);

    // Combien de cases le trace reclame de chaque ETypePiece, indexe par le
    // type. C'est la mesure qui decide si un trace est bon : un histogramme
    // plat sur les sept types tires veut dire qu'aucune piece n'est perdue.
    QVector<int> histogramme() const;

    // Le type qu'une case impose quand le flux y entre par `entree` et en sort
    // par `sortie`. Public et statique : le futur bot en a autant besoin que le
    // calcul lui-meme.
    static ETypePiece typePour(ESens entree, ESens sortie);

private:
    int largeur = 0;
    int hauteur = 0;
    quint32 graineCalcul = 0;
    int gardeTenue = 0;
    // Les cases dans l'ordre du parcours.
    QVector<int> cases;
    // Une case par octet, indexee row * largeur + col.
    QVector<unsigned char> types;
    QVector<unsigned char> entrees;
    QVector<int> rangs;

    // Le corps de calculer(), une passe de recherche. Rendue a part pour que le
    // repli sans contrainte soit le meme code, appele une seconde fois.
    // `garde` : combien de RANGS du debut doivent etre servis par la main. Le
    // vivier reste la file entiere -- on prend les pieces qui vont, pas les
    // premieres. Zero = recherche libre.
    bool chercher(const Game *plateau, int longueurVisee, long budget,
                  const QVector<ETypePiece> &enMain, int garde,
                  int gestesAvantDepart, int gestesParCase, ESens &premiereEntree);

    // Remplit types/entrees/rangs a partir de `cases`, une fois la recherche
    // finie : le type d'une case se lit sur ses deux voisines du chemin, ce qui
    // evite de le tenir a jour a chaque retour arriere.
    void finaliser(ESens premiereEntree, const Game *plateauFinal);
};

#endif // TRACE_H
