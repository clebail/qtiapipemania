#ifndef COMMON_H
#define COMMON_H

// Les pieces sont tracees en vectoriel : une seule taille de case suffit, sans
// facteur d'echelle lie a une image source.
#define TAILLE_CASE                 54

typedef enum _ETypePiece {
    tpNone, tpReservoir, tpHorizontal, tpVertical, tpCoudeHautGauche, tpCoudeHautDroite, tpCoudeBasGauche, tpCoudeBasDroite, tpCroix, tpBombe
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
