#include "spritecoudehautgauche.h"

SpriteCoudeHautGauche::SpriteCoudeHautGauche() {
}

ETypePiece SpriteCoudeHautGauche::getTypePiece() const {
    return tpCoudeHautGauche;
}

QRect SpriteCoudeHautGauche::getImageCoords() const {
    return QRect(QPoint(56, 2), QPoint(81, 27));
}
