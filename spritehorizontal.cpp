#include "spritehorizontal.h"

SpriteHorizontal::SpriteHorizontal() {
}

ETypePiece SpriteHorizontal::getTypePiece() const {
    return tpHorizontal;
}

QRect SpriteHorizontal::getImageCoords() const {
    return QRect(QPoint(2, 29), QPoint(27, 54));
}
