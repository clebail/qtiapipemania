#ifndef WGAME_H
#define WGAME_H

#include <QWidget>
#include "game.h"
#include "ecoulement.h"
#include "piecefile.h"

class WGame : public QWidget
{
    Q_OBJECT
public:
    explicit WGame(QWidget *parent = nullptr);
    void setGame(Game *game);
    void setEcoulement(Ecoulement *ecoulement);
    void setPieceFile(PieceFile *pieceFile);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);

private:
    Game *game = nullptr;
    Ecoulement *ecoulement = nullptr;
    PieceFile *pieceFile = nullptr;

    int spriteWidth() const;
    int spriteHeight() const;
signals:
    // remplacement : la case portait deja une piece, ce qui coute des points.
    void pieceDeposee(bool remplacement);
};

#endif // WGAME_H
