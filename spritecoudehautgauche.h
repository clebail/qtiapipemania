#ifndef SPRITECOUDEHAUTGAUCHE_H
#define SPRITECOUDEHAUTGAUCHE_H

#include "sprite.h"

class SpriteCoudeHautGauche : public Sprite
{
public:
    SpriteCoudeHautGauche();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITECOUDEHAUTGAUCHE_H
