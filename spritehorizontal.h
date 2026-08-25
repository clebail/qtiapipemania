#ifndef SPRITEHORIZONTAL_H
#define SPRITEHORIZONTAL_H

#include "sprite.h"

class SpriteHorizontal : public Sprite
{
public:
    SpriteHorizontal();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITEHORIZONTAL_H
