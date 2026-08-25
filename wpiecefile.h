#ifndef WPIECEFILE_H
#define WPIECEFILE_H

#define FILE_SIZE       5

#include <QWidget>
#include <QImage>
#include "piecefile.h"

class WPieceFile : public QWidget
{
    Q_OBJECT
public:
    explicit WPieceFile(QWidget *parent = nullptr);
    void setPieceFile(PieceFile *pieceFile);

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    PieceFile *pieceFile = nullptr;
    QImage tileset;

    int spriteWidth() const;
    int spriteHeight() const;
signals:
};

#endif // WPIECEFILE_H
