#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "dessinpiece.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
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
