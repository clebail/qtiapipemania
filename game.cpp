#include <QRandomGenerator>
#include <string.h>
#include "game.h"

// Sans graine imposee, on en tire une du systeme, comme pour la file.
Game::Game(int largeur, int hauteur)
    : Game(largeur, hauteur, QRandomGenerator::securelySeeded().generate()) {
}

Game::Game(int largeur, int hauteur, quint32 seed) : alea(seed), graine(seed) {
    this->largeur = largeur;
    this->hauteur = hauteur;
    size = largeur * hauteur;

    map = new unsigned char[size];

    reinitialiser();
}

Game::Game(const Game& other) : alea(other.alea), graine(other.graine), largeur(other.largeur), hauteur(other.hauteur),
    size(other.size), xDepart(other.xDepart), yDepart(other.yDepart) {

    map = new unsigned char[size];
    memcpy(map, other.map, size * sizeof(*map));
}

Game& Game::operator=(const Game& other) {
    if (this == &other) return *this;
    delete[] map;
    alea = other.alea;
    graine = other.graine;
    largeur = other.largeur;
    hauteur = other.hauteur;
    size = other.size;
    xDepart = other.xDepart;
    yDepart = other.yDepart;

    map = new unsigned char[size];
    memcpy(map, other.map, size * sizeof(*map));

    return *this;
}

// Plateau vide, avec un nouveau reservoir place au hasard et le nombre demande
// de cases infranchissables : un niveau propre.
void Game::reinitialiser(int nbBloquees) {
    memset(map, (unsigned char)tpNone, size*sizeof(*map));

    xDepart = alea.bounded(1, largeur-1);
    yDepart = alea.bounded(1, hauteur-1);
    ESens sens = (ESens)alea.bounded((int)sHaut, (int)sDroite + 1);

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
            int x = alea.bounded(0, largeur);
            int y = alea.bounded(0, hauteur);

            if((x == sortieX && y == sortieY) || getTypePiece(x, y) != tpNone) {
                continue;
            }

            setTypePiece(x, y, tpBloque);
            break;
        }
    }

    // La sortie doit aussi MENER quelque part. Le flux y entre par le cote du
    // reservoir et ne peut donc ressortir que par les trois autres : si tous
    // sont bloques ou hors grille, aucune piece ne sauve la case et la manche
    // est perdue au premier geste, quoi que fasse le joueur. Vu sur
    // --graine 3265344782 --niveau 10 : sortie en (14,9), (14,8) et (14,10)
    // bloquees, bord de grille a droite.
    //
    // On libere alors un voisin bloque. Reparer APRES coup plutot que reserver
    // une case avant : un plateau deja correct ne bouge pas d'un pouce, donc
    // une graine connue reste exactement la meme partie.
    //
    // Ce que ca ne promet pas : que le niveau soit faisable. Un cul-de-sac
    // trois cases plus loin reste possible -- le garantir demanderait de
    // chercher un chemin de la longueur de l'objectif, ce qui est un tout autre
    // travail. Ici on ne corrige que l'impossible immediat.
    static const SDelta voisines[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

    bool issue = false;
    int aLiberer = -1;

    for(int d = 0; d < 4 && !issue; d++) {
        int nx = sortieX + voisines[d].dx;
        int ny = sortieY + voisines[d].dy;

        // Hors grille, ou retour au reservoir : ce n'est pas une issue.
        if(nx < 0 || nx >= largeur || ny < 0 || ny >= hauteur
           || (nx == xDepart && ny == yDepart)) {
            continue;
        }

        if(getTypePiece(nx, ny) == tpBloque) {
            if(aLiberer < 0) {
                aLiberer = ny * largeur + nx;
            }

            continue;
        }

        issue = true;
    }

    if(!issue && aLiberer >= 0) {
        setTypePiece(aLiberer % largeur, aLiberer / largeur, tpNone);
    }
}

void Game::reinitialiser(int nbBloquees, quint32 seed) {
    alea.seed(seed);
    graine = seed;

    reinitialiser(nbBloquees);
}

quint32 Game::getGraine() const {
    return graine;
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

