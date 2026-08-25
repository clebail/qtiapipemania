#include "spritenone.h"

SpriteNone::SpriteNone() {
}

ETypePiece SpriteNone::getTypePiece() const {
    return tpNone;
}

QRect SpriteNone::getImageCoords() const {
    return QRect(QPoint(29, 2), QPoint(54, 27));
}
