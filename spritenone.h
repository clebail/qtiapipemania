#ifndef SPRITENONE_H
#define SPRITENONE_H

#include "sprite.h"

class SpriteNone : public Sprite
{
public:
    SpriteNone();

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITENONE_H
