#include "spritecoudehautdroite.h"

SpriteCoudeHautDroite::SpriteCoudeHautDroite() {
}

ETypePiece SpriteCoudeHautDroite::getTypePiece() const {
    return tpCoudeHautDroite;
}

QRect SpriteCoudeHautDroite::getImageCoords() const {
    return QRect(QPoint(83, 2), QPoint(108, 27));
}
