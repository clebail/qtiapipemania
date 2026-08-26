#ifndef PIECEFILE_H
#define PIECEFILE_H

#include <QVector>
#include "common.h"

// Nombre de pieces visibles dans la file.
#define FILE_SIZE       5

struct Piece {
    ETypePiece type;
    ESens sens;
};

class PieceFile
{
public:
    explicit PieceFile(int taille);

    int getTaille() const;
    Piece getPiece(int index) const;
    Piece depiler();

private:
    int taille;
    QVector<Piece> pieces;

    static Piece genererPiece();
};

#endif // PIECEFILE_H
