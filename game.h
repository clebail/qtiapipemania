#ifndef GAME_H
#define GAME_H

#include "common.h"

class Game
{
public:
    Game(int largeur, int hauteur);
    ~Game();

    int getLargeur() const;
    int getHauteur() const;
    ETypePiece getTypePiece(int col, int row) const;
    void setTypePiece(int col, int row, const ETypePiece& typePiece);
    ESens getSens(int col, int row) const;
    void setSens(int col, int row, const ESens& sens);

private:
    int largeur;
    int hauteur;
    // Chaque case est un octet : bits 0-3 = ETypePiece, bits 4-5 = ESens.
    unsigned char* map = nullptr;
};

#endif // GAME_H
