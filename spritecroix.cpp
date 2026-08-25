#include "spritecroix.h"

SpriteCroix::SpriteCroix() {
}

ETypePiece SpriteCroix::getTypePiece() const {
    return tpCroix;
}

QRect SpriteCroix::getImageCoords() const {
    return QRect(QPoint(29, 29), QPoint(54, 54));
}
