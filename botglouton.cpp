#include "botglouton.h"

BotGlouton::BotGlouton(Partie *p, float cadence, quint32 seed) : Bot(p, cadence, seed) {
}

void BotGlouton::jouer(float dt) {
    if(!prendreJeton(dt)) {
        return;
    }

    int col, row;
    ESens entree;

    if(!p->ecoulement()->tete(col, row, entree)) {
        // Plus de tete : le trace ne mene nulle part. Rien a construire, on
        // fonce pour derouler la fin de manche.
        demanderFoncer();
        return;
    }

    Piece piece = p->file()->getPiece(0);

    // Tant que la piece du haut n'est pas d'un type compatible avec l'entree
    // de la tete, on la defausse -- pioche apres pioche.
    if(!Ecoulement::piecesCompatibles(entree).contains(piece.type)) {
        defausser();
        return;
    }

    // Piece compatible : on la pose sur la tete. Si sa sortie condamne la
    // manche, on la pose quand meme et on fonce : il n'y a plus rien a
    // construire, autant encaisser ce qui est deja parcouru.
    if(meneALaMort(piece.type, col, row, entree)) {
        demanderFoncer();
    }

    p->poserPiece(col, row);
}
