#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "dessinpiece.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
}

void WGame::setGame(Game *game) {
    this->game = game;
    repaint();
}

void WGame::setEcoulement(Ecoulement *ecoulement) {
    this->ecoulement = ecoulement;
    repaint();
}

void WGame::setPieceFile(PieceFile *pieceFile) {
    this->pieceFile = pieceFile;
}

int WGame::spriteWidth() const {
    return TAILLE_CASE;
}

int WGame::spriteHeight() const {
    return TAILLE_CASE;
}

void WGame::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    if(game == nullptr || ecoulement == nullptr) {
        return;
    }

    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - game->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - game->getHauteur() * spriteH) / 2;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Deux passages : toutes les pieces, puis tout le liquide. Ca evite qu'une
    // case repeigne son fond par-dessus le liquide de sa voisine.
    for(int y=0;y<game->getHauteur();y++) {
        for(int x=0;x<game->getLargeur();x++) {
            QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
            dessinerPiece(painter, dest, game->getTypePiece(x, y), game->getSens(x, y));
        }
    }

    for(int y=0;y<game->getHauteur();y++) {
        for(int x=0;x<game->getLargeur();x++) {
            float p = ecoulement->progression(x, y);

            if(p > 0.0f) {
                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
                dessinerLiquide(painter, dest, game->getTypePiece(x, y), game->getSens(x, y),
                                ecoulement->entree(x, y), p);
            }
        }
    }
}

void WGame::mouseReleaseEvent(QMouseEvent *event) {
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - game->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - game->getHauteur() * spriteH) / 2;
    int px = event->pos().x() - margeX;
    int py = event->pos().y() - margeY;

    // Un relachement hors de la grille est possible : Qt livre l'evenement au
    // widget qui a recu l'appui, meme si le curseur en est sorti. On borne en
    // pixels et non en cases, car la division entiere tronque vers zero :
    // -40 / 54 vaut 0, donc un clic a gauche de la grille retomberait sur la
    // colonne 0 au lieu d'etre rejete.
    if(px < 0 || py < 0
       || px >= game->getLargeur() * spriteW
       || py >= game->getHauteur() * spriteH) {
        return;
    }

    int x = px / spriteW;
    int y = py / spriteH;

    ETypePiece actuelle = game->getTypePiece(x, y);

    // Case deja traversee par le fluide, ou reservoir : rien a faire. Sans ce
    // test la piece serait depilee pour rien, puisque setTypePiece refuse
    // d'ecraser le reservoir.
    if(ecoulement->estRempli(x, y) || actuelle == tpReservoir) {
        return;
    }

    bool remplacement = (actuelle != tpNone);

    Piece piece = pieceFile->depiler();
    game->setTypePiece(x, y, piece.type);
    game->setSens(x, y, piece.sens);
    repaint();

    emit pieceDeposee(remplacement);
}
