#ifndef WGAME_H
#define WGAME_H

#include <QWidget>
#include <QImage>
#include "game.h"
#include "piecefile.h"

class WGame : public QWidget
{
    Q_OBJECT
public:
    explicit WGame(QWidget *parent = nullptr);
    void setGame(Game *game);
    void setPieceFile(PieceFile *pieceFile);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    Game *game = nullptr;
    PieceFile *pieceFile = nullptr;
    QImage tileset;

    int spriteWidth() const;
    int spriteHeight() const;
signals:
    void pieceDeposee();
};

#endif // WGAME_H
