#include "piecefile.h"

// Sans graine imposee, on en tire une du systeme : le jeu reste imprevisible,
// et la partie reste rejouable a l'identique si on retient cette graine.
PieceFile::PieceFile(int taille)
    : PieceFile(taille, QRandomGenerator::securelySeeded().generate()) {
}

// La graine est posee dans la liste d'initialisation, donc avant que populate()
// ne tire quoi que ce soit : les premieres pieces de la file en dependent.
PieceFile::PieceFile(int taille, quint32 seed) : taille(taille), graine(seed), alea(seed) {
    populate();
}

void PieceFile::reinitialiser(quint32 seed) {
    graine = seed;
    alea.seed(seed);

    pieces.clear();
    populate();
}

int PieceFile::getTaille() const {
    return taille;
}

quint32 PieceFile::getGraine() const {
    return graine;
}

Piece PieceFile::getPiece(int index) const {
    if(index >= 0 && index < pieces.size()) {
        return pieces[index];
    }

    return { tpNone, sHaut };
}

Piece PieceFile::depiler() {
    Piece piece = pieces.first();
    pieces.removeFirst();
    pieces.append(genererPiece());

    return piece;
}

void PieceFile::populate() {
    for(int i=0;i<taille;i++) {
        pieces.append(genererPiece());
    }
}

Piece PieceFile::genererPiece() {
    // Borne haute a tpCroix : la bombe est volontairement hors du tirage tant
    // qu'elle n'est pas geree par l'ecoulement.
    ETypePiece type = (ETypePiece)alea.bounded((int)tpHorizontal, (int)tpCroix + 1);
    ESens sens = (ESens)alea.bounded((int)sHaut, (int)sDroite + 1);

    return { type, sens };
}
