#include "spritevertical.h"

SpriteVertical::SpriteVertical() {
}

ETypePiece SpriteVertical::getTypePiece() const {
    return tpVertical;
}

QRect SpriteVertical::getImageCoords() const {
    return QRect(QPoint(2, 2), QPoint(27, 27));
}


