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
    int getSize() const;
    ETypePiece getTypePiece(int col, int row) const;
    void setTypePiece(int col, int row, const ETypePiece& typePiece);
    ESens getSens(int col, int row) const;
    ESens getSens(int idx) const;
    void setSens(int col, int row, const ESens& sens);
    int getIdxDepart() const;
    // Temporaire, pour tester flood() plus vite sans poser 200+ pieces a la main.
    void genererReseauTest();
private:
    int largeur;
    int hauteur;
    int size;
    // Chaque case est un octet : bits 0-3 = ETypePiece, bits 4-5 = ESens.
    unsigned char* map = nullptr;
    int xDepart;
    int yDepart;
};

#endif // GAME_H
