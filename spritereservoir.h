#ifndef SPRITERESERVOIR_H
#define SPRITERESERVOIR_H

#include "spriteoriente.h"

class SpriteReservoir : public SpriteOriente
{
public:
    SpriteReservoir(ESens sens);

    virtual ETypePiece getTypePiece() const;
    virtual QRect getImageCoords() const;
};

#endif // SPRITERESERVOIR_H
