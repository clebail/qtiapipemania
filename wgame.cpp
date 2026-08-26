#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "dessinpiece.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
}

void WGame::setGame(Game *game) {
    this->game = game;
    repaint();
}

void WGame::setEcoulement(Ecoulement *ecoulement) {
    this->ecoulement = ecoulement;
    repaint();
}

void WGame::setPieceFile(PieceFile *pieceFile) {
    this->pieceFile = pieceFile;
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

    if(game == nullptr || ecoulement == nullptr) {
        return;
    }

    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - game->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - game->getHauteur() * spriteH) / 2;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Deux passages : toutes les pieces, puis tout le liquide. Ca evite qu'une
    // case repeigne son fond par-dessus le liquide de sa voisine.
    for(int y=0;y<game->getHauteur();y++) {
        for(int x=0;x<game->getLargeur();x++) {
            QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
            dessinerPiece(painter, dest, game->getTypePiece(x, y), game->getSens(x, y));
        }
    }

    for(int y=0;y<game->getHauteur();y++) {
        for(int x=0;x<game->getLargeur();x++) {
            float p = ecoulement->progression(x, y);

            if(p > 0.0f) {
                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
                dessinerLiquide(painter, dest, game->getTypePiece(x, y), game->getSens(x, y),
                                ecoulement->entree(x, y), p);
            }
        }
    }
}

void WGame::mouseReleaseEvent(QMouseEvent *event) {
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - game->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - game->getHauteur() * spriteH) / 2;
    int x = (event->pos().x() - margeX) / spriteW;
    int y = (event->pos().y() - margeY) / spriteH;

    if(ecoulement->estRempli(x, y)) {
        return;
    }

    Piece piece = pieceFile->depiler();
    game->setTypePiece(x, y, piece.type);
    game->setSens(x, y, piece.sens);
    repaint();

    emit pieceDeposee();
}
