#ifndef WPANNEAU_H
#define WPANNEAU_H

#include <QWidget>
#include <QPainter>
#include <QVariantAnimation>
#include "partie.h"

class WPanneau : public QWidget
{
    Q_OBJECT
public:
    explicit WPanneau(QWidget *parent = nullptr);
    void setPartie(Partie *partie);


public slots:
    void animerDepilage();

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    Partie *partie = nullptr;
    QVariantAnimation *animation;
    QVector<Piece> anciennesPieces;
    qreal progression = 1.0;

    int spriteWidth() const;
    int spriteHeight() const;
    void memoriserPieces();
    void dessinerScore(QPainter& painter, int y);
signals:
};

#endif // WPANNEAU_H
