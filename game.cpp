#include <QRandomGenerator>
#include <string.h>
#include "game.h"

Game::Game(int largeur, int hauteur) {
    this->largeur = largeur;
    this->hauteur = hauteur;
    size = largeur * hauteur;

    map = new unsigned char[size];

    reinitialiser();
}

// Plateau vide, avec un nouveau reservoir place au hasard et le nombre demande
// de cases infranchissables : un niveau propre.
void Game::reinitialiser(int nbBloquees) {
    memset(map, (unsigned char)tpNone, size*sizeof(*map));

    xDepart = QRandomGenerator::global()->bounded(1, largeur-1);
    yDepart = QRandomGenerator::global()->bounded(1, hauteur-1);
    ESens sens = (ESens)QRandomGenerator::global()->bounded((int)sHaut, (int)sDroite + 1);

    setTypePiece(xDepart, yDepart, tpReservoir);
    setSens(xDepart, yDepart, sens);

    // La case devant la sortie du reservoir reste libre : la bloquer rendrait
    // la manche perdue d'avance.
    int sortieX = xDepart;
    int sortieY = yDepart;

    switch(sens) {
    case sHaut:   sortieY--; break;
    case sBas:    sortieY++; break;
    case sGauche: sortieX--; break;
    case sDroite: sortieX++; break;
    }

    for(int posees=0; posees<nbBloquees; posees++) {
        // Quelques essais suffisent : les cases libres sont largement
        // majoritaires, et rater un obstacle est sans consequence.
        for(int essai=0; essai<50; essai++) {
            int x = QRandomGenerator::global()->bounded(0, largeur);
            int y = QRandomGenerator::global()->bounded(0, hauteur);

            if((x == sortieX && y == sortieY) || getTypePiece(x, y) != tpNone) {
                continue;
            }

            setTypePiece(x, y, tpBloque);
            break;
        }
    }
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

int Game::getSize() const {
    return size;
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

ESens Game::getSens(int idx) const {
    return getSens(idx % largeur, idx / largeur);
}

void Game::setSens(int col, int row, const ESens& sens) {
    if(col >= 0 && col < largeur && row >= 0 && row < hauteur) {
        unsigned char& cell = map[row*largeur+col];
        cell = (cell & 0x0F) | ((unsigned char)sens << 4);
    }
}

int Game::getIdxDepart() const {
    return yDepart * largeur + xDepart;
}
