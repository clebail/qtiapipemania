#include <QRandomGenerator>
#include <string.h>
#include "game.h"

// Reflete un coude selon un miroir horizontal et/ou vertical (les autres
// types sont symetriques, donc inchanges par un miroir).
static ETypePiece coudeMiroir(ETypePiece type, bool miroirX, bool miroirY) {
    ESens vertical, horizontal;

    switch(type) {
    case tpCoudeHautGauche: vertical = sHaut; horizontal = sGauche; break;
    case tpCoudeHautDroite: vertical = sHaut; horizontal = sDroite; break;
    case tpCoudeBasGauche:  vertical = sBas;  horizontal = sGauche; break;
    case tpCoudeBasDroite:  vertical = sBas;  horizontal = sDroite; break;
    default: return type;
    }

    if(miroirY) {
        vertical = (vertical == sHaut) ? sBas : sHaut;
    }
    if(miroirX) {
        horizontal = (horizontal == sGauche) ? sDroite : sGauche;
    }

    if(vertical == sHaut) {
        return (horizontal == sGauche) ? tpCoudeHautGauche : tpCoudeHautDroite;
    }
    return (horizontal == sGauche) ? tpCoudeBasGauche : tpCoudeBasDroite;
}

Game::Game(int largeur, int hauteur) {
    this->largeur = largeur;
    this->hauteur = hauteur;
    size = largeur * hauteur;

    map = new unsigned char[size];
    memset(map, (unsigned char)tpNone, size*sizeof(*map));

    xDepart = QRandomGenerator::global()->bounded(1, largeur-1);
    yDepart = QRandomGenerator::global()->bounded(1, hauteur-1);
    ESens sens = (ESens)QRandomGenerator::global()->bounded((int)sHaut, (int)sDroite + 1);

    setTypePiece(xDepart, yDepart, tpReservoir);
    setSens(xDepart, yDepart, sens);
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

void Game::genererReseauTest() {
    memset(map, (unsigned char)tpNone, size*sizeof(*map));


    // Le serpentin est toujours calcule comme s'il partait du coin haut-gauche
    // (coordonnees "canoniques"), puis un miroir aleatoire est applique au
    // moment de poser chaque piece : ca fait demarrer le reservoir dans un
    // coin different a chaque appel sans avoir a reecrire la geometrie.
    bool miroirX = QRandomGenerator::global()->bounded(2) == 0;
    bool miroirY = QRandomGenerator::global()->bounded(2) == 0;
    auto mx = [&](int x) { return miroirX ? (largeur-1-x) : x; };
    auto my = [&](int y) { return miroirY ? (hauteur-1-y) : y; };

    ESens sensReservoir = miroirX ? sGauche : sDroite;
    int xReservoir = mx(0);
    int yReservoir = my(0);

    xDepart = xReservoir;
    yDepart = yReservoir;
    map[yReservoir*largeur+xReservoir] = (unsigned char)tpReservoir | ((unsigned char)sensReservoir << 4);

    for(int y=0;y<hauteur;y++) {
        bool gaucheADroite = (y % 2 == 0);
        int xDebut = gaucheADroite ? 0 : largeur-1;
        int xFin = gaucheADroite ? largeur-1 : 0;
        int pas = gaucheADroite ? 1 : -1;

        for(int x=xDebut; gaucheADroite ? (x<=xFin) : (x>=xFin); x+=pas) {
            if(x == 0 && y == 0) {
                continue;
            }

            bool debutDeLigne = (x == xDebut && y > 0);
            bool finDeLigne = (x == xFin && y < hauteur-1);
            ETypePiece type;

            if(debutDeLigne) {
                type = gaucheADroite ? tpCoudeHautDroite : tpCoudeHautGauche;
            } else if(finDeLigne) {
                type = gaucheADroite ? tpCoudeBasGauche : tpCoudeBasDroite;
            } else {
                type = tpHorizontal;
            }

            setTypePiece(mx(x), my(y), coudeMiroir(type, miroirX, miroirY));
        }
    }

    // Chaine verticale de croix (coordonnees canoniques) : leurs ouvertures
    // Haut/Bas se repondent d'une croix a l'autre, en plus des ouvertures
    // Gauche/Droite deja utilisees par les lignes du serpentin. Les croix du
    // milieu ont donc leurs 4 branches reliees au reseau.
    //
    // Les 2 croix aux extremites gardent forcement une branche morte : seule
    // une autre croix peut alimenter verticalement une croix (il faudrait une
    // piece {Bas,Gauche,Droite}, le "te", qui n'existe pas dans le jeu), et
    // pousser la chaine jusqu'au bord transformerait ces branches en fuites.
    // La chaine reste donc dans les lignes interieures.
    int longueurMax = qMin(6, hauteur-2);
    if(largeur >= 3 && longueurMax >= 3) {
        int longueur = QRandomGenerator::global()->bounded(3, longueurMax+1);
        int xCroix = QRandomGenerator::global()->bounded(1, largeur-1);
        int yCroix = QRandomGenerator::global()->bounded(1, hauteur-longueur);

        for(int i=0;i<longueur;i++) {
            setTypePiece(mx(xCroix), my(yCroix+i), tpCroix);
        }
    }
}
