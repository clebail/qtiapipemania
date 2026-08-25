#include "spritecoudebasdroite.h"

SpriteCoudeBasDroite::SpriteCoudeBasDroite() {
}

ETypePiece SpriteCoudeBasDroite::getTypePiece() const {
    return tpCoudeBasDroite;
}

QRect SpriteCoudeBasDroite::getImageCoords() const {
    return QRect(QPoint(83, 29), QPoint(108, 54));
}
