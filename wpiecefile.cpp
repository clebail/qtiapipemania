#include <QPainter>
#include "wpiecefile.h"
#include "sprite.h"

WPieceFile::WPieceFile(QWidget *parent)
    : QWidget{parent}
{
    tileset.load(":/tileset.png");
    setFixedWidth(spriteWidth());
}

void WPieceFile::setPieceFile(PieceFile *pieceFile) {
    this->pieceFile = pieceFile;
    repaint();
}

int WPieceFile::spriteWidth() const {
    return static_cast<int>(SPRITE_WIDTH * SPRITE_SCALE);
}

int WPieceFile::spriteHeight() const {
    return static_cast<int>(SPRITE_HEIGHT * SPRITE_SCALE);
}

void WPieceFile::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    QPen pen(Qt::black);
    QBrush brush(Qt::black);

    painter.setPen(pen);
    painter.setBrush(brush);
    painter.drawRect(0, 0, size().width(), size().height());

    if(pieceFile != nullptr) {
        int spriteW = spriteWidth();
        int spriteH = spriteHeight();
        int margeX = (size().width() - spriteW) / 2;

        for(int i=0;i<pieceFile->getTaille();i++) {
            Piece piece = pieceFile->getPiece(i);
            Sprite *sprite = Sprite::create(piece.type, piece.sens);
            QRect dest(QPoint(margeX, i*spriteH), QSize(spriteW, spriteH));
            painter.drawImage(dest, tileset, sprite->getImageCoords());
            delete sprite;
        }
    }
}
