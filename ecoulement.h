#ifndef ECOULEMENT_H
#define ECOULEMENT_H

#include <QVector>
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
    // Publique et statique : le rendu en a besoin pour connaitre les sorties
    // d'une case. Si un troisieme utilisateur apparait, l'extraire dans un
    // module de geometrie de tuyaux.
    static QVector<ESens> ouvertures(const ETypePiece& typePiece, const ESens& sens);
    // Cotes par lesquels le flux peut ressortir, sachant par ou il est entre.
    static QVector<ESens> sorties(const ETypePiece& typePiece, const ESens& sens, const ESens& entree);
private:
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
