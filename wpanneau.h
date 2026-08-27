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
    int hauteurScore = 0;
    int tailleChiffres = 12;
    qreal progression = 1.0;

    int spriteWidth() const;
    int spriteHeight() const;
    void memoriserPieces();
    int dessinerNiveau(QPainter& painter, int y);
    int dessinerVies(QPainter& painter, int y);
    void dessinerScore(QPainter& painter, int y);
    void dessinerEtat(QPainter& painter, int y, int tailleScore);
    int dessinerCompteur(QPainter& painter, int y, int maxi, const QString& libelle,
                         const QString& valeur, const QColor& couleur);
signals:
};

#endif // WPANNEAU_H
