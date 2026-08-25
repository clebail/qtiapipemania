#include "spritereservoir.h"

SpriteReservoir::SpriteReservoir(ESens sens) : SpriteOriente(sens) {
}

ETypePiece SpriteReservoir::getTypePiece() const {
    return tpReservoir;
}

QRect SpriteReservoir::getImageCoords() const {
    switch (m_sens) {
    case sHaut:
        return QRect(QPoint(137, 29), QPoint(162, 54));
    case sBas:
        return QRect(QPoint(110, 2), QPoint(135, 27));
    case sGauche:
        return QRect(QPoint(137, 2), QPoint(162, 27));
    case sDroite:
        return QRect(QPoint(110, 29), QPoint(135, 54));
    }
    return QRect();
}
