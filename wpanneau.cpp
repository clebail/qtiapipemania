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

// Plus grande taille de police dont `texte` tient encore dans `largeur`.
static int tailleQuiTient(QFont& police, const QString& texte, int largeur, int maxi) {
    int retenue = 8;

    for(int essai = 9; essai <= maxi; essai++) {
        police.setPixelSize(essai);
        if(QFontMetrics(police).horizontalAdvance(texte) > largeur) {
            break;
        }
        retenue = essai;
    }

    police.setPixelSize(retenue);
    return retenue;
}

// Numero de niveau, au-dessus du score. Renvoie la hauteur occupee.
int WPanneau::dessinerNiveau(QPainter& painter, int y) {
    static const QColor cNiveau(0x9a, 0xa4, 0xc8);

    QString texte = tr("NIVEAU %1").arg(partie->niveau(), 2, 10, QChar('0'));

    QFont police("monospace");
    police.setStyleHint(QFont::TypeWriter);
    tailleQuiTient(police, texte, width() - 12, 22);

    QFontMetrics mesure(police);
    painter.setFont(police);
    painter.setPen(cNiveau);
    painter.drawText(QPointF((width() - mesure.horizontalAdvance(texte)) / 2.0,
                             y + mesure.ascent()), texte);

    return mesure.height();
}

// Etat de la manche, sous le score : un mot et son detail chiffre.
void WPanneau::dessinerEtat(QPainter& painter, int y, int tailleScore) {
    static const QColor cDetail(0x6e, 0x7a, 0xa8);

    QString titre;
    QString detail;
    QColor couleur;

    switch(partie->etat()) {
    case epAttente:
        titre = tr("PRET");
        couleur = QColor(0x6e, 0xd8, 0xff);
        break;
    case epEcoulement:
        titre = tr("FLUX");
        detail = QString("%1/%2").arg(partie->casesTraversees()).arg(partie->longueurMinimale());
        couleur = QColor(0x6e, 0xd8, 0xff);
        break;
    case epReussie:
        titre = tr("REUSSI");
        detail = QString("+%1").arg(partie->bonusManche());
        couleur = QColor(0x2f, 0xbf, 0x4f);
        break;
    case epPerdue:
        titre = tr("PERDU");
        detail = QString("%1/%2").arg(partie->casesTraversees()).arg(partie->longueurMinimale());
        couleur = QColor(0xd8, 0x50, 0x40);
        break;
    }

    int disponible = width() - 12;

    QFont policeTitre("monospace");
    policeTitre.setStyleHint(QFont::TypeWriter);
    policeTitre.setBold(true);
    int tailleTitre = qMin(tailleQuiTient(policeTitre, titre, disponible, 72), tailleScore * 2 / 3);
    policeTitre.setPixelSize(tailleTitre);

    QFontMetrics mesureTitre(policeTitre);
    painter.setFont(policeTitre);
    painter.setPen(couleur);
    painter.drawText(QPointF((width() - mesureTitre.horizontalAdvance(titre)) / 2.0,
                             y + mesureTitre.ascent()), titre);

    if(detail.isEmpty()) {
        return;
    }

    QFont policeDetail("monospace");
    policeDetail.setStyleHint(QFont::TypeWriter);
    policeDetail.setPixelSize(qMax(9, tailleTitre * 3 / 4));

    QFontMetrics mesureDetail(policeDetail);
    painter.setFont(policeDetail);
    painter.setPen(cDetail);
    painter.drawText(QPointF((width() - mesureDetail.horizontalAdvance(detail)) / 2.0,
                             y + mesureTitre.height() + 4 + mesureDetail.ascent()), detail);
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
    int pixels = tailleQuiTient(policeChiffres, chiffres, disponible, 72);

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

    hauteurScore = mesureLibelle.height() + 6 + mesureChiffres.height();
    tailleChiffres = pixels;
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
    // Bloc d'information sous la file : niveau, score, etat de la manche.
    int y = (taille + 1) * spriteH + 10;
    y += dessinerNiveau(painter, y) + 12;
    dessinerScore(painter, y);
    dessinerEtat(painter, y + hauteurScore + 18, tailleChiffres);
}
