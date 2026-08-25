#ifndef SPRITEBOMBE_H
#define SPRITEBOMBE_H

#include "spriteoriente.h"

class SpriteBombe : public SpriteOriente
{
public:
    SpriteBombe(ESens sens);

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITEBOMBE_H
