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

// Plafonds des deux ressources. Ici plutot que dans partie.cpp parce que le
// panneau en a autant besoin que la regle : c'est dessus qu'il calibre ses
// rangees d'icones, et un plafond qui bougerait sans que l'affichage suive
// donnerait une rangee a moitie vide ou des icones qui debordent.
//
// Dix, et non neuf : le panneau les montre sur DEUX rangees de cinq
// (WPanneau::dessinerVies), ce qui double la taille des icones a largeur de
// panneau egale. Le plafond vaut donc exactement 5 x 2, et wpanneau.cpp le
// verifie a la compilation.
#define VIES_MAX                    10
#define BOMBES_MAX                  10

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
    // Manche perdue et fin de partie sont deux choses distinctes depuis les
    // vies : la premiere coute une vie et rejoue le MEME niveau, la seconde
    // seule remet les compteurs a zero. Les confondre -- ce que faisait
    // epPerdue -- rendait la vie impossible a depenser.
    epPerdue,       // manche perdue : le flux s'est arrete trop tot, il reste des vies
    epGameOver      // plus de vie : la partie est finie
} EEtatPartie;

#endif // COMMON_H
