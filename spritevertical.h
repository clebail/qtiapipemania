#ifndef SPRITEVERTICAL_H
#define SPRITEVERTICAL_H

#include "sprite.h"

class SpriteVertical : public Sprite
{
public:
    SpriteVertical();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITEVERTICAL_H
