#ifndef SPRITECOUDEBASGAUCHE_H
#define SPRITECOUDEBASGAUCHE_H

#include "sprite.h"

class SpriteCoudeBasGauche : public Sprite
{
public:
    SpriteCoudeBasGauche();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITECOUDEBASGAUCHE_H
