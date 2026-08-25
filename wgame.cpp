#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "sprite.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
    tileset.load(":/tileset.png");
}

void WGame::setGame(Game *game) {
    this->game = game;
    repaint();
}

void WGame::setPieceFile(PieceFile *pieceFile) {
    this->pieceFile = pieceFile;
}

int WGame::spriteWidth() const {
    return static_cast<int>(SPRITE_WIDTH * SPRITE_SCALE);
}

int WGame::spriteHeight() const {
    return static_cast<int>(SPRITE_HEIGHT * SPRITE_SCALE);
}

void WGame::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    QPen pen(Qt::black);
    QBrush brush(Qt::black);

    painter.setPen(pen);
    painter.setBrush(brush);
    painter.drawRect(0, 0, size().width(), size().height());

    if(game != nullptr) {
        int spriteW = spriteWidth();
        int spriteH = spriteHeight();
        int margeX = (size().width() - game->getLargeur() * spriteW) / 2;
        int margeY = (size().height() - game->getHauteur() * spriteH) / 2;

        for(int y=0;y<game->getHauteur();y++) {
            for(int x=0;x<game->getLargeur();x++) {
                ETypePiece type = game->getTypePiece(x, y);
                ESens sens = game->getSens(x, y);
                Sprite *sprite = Sprite::create(type, sens);
                QRect dest(QPoint(x*spriteW + margeX, y*spriteH + margeY), QSize(spriteW, spriteH));
                painter.drawImage(dest, tileset, sprite->getImageCoords());
                delete sprite;
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

    Piece piece = pieceFile->depiler();
    game->setTypePiece(x, y, piece.type);
    game->setSens(x, y, piece.sens);
    repaint();

    emit pieceDeposee();
}
