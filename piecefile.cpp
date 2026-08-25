#include <QRandomGenerator>
#include "piecefile.h"

PieceFile::PieceFile(int taille) : taille(taille) {
    for(int i=0;i<taille;i++) {
        pieces.append(genererPiece());
    }
}

int PieceFile::getTaille() const {
    return taille;
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

Piece PieceFile::genererPiece() {
    ETypePiece type = (ETypePiece)QRandomGenerator::global()->bounded((int)tpHorizontal, (int)tpBombe + 1);
    ESens sens = (ESens)QRandomGenerator::global()->bounded((int)sHaut, (int)sDroite + 1);

    return { type, sens };
}
