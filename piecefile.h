#ifndef PIECEFILE_H
#define PIECEFILE_H

#include <QVector>
#include <QRandomGenerator>
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
    explicit PieceFile(int taille, quint32 seed);

    // Repart d'une file neuve, tiree avec la graine donnee.
    void reinitialiser(quint32 seed);

    int getTaille() const;
    // Graine du tirage : a retenir pour rejouer exactement la meme file.
    quint32 getGraine() const;
    Piece getPiece(int index) const;
    Piece depiler();

private:
    int taille;
    quint32 graine;
    QVector<Piece> pieces;
    // Generateur propre a la file : deux PieceFile menent leur tirage chacune
    // de leur cote, ce qui permet de rejouer une partie a l'identique meme si
    // une autre vit dans le meme process.
    QRandomGenerator alea;

    void populate();

    Piece genererPiece();
};

#endif // PIECEFILE_H
