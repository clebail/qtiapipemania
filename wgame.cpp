#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "dessinpiece.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
    // Sans cela Qt n'envoie mouseMoveEvent que bouton enfonce.
    setMouseTracking(true);
}

// Case sous une position en pixels, ou (-1,-1) si le point tombe hors grille.
QPoint WGame::caseSous(const QPoint& position) const {
    if(partie == nullptr) {
        return QPoint(-1, -1);
    }

    Game *plateau = partie->plateau();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int px = position.x() - (size().width() - plateau->getLargeur() * spriteW) / 2;
    int py = position.y() - (size().height() - plateau->getHauteur() * spriteH) / 2;

    // Borne en pixels et non en cases : la division entiere tronque vers zero,
    // donc -40 / 54 vaut 0 et un point a gauche de la grille retomberait sur la
    // colonne 0 au lieu d'etre rejete.
    if(px < 0 || py < 0
       || px >= plateau->getLargeur() * spriteW
       || py >= plateau->getHauteur() * spriteH) {
        return QPoint(-1, -1);
    }

    return QPoint(px / spriteW, py / spriteH);
}

void WGame::mouseMoveEvent(QMouseEvent *event) {
    QPoint survolee = caseSous(event->pos());

    if(survolee != caseSurvolee) {
        caseSurvolee = survolee;
        update();
    }
}

void WGame::leaveEvent(QEvent *) {
    if(caseSurvolee.x() >= 0) {
        caseSurvolee = QPoint(-1, -1);
        update();
    }
}

void WGame::setPartie(Partie *partie) {
    this->partie = partie;
    repaint();
}

int WGame::spriteWidth() const {
    return TAILLE_CASE;
}

int WGame::spriteHeight() const {
    return TAILLE_CASE;
}

// Curseur en equerres, facon viseur : quatre coins plutot qu'un cadre plein,
// pour ne pas masquer le tuyau de la case.
static void dessinerCurseur(QPainter& painter, const QRectF& tuile, const QColor& couleur) {
    qreal retrait = tuile.width() * 0.08;
    qreal branche = tuile.width() * 0.26;
    QRectF r = tuile.adjusted(retrait, retrait, -retrait, -retrait);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 40));
    painter.drawRect(r);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(couleur, qMax(2.0, tuile.width() * 0.045), Qt::SolidLine, Qt::FlatCap));

    const QPointF coins[4] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
    const QPointF sensH[4] = { QPointF(branche, 0), QPointF(-branche, 0),
                               QPointF(-branche, 0), QPointF(branche, 0) };
    const QPointF sensV[4] = { QPointF(0, branche), QPointF(0, branche),
                               QPointF(0, -branche), QPointF(0, -branche) };

    for(int i=0;i<4;i++) {
        painter.drawLine(coins[i], coins[i] + sensH[i]);
        painter.drawLine(coins[i], coins[i] + sensV[i]);
    }
}

void WGame::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    if(partie == nullptr) {
        return;
    }

    Game *plateau = partie->plateau();
    Ecoulement *ecoul = partie->ecoulement();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - plateau->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - plateau->getHauteur() * spriteH) / 2;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Deux passages : toutes les pieces, puis tout le liquide. Ca evite qu'une
    // case repeigne son fond par-dessus le liquide de sa voisine.
    for(int y=0;y<plateau->getHauteur();y++) {
        for(int x=0;x<plateau->getLargeur();x++) {
            QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
            dessinerPiece(painter, dest, plateau->getTypePiece(x, y), plateau->getSens(x, y));
        }
    }

    for(int y=0;y<plateau->getHauteur();y++) {
        for(int x=0;x<plateau->getLargeur();x++) {
            float p = ecoul->progression(x, y);

            if(p > 0.0f) {
                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
                dessinerLiquide(painter, dest, plateau->getTypePiece(x, y), plateau->getSens(x, y),
                                ecoul->entree(x, y), p);
            }
        }
    }

    // Curseur en dernier, pour rester au-dessus des tuyaux et du liquide. Sa
    // couleur annonce si le coup serait accepte.
    if(caseSurvolee.x() >= 0) {
        static const QColor cPermis(0x6e, 0xd8, 0xff);
        static const QColor cRefuse(0xd8, 0x50, 0x40);

        QRectF dest(caseSurvolee.x()*spriteW + margeX, caseSurvolee.y()*spriteH + margeY,
                    spriteW, spriteH);
        dessinerCurseur(painter, dest,
                        partie->peutPoser(caseSurvolee.x(), caseSurvolee.y()) ? cPermis : cRefuse);
    }
}

void WGame::mouseReleaseEvent(QMouseEvent *event) {
    if(partie == nullptr) {
        return;
    }

    Game *plateau = partie->plateau();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - plateau->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - plateau->getHauteur() * spriteH) / 2;
    int px = event->pos().x() - margeX;
    int py = event->pos().y() - margeY;

    // Un relachement hors de la grille est possible : Qt livre l'evenement au
    // widget qui a recu l'appui, meme si le curseur en est sorti. On borne en
    // pixels et non en cases, car la division entiere tronque vers zero :
    // -40 / 54 vaut 0, donc un clic a gauche de la grille retomberait sur la
    // colonne 0 au lieu d'etre rejete.
    if(px < 0 || py < 0
       || px >= plateau->getLargeur() * spriteW
       || py >= plateau->getHauteur() * spriteH) {
        return;
    }

    // Les regles du coup (case interdite, penalite de remplacement) sont dans
    // Partie : le widget ne fait que traduire un clic en coordonnees.
    if(partie->poserPiece(px / spriteW, py / spriteH)) {
        repaint();
        emit pieceDeposee();
    }
}
