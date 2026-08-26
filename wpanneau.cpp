#include <QPainter>
#include "wpanneau.h"
#include "dessinpiece.h"

// Plus large qu'une case : les pieces restent centrees dessus, et la place
// gagnee sert a afficher le score en gros.
#define LARGEUR_PANNEAU     (TAILLE_CASE * 2)

WPanneau::WPanneau(QWidget *parent)
    : QWidget{parent}
{
    setFixedWidth(LARGEUR_PANNEAU);

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

void WPanneau::setPartie(Partie *partie) {
    this->partie = partie;
    memoriserPieces();
    repaint();
}

void WPanneau::memoriserPieces() {
    anciennesPieces.clear();
    if(partie != nullptr) {
        PieceFile *file = partie->file();
        for(int i=0;i<file->getTaille();i++) {
            anciennesPieces.append(file->getPiece(i));
        }
    }
}

void WPanneau::animerDepilage() {
    // anciennesPieces contient encore l'état de la file avant le depiler().
    animation->stop();
    progression = 0.0;
    animation->start();
}

int WPanneau::spriteWidth() const {
    return TAILLE_CASE;
}

int WPanneau::spriteHeight() const {
    return TAILLE_CASE;
}

// Afficheur facon borne d'arcade : toujours 6 chiffres, zeros de tete compris.
void WPanneau::dessinerScore(QPainter& painter, int y) {
    static const QColor cLibelle(0x6e, 0x7a, 0xa8);
    static const QColor cChiffres(0xf0, 0xc0, 0x20);

    QString chiffres = QString("%1").arg(partie->score(), 6, 10, QChar('0'));
    QString libelle = tr("SCORE");

    QFont policeChiffres("monospace");
    policeChiffres.setStyleHint(QFont::TypeWriter);
    policeChiffres.setBold(true);

    // On prend la plus grande taille dont les six chiffres tiennent encore :
    // l'affichage suit donc la largeur du panneau, quelle qu'elle soit.
    int disponible = width() - 12;
    int pixels = 8;
    for(int essai = 9; essai <= 72; essai++) {
        policeChiffres.setPixelSize(essai);
        if(QFontMetrics(policeChiffres).horizontalAdvance(chiffres) > disponible) {
            break;
        }
        pixels = essai;
    }
    policeChiffres.setPixelSize(pixels);

    QFontMetrics mesureChiffres(policeChiffres);
    int largeur = mesureChiffres.horizontalAdvance(chiffres);

    // Le libelle est cale sur la largeur exacte des chiffres en ecartant ses
    // lettres. L'ecart s'applique apres chaque caractere, y compris le dernier,
    // d'ou le (n - 1) : c'est l'encre visible qu'on veut de cette largeur, pas
    // l'avance totale.
    QFont policeLibelle("monospace");
    policeLibelle.setStyleHint(QFont::TypeWriter);
    policeLibelle.setPixelSize(qMax(8, pixels / 2));

    qreal naturelle = QFontMetrics(policeLibelle).horizontalAdvance(libelle);
    if(libelle.length() > 1 && largeur > naturelle) {
        policeLibelle.setLetterSpacing(QFont::AbsoluteSpacing,
                                       (largeur - naturelle) / (libelle.length() - 1));
    }

    QFontMetrics mesureLibelle(policeLibelle);
    int x = (width() - largeur) / 2;

    painter.setFont(policeLibelle);
    painter.setPen(cLibelle);
    painter.drawText(QPointF(x, y + mesureLibelle.ascent()), libelle);

    painter.setFont(policeChiffres);
    painter.setPen(cChiffres);
    painter.drawText(QPointF(x, y + mesureLibelle.height() + 6 + mesureChiffres.ascent()), chiffres);
}

void WPanneau::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);
    painter.setRenderHint(QPainter::Antialiasing, true);

    if(partie == nullptr) {
        return;
    }

    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - spriteW) / 2;
    PieceFile *file = partie->file();
    int taille = file->getTaille();

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
                : file->getPiece(taille - 1);
            qreal y = (i - progression) * spriteH;
            dessinerPiece(painter, QRectF(margeX, y, spriteW, spriteH), piece.type, piece.sens);
        }
    } else {
        for(int i=0;i<taille;i++) {
            Piece piece = file->getPiece(i);
            dessinerPiece(painter, QRectF(margeX, i*spriteH, spriteW, spriteH), piece.type, piece.sens);
        }
    }

    // Sous la file, en laissant la place a la piece qui descend pendant
    // l'animation : elle atteint le bas de la case (taille), donc on demarre
    // une case plus bas.
    dessinerScore(painter, (taille + 1) * spriteH + 10);
}
