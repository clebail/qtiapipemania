#ifndef GAME_H
#define GAME_H

#include <QRandomGenerator>
#include "common.h"

class Game
{
public:
    Game(int largeur, int hauteur);
    Game(int largeur, int hauteur, quint32 seed);
    // Seule des trois classes de simulation a etre copiable, et c'est voulu :
    // un bot sonde un coup en posant sa piece sur une copie du plateau, puis
    // jette la copie. Rien a restaurer, donc rien a oublier de restaurer.
    Game(const Game& other);
    Game& operator=(const Game& other);
    ~Game();

    // Nouveau plateau tire avec la graine courante.
    void reinitialiser(int nbBloquees = 0);
    // Idem, mais en repartant de la graine donnee : c'est ce qui permet de
    // rejouer une manche a l'identique, quel que soit ce qui l'a precedee.
    void reinitialiser(int nbBloquees, quint32 seed);
    // Graine du dernier tirage de plateau.
    quint32 getGraine() const;

    int getLargeur() const;
    int getHauteur() const;
    int getSize() const;
    ETypePiece getTypePiece(int col, int row) const;
    void setTypePiece(int col, int row, const ETypePiece& typePiece);
    ESens getSens(int col, int row) const;
    ESens getSens(int idx) const;
    void setSens(int col, int row, const ESens& sens);
    int getIdxDepart() const;
private:
    // Generateur propre au plateau : le placement du reservoir et des cases
    // bloquees ne doit pas dependre du nombre de pieces deja tirees ailleurs.
    QRandomGenerator alea;
    quint32 graine;
    int largeur;
    int hauteur;
    int size;
    // Chaque case est un octet : bits 0-3 = ETypePiece, bits 4-5 = ESens.
    unsigned char* map = nullptr;
    int xDepart;
    int yDepart;
};

#endif // GAME_H
