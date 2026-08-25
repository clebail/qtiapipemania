#ifndef SPRITECOUDEBASDROITE_H
#define SPRITECOUDEBASDROITE_H

#include "sprite.h"

class SpriteCoudeBasDroite : public Sprite
{
public:
    SpriteCoudeBasDroite();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITECOUDEBASDROITE_H
