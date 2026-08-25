#include "spritebombe.h"

SpriteBombe::SpriteBombe(ESens sens) : SpriteOriente(sens) {
}

ETypePiece SpriteBombe::getTypePiece() const {
    return tpBombe;
}

QRect SpriteBombe::getImageCoords() const {
    switch (m_sens) {
    case sGauche:
    case sDroite:
        return QRect(QPoint(218, 2), QPoint(243, 27));
    case sHaut:
    case sBas:
        return QRect(QPoint(245, 2), QPoint(270, 27));
    }

    return QRect();
}
