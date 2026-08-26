#ifndef WPIECEFILE_H
#define WPIECEFILE_H

#define FILE_SIZE       5

#include <QWidget>
#include <QVariantAnimation>
#include "piecefile.h"

class WPieceFile : public QWidget
{
    Q_OBJECT
public:
    explicit WPieceFile(QWidget *parent = nullptr);
    void setPieceFile(PieceFile *pieceFile);

public slots:
    void animerDepilage();

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    PieceFile *pieceFile = nullptr;
    QVariantAnimation *animation;
    QVector<Piece> anciennesPieces;
    qreal progression = 1.0;

    int spriteWidth() const;
    int spriteHeight() const;
    void memoriserPieces();
signals:
};

#endif // WPIECEFILE_H
