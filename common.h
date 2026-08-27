#ifndef COMMON_H
#define COMMON_H

// Dimensions du plateau. Elles disent aussi la place que la fenetre reclame a
// l'ecran, d'ou leur presence ici : tailleCase() en a besoin.
#define COLONNES_PLATEAU            15
#define LIGNES_PLATEAU              15

// Les pieces sont tracees en vectoriel : aucune image source ne fixe la taille
// d'une case, on peut donc la choisir a l'execution.
#define TAILLE_CASE_MAX             54
#define TAILLE_CASE_MIN             24

// Cote d'une case pour cette session : TAILLE_CASE_MAX quand l'ecran est assez
// grand, juste ce qu'il faut de moins sinon. Le layout de la fenetre est fige
// (SetFixedSize) : ce qui deborde n'est pas seulement coupe, il devient
// inatteignable -- sur un portable, c'est la rangee des boutons de bot qui
// passe sous le bord de l'ecran, et les bots avec.
int tailleCase();

typedef enum _ETypePiece {
    tpNone, tpReservoir, tpHorizontal, tpVertical, tpCoudeHautGauche, tpCoudeHautDroite, tpCoudeBasGauche, tpCoudeBasDroite, tpCroix, tpBombe, tpBloque
} ETypePiece;

typedef enum _ESens {
    sHaut, sBas, sGauche, sDroite
} ESens;

typedef struct _SDelta {
    int dx;
    int dy;
} SDelta;

typedef enum _EEtat {
    eEnCours, eTermine, eFuite
} EEtat;

typedef enum _EEtatPartie {
    epAttente,      // le joueur pose ses tuyaux, le flux n'est pas encore parti
    epEcoulement,   // le flux progresse
    epReussie,      // manche gagnee : longueur minimale atteinte
    epPerdue        // manche perdue : le flux s'est arrete trop tot
} EEtatPartie;

#endif // COMMON_H
