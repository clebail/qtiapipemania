#include <QRandomGenerator>
#include <string.h>
#include "game.h"

Game::Game(int largeur, int hauteur) {
    this->largeur = largeur;
    this->hauteur = hauteur;

    map = new unsigned char[largeur*hauteur];
    memset(map, (unsigned char)tpNone, largeur*hauteur*sizeof(unsigned char));

    int x = QRandomGenerator::global()->bounded(1, largeur-1);
    int y = QRandomGenerator::global()->bounded(1, hauteur-1);
    ESens sens = (ESens)QRandomGenerator::global()->bounded((int)sHaut, (int)sDroite + 1);

    setTypePiece(x, y, tpReservoir);
    setSens(x, y, sens);
}

Game::~Game() {
    delete[] map;
}

int Game::getLargeur() const {
    return largeur;
}

int Game::getHauteur() const {
    return hauteur;
}

ETypePiece Game::getTypePiece(int col, int row) const {
    if(col >= 0 && col < largeur && row >= 0 && row < hauteur) {
        return (ETypePiece)(map[row*largeur+col] & 0x0F);
    }

    return tpNone;
}

void Game::setTypePiece(int col, int row, const ETypePiece& typePiece) {
    if(col >= 0 && col < largeur && row >= 0 && row < hauteur) {
        if(getTypePiece(col, row) != tpReservoir) {
            unsigned char& cell = map[row*largeur+col];
            cell = (cell & 0xF0) | (unsigned char)typePiece;
        }
    }
}

ESens Game::getSens(int col, int row) const {
    if(col >= 0 && col < largeur && row >= 0 && row < hauteur) {
        return (ESens)((map[row*largeur+col] >> 4) & 0x03);
    }

    return sHaut;
}

void Game::setSens(int col, int row, const ESens& sens) {
    if(col >= 0 && col < largeur && row >= 0 && row < hauteur) {
        unsigned char& cell = map[row*largeur+col];
        cell = (cell & 0x0F) | ((unsigned char)sens << 4);
    }
}
