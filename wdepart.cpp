#include <QPainter>
#include "wdepart.h"
#include "common.h"

static const QColor cFond   (0x0b, 0x0b, 0x14);
static const QColor cCadre  (0x3a, 0x3a, 0xc8);
static const QColor cCreux  (0x08, 0x08, 0x16);

static const qreal MARGE = 4.0;
static const qreal ARRONDI = 3.0;

WDepart::WDepart(QWidget *parent) : QWidget{parent} {
    // Une demi-case de large, pour rester dans la trame du jeu.
    setFixedWidth(tailleCase() / 2);
}

void WDepart::setFraction(float fraction) {
    this->fraction = qBound(0.0f, fraction, 1.0f);
    update();
}

void WDepart::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), cFond);

    QRectF creux = QRectF(rect()).adjusted(MARGE, MARGE, -MARGE, -MARGE);

    painter.setPen(QPen(cCadre, 1.5));
    painter.setBrush(cCreux);
    painter.drawRoundedRect(creux, ARRONDI, ARRONDI);

    if(fraction <= 0.0f) {
        return;
    }

    // Du vert au rouge a mesure que le temps s'epuise : la teinte va de 0.33
    // (vert) a 0 (rouge) proportionnellement a ce qu'il reste.
    QColor couleur = QColor::fromHsvF(0.33 * fraction, 0.85, 0.95);

    qreal hauteur = creux.height() * fraction;
    QRectF jauge(creux.left() + 1.5, creux.bottom() - hauteur + 1.5,
                 creux.width() - 3.0, hauteur - 3.0);

    if(jauge.height() <= 0.0) {
        return;
    }

    painter.setPen(Qt::NoPen);
    painter.setBrush(couleur);
    painter.drawRoundedRect(jauge, ARRONDI - 1.0, ARRONDI - 1.0);

    // Reflet vertical, dans le meme esprit que celui des tuyaux.
    painter.setBrush(QColor(255, 255, 255, 60));
    painter.drawRoundedRect(QRectF(jauge.left() + 1.5, jauge.top() + 1.5,
                                   jauge.width() * 0.28, jauge.height() - 3.0),
                            1.5, 1.5);
}
