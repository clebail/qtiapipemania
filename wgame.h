#ifndef WGAME_H
#define WGAME_H

#include <QWidget>
#include "partie.h"

class WGame : public QWidget
{
    Q_OBJECT
public:
    explicit WGame(QWidget *parent = nullptr);
    void setPartie(Partie *partie);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void leaveEvent(QEvent *event);

private:
    Partie *partie = nullptr;
    QPoint caseSurvolee = QPoint(-1, -1);

    int spriteWidth() const;
    int spriteHeight() const;
    QPoint caseSous(const QPoint& position) const;
signals:
    void pieceDeposee();
};

#endif // WGAME_H
