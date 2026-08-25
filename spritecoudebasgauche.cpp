#include "spritecoudebasgauche.h"

SpriteCoudeBasGauche::SpriteCoudeBasGauche() {
}

ETypePiece SpriteCoudeBasGauche::getTypePiece() const {
    return tpCoudeBasGauche;
}

QRect SpriteCoudeBasGauche::getImageCoords() const {
    return QRect(QPoint(56, 29), QPoint(81, 54));
}
