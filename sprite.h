#ifndef SPRITE_H
#define SPRITE_H

#include <QRect>
#include "common.h"

class Sprite
{
public:
    Sprite();
    virtual ~Sprite() {}

    virtual ETypePiece getTypePiece() const = 0;
    virtual QRect getImageCoords() const = 0;

    static Sprite* create(ETypePiece type, ESens sens = sHaut);
};

#endif // SPRITE_H
