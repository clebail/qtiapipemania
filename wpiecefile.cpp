#include <QPainter>
#include "wpiecefile.h"
#include "dessinpiece.h"

WPieceFile::WPieceFile(QWidget *parent)
    : QWidget{parent}
{
    setFixedWidth(spriteWidth());

    animation = new QVariantAnimation(this);
    animation->setStartValue(0.0);
    animation->setEndValue(1.0);
    animation->setDuration(300);
    animation->setEasingCurve(QEasingCurve::InOutQuad);
    connect(animation, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        progression = value.toReal();
        update();
    });
    connect(animation, &QVariantAnimation::finished, this, [this]() {
        memoriserPieces();
        update();
    });
}

void WPieceFile::setPieceFile(PieceFile *pieceFile) {
    this->pieceFile = pieceFile;
    memoriserPieces();
    repaint();
}

void WPieceFile::memoriserPieces() {
    anciennesPieces.clear();
    if(pieceFile != nullptr) {
        for(int i=0;i<pieceFile->getTaille();i++) {
            anciennesPieces.append(pieceFile->getPiece(i));
        }
    }
}

void WPieceFile::animerDepilage() {
    // anciennesPieces contient encore l'état de la file avant le depiler().
    animation->stop();
    progression = 0.0;
    animation->start();
}

int WPieceFile::spriteWidth() const {
    return TAILLE_CASE;
}

int WPieceFile::spriteHeight() const {
    return TAILLE_CASE;
}

void WPieceFile::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if(pieceFile == nullptr) {
        return;
    }

    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - spriteW) / 2;
    int taille = pieceFile->getTaille();

    if(animation->state() == QAbstractAnimation::Running && !anciennesPieces.isEmpty()) {
        // La piece du haut se fait progressivement ecraser par la suivante.
        Piece ecrasee = anciennesPieces.first();
        int hauteurRestante = static_cast<int>(spriteH * (1.0 - progression));
        if(hauteurRestante > 0) {
            // La piece ecrasee est dessinee a sa taille normale mais rognee,
            // pour ne pas la deformer pendant l'ecrasement.
            painter.save();
            painter.setClipRect(QRect(QPoint(margeX, 0), QSize(spriteW, hauteurRestante)));
            dessinerPiece(painter, QRectF(margeX, 0, spriteW, spriteH), ecrasee.type, ecrasee.sens);
            painter.restore();
        }

        // Les pieces suivantes remontent d'un cran, la derniere etant la piece qui vient d'apparaitre.
        for(int i=1;i<=taille;i++) {
            Piece piece = (i < anciennesPieces.size())
                ? anciennesPieces.at(i)
                : pieceFile->getPiece(taille - 1);
            qreal y = (i - progression) * spriteH;
            dessinerPiece(painter, QRectF(margeX, y, spriteW, spriteH), piece.type, piece.sens);
        }
    } else {
        for(int i=0;i<taille;i++) {
            Piece piece = pieceFile->getPiece(i);
            dessinerPiece(painter, QRectF(margeX, i*spriteH, spriteW, spriteH), piece.type, piece.sens);
        }
    }
}
