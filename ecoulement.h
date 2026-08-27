#ifndef ECOULEMENT_H
#define ECOULEMENT_H

#include <QVector>
#include <QtGlobal>
#include "game.h"
#include "common.h"

// Une case peut porter deux flux, jamais plus : la croix est le seul tuyau qui
// se laisse traverser deux fois, une fois par axe, et ses deux conduites sont
// independantes. Toutes les autres pieces n'occupent que l'axe 0.
#define NB_AXES 2
#define AXE_HORIZONTAL 0
#define AXE_VERTICAL 1

class Ecoulement
{
public:
    Ecoulement(const Game *plateau);
    ~Ecoulement();

    void reinitialiser();
    void demarrer();
    EEtat avancer(float dt);
    void setDureeRemplissage(float secondes);
    // Secondes que le flux met a traverser une case. Le bot en a besoin pour
    // convertir une avance en cases en un nombre de gestes qu'il pourra encore
    // jouer avant que le flux ne le rattrape.
    float getDureeRemplissage() const;
    bool enCours() const;
    int nbCasesTraversees() const;
    // Combien de cases le flux traverserait encore si on le laissait continuer
    // sur le plateau actuel. Ne modifie rien : sert a mesurer l'avance prise
    // par le joueur au moment ou la manche s'arrete.
    int casesEnAval() const;
    // Vrai des qu'un des deux axes est pris : on ne pose plus rien sur une case
    // ou le fluide est passe, meme si l'autre conduite d'une croix est libre.
    bool estRempli(int col, int row) const;
    // Etat d'une des deux conduites de la case. Le rendu les parcourt toutes
    // les deux : une croix traversee deux fois dessine deux liquides.
    float progression(int col, int row, int axe) const;
    ESens entree(int col, int row, int axe) const;
    // Conduite empruntee en entrant par ce cote : les deux branches d'une croix
    // sont distinctes, tout le reste tient sur l'axe 0.
    static int axe(const ETypePiece& typePiece, const ESens& entree);
    // Case libre au bout du tuyau pose, et cote par lequel le flux y entrera :
    // le seul endroit ou poser une piece prolonge le trace. Se calcule depuis le
    // reservoir et ne regarde que le plateau, donc repond aussi avant le depart
    // du flux -- c'est justement la que le joueur construit le plus.
    // Renvoie false quand le tuyau ne mene nulle part : bord de grille, case
    // bloquee, piece sans ouverture en face, ou circuit referme sur lui-meme.
    bool tete(int& col, int& row, ESens& entree) const;
    // Publique et statique : le rendu en a besoin pour connaitre les sorties
    // d'une case. Si un troisieme utilisateur apparait, l'extraire dans un
    // module de geometrie de tuyaux.
    static QVector<ESens> ouvertures(const ETypePiece& typePiece, const ESens& sens);
    // Cotes par lesquels le flux peut ressortir, sachant par ou il est entre.
    static QVector<ESens> sorties(const ETypePiece& typePiece, const ESens& sens, const ESens& entree);
    // Les quatre pieces qui bouchent la tete de construction quand le flux y
    // entre par ce cote : le tuyau droit du bon axe, les deux coudes qui
    // touchent ce cote, et la croix. Toute autre piece laisse le bout ouvert.
    // Purement geometrique : ne regarde ni le plateau ni le sens pose (coudes et
    // croix ont un sens neutre), donc c'est une table figee.
    static QVector<ETypePiece> piecesCompatibles(const ESens& entree);
    // Geometrie pure : depuis une case et un cote de sortie, la case voisine et
    // le cote par lequel le flux y entrerait. Ne regarde pas le plateau -- sert
    // a projeter un coup qui n'est pas encore pose (case voisine hors grille
    // possible, a l'appelant de la borner).
    static void voisine(int col, int row, ESens sortie, int& vCol, int& vRow, ESens& vEntree);
    static int longueurTracee(const Game *plateau);
    // Pas elementaire du parcours : depuis une case et son cote d'entree, la
    // case suivante du tuyau. Renvoie false si la piece n'offre pas de sortie ou
    // si celle-ci donne hors grille. Ne juge pas de la case d'arrivee : c'est a
    // l'appelant de dire si elle prolonge le trace ou l'interrompt.
    //
    // Publique parce qu'un bot remonte le trace pour trouver la case qui le
    // bloque, et qu'il doit le faire avec le pas du moteur. La refaire chez lui
    // serait rouvrir la porte que le v2 avait fermee : un bot qui raisonne sur
    // la geometrie des tuyaux finit par diverger de celui qui les fait couler.
    bool suivante(int col, int row, ESens entree, int& suivCol, int& suivRow,
                  ESens& suivEntree) const;
private:
    // Non copiable, a la difference de Game : possede ses tableaux par pointeur
    // nu, et une copie implicite les libererait deux fois. Sonder un coup se
    // fait en branchant un Ecoulement neuf sur une copie de plateau, pas en
    // dupliquant celui de la partie.
    Q_DISABLE_COPY(Ecoulement)

    const Game *plateau;
    // Indexes par case * NB_AXES + axe, et non par case : c'est ce qui permet a
    // la croix de porter deux flux sans que l'un efface l'autre.
    unsigned char* remplis = nullptr;
    float* progressions = nullptr;
    unsigned char* entrees = nullptr;
    // Conduites en cours de remplissage, dans la meme indexation.
    QVector<int> front;
    int casesTraversees = 0;
    // Secondes pour remplir une case. Baisser cette valeur accelere
    // l'ecoulement, sans rien changer d'autre au reste du modele.
    float dureeRemplissage = 0.3f;
};

#endif // ECOULEMENT_H
