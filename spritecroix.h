#ifndef SPRITECROIX_H
#define SPRITECROIX_H

#include "sprite.h"

class SpriteCroix : public Sprite
{
public:
    SpriteCroix();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITECROIX_H
