#ifndef SPRITECOUDEHAUTDROITE_H
#define SPRITECOUDEHAUTDROITE_H

#include "sprite.h"

class SpriteCoudeHautDroite : public Sprite
{
public:
    SpriteCoudeHautDroite();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITECOUDEHAUTDROITE_H
