#include <QPainter>
#include <QMouseEvent>
#include <QtDebug>
#include "wgame.h"
#include "bot.h"
#include "dessinpiece.h"

WGame::WGame(QWidget *parent) : QWidget{parent} {
    // Sans cela Qt n'envoie mouseMoveEvent que bouton enfonce.
    setMouseTracking(true);
}

// Case sous une position en pixels, ou (-1,-1) si le point tombe hors grille.
QPoint WGame::caseSous(const QPoint& position) const {
    if(partie == nullptr) {
        return QPoint(-1, -1);
    }

    Game *plateau = partie->plateau();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int px = position.x() - (size().width() - plateau->getLargeur() * spriteW) / 2;
    int py = position.y() - (size().height() - plateau->getHauteur() * spriteH) / 2;

    // Borne en pixels et non en cases : la division entiere tronque vers zero,
    // donc -40 / 54 vaut 0 et un point a gauche de la grille retomberait sur la
    // colonne 0 au lieu d'etre rejete.
    if(px < 0 || py < 0
       || px >= plateau->getLargeur() * spriteW
       || py >= plateau->getHauteur() * spriteH) {
        return QPoint(-1, -1);
    }

    return QPoint(px / spriteW, py / spriteH);
}

void WGame::mouseMoveEvent(QMouseEvent *event) {
    QPoint survolee = caseSous(event->pos());

    if(survolee != caseSurvolee) {
        caseSurvolee = survolee;
        update();
    }
}

void WGame::leaveEvent(QEvent *) {
    if(caseSurvolee.x() >= 0) {
        caseSurvolee = QPoint(-1, -1);
        update();
    }
}

void WGame::setPartie(Partie *partie) {
    this->partie = partie;
    repaint();
}

void WGame::setBot(Bot *bot) {
    this->bot = bot;
    update();
}

void WGame::setAfficherTas(bool afficher) {
    afficherTas = afficher;
    update();
}

void WGame::setAfficherPlan(bool afficher) {
    afficherPlan = afficher;

    // Le point fixe des cases mortes coute 2,5x en simulation et ne sert plus
    // a aucune decision : on ne le fait calculer que tant qu'on le regarde.
    if(bot != nullptr) {
        bot->setCalculerMortes(afficher);
    }

    update();
}

void WGame::setRegion(const QVector<unsigned char> &region) {
    this->region = region;
    update();
}

int WGame::spriteWidth() const {
    return tailleCase();
}

int WGame::spriteHeight() const {
    return tailleCase();
}

// Curseur en equerres, facon viseur : quatre coins plutot qu'un cadre plein,
// pour ne pas masquer le tuyau de la case.
// Ne se dessine que sur une case ou le coup passe : l'absence de curseur dit
// deja "pas la". Un curseur rouge n'ajoutait rien -- et depuis que le flux va au
// bout de son tuyau, peutPoser() refuse la grille entiere des l'objectif
// atteint, ce qui la peignait en rouge pendant toute la fin de manche.
static void dessinerCurseur(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0x6e, 0xd8, 0xff);

    qreal retrait = tuile.width() * 0.08;
    qreal branche = tuile.width() * 0.26;
    QRectF r = tuile.adjusted(retrait, retrait, -retrait, -retrait);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 40));
    painter.drawRect(r);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(couleur, qMax(2.0, tuile.width() * 0.045), Qt::SolidLine, Qt::FlatCap));

    const QPointF coins[4] = { r.topLeft(), r.topRight(), r.bottomRight(), r.bottomLeft() };
    const QPointF sensH[4] = { QPointF(branche, 0), QPointF(-branche, 0),
                               QPointF(-branche, 0), QPointF(branche, 0) };
    const QPointF sensV[4] = { QPointF(0, branche), QPointF(0, branche),
                               QPointF(0, -branche), QPointF(0, -branche) };

    for(int i=0;i<4;i++) {
        painter.drawLine(coins[i], coins[i] + sensH[i]);
        painter.drawLine(coins[i], coins[i] + sensV[i]);
    }
}

// Ce qu'on laisse voir d'une case obligee. Assez pour lire le tuyau et son
// orientation, assez peu pour qu'il ne se confonde pas avec une piece posee :
// c'est encore une case vide.
#define OPACITE_OBLIGEE 0.45

// Une case MORTE : morte par ses quatre cotes, donc aucun trace ne la
// traversera jamais, quelle que soit la piece et quel que soit le chemin. Ce
// n'est pas une contrainte de routage mais de la surface perdue -- d'ou la
// croix barree plutot qu'un tuyau. Grise et sourde : elle ne demande rien, elle
// constate que ce terrain est sorti du jeu.
static void dessinerCaseMorte(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0x8a, 0x8f, 0x9c);

    painter.save();

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 38));
    painter.drawRect(tuile);

    QRectF r = tuile.adjusted(tuile.width() * 0.3, tuile.height() * 0.3,
                              -tuile.width() * 0.3, -tuile.height() * 0.3);

    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 130),
                        qMax(1.0, tuile.width() * 0.05), Qt::SolidLine, Qt::RoundCap));
    painter.drawLine(r.topLeft(), r.bottomRight());
    painter.drawLine(r.topRight(), r.bottomLeft());

    painter.restore();
}

// Plan de defausse : le reseau que le bot vise quand il jette une piece. Un
// trait fin et jaune du centre de la case vers chacune de ses ouvertures --
// assez pour lire la trame et voir ou le plan se tait, assez discret pour ne
// pas se confondre avec un tuyau reellement pose.
// `oblige` : le plan n'a rien choisi sur cette case, il a constate -- un seul
// routage y survit. On n'y dessine donc plus une intention mais LA PIECE, en
// transparence. Le trait jaune dit "le bot aimerait" ; le fantome dit "ce
// tuyau sera la, ou le trace mourra". Une case obligee est toujours vide
// (marquerObligations saute les cases pleines) : le fantome ne peut pas
// masquer un tuyau reellement pose.
static void dessinerPlan(QPainter& painter, const QRectF& tuile, const ETypePiece& type,
                         bool oblige) {
    static const QColor couleur(0xf2, 0xc3, 0x3c);

    if(oblige) {
        // Le plan ne stocke qu'un type ; sHaut est l'orientation canonique,
        // celle que les ouvertures du plan supposent partout ailleurs.
        qreal avant = painter.opacity();

        painter.setOpacity(OPACITE_OBLIGEE);
        dessinerPiece(painter, tuile, type, sHaut);
        painter.setOpacity(avant);
        return;
    }

    QPointF centre = tuile.center();
    qreal demi = tuile.width() / 2.0;

    painter.save();
    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 150),
                        qMax(1.0, tuile.width() * 0.035),
                        Qt::SolidLine, Qt::RoundCap));

    foreach(ESens s, Ecoulement::ouvertures(type, sHaut)) {
        QPointF bout = centre;

        switch(s) {
        case sHaut:   bout.ry() -= demi; break;
        case sBas:    bout.ry() += demi; break;
        case sGauche: bout.rx() -= demi; break;
        case sDroite: bout.rx() += demi; break;
        }

        painter.drawLine(centre, bout);
    }

    painter.restore();
}

// Une case du tas posee par l'ANTICIPATION : la chaine que le v3 projette
// devant lui pour un pont encore loin dans la file. Hachures inclinees dans
// l'autre sens et teinte violette, pour la distinguer d'un coup d'oeil d'une
// case de defausse -- les deux etaient indiscernables, alors que leur nature
// n'a rien a voir : la defausse suit un plan qui vaut pour toute la manche,
// l'anticipation parie sur un pont qui n'arrivera peut-etre jamais.
static void dessinerMarqueurAnticipation(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0x9b, 0x6e, 0xc8);

    painter.save();
    painter.setClipRect(tuile);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 44));
    painter.drawRect(tuile);

    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 85),
                        qMax(1.0, tuile.width() * 0.03)));

    // Sens inverse de celles du tas : c'est ce qui les separe a l'oeil.
    qreal pas = tuile.width() / 4.0;
    for(qreal d = 0; d < tuile.width() + tuile.height(); d += pas) {
        painter.drawLine(QPointF(tuile.left() + d, tuile.top()),
                         QPointF(tuile.left() + d - tuile.height(), tuile.bottom()));
    }

    painter.restore();
}

// Une case du tas posee sur un PARI : la defausse a vise la case de rang 2 du
// trajet anticipe, en pariant que le rang 1 recevra le type suppose. Hachures
// rouges, verticales, pour qu'aucune des trois origines ne se confonde -- la
// defausse ordinaire penche a droite, l'anticipation a gauche, le pari est
// droit. Le plan reclamait cette case de toute facon : un pari perdu reste une
// defausse valable, la teinte ne signale pas un risque mais une intention.
static void dessinerMarqueurPari(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0xd8, 0x54, 0x54);

    painter.save();
    painter.setClipRect(tuile);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 46));
    painter.drawRect(tuile);

    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 95),
                        qMax(1.0, tuile.width() * 0.03)));

    qreal pas = tuile.width() / 4.0;
    for(qreal d = pas / 2.0; d < tuile.width(); d += pas) {
        painter.drawLine(QPointF(tuile.left() + d, tuile.top()),
                         QPointF(tuile.left() + d, tuile.bottom()));
    }

    painter.restore();
}

// Overlay du mode pas a pas : la place que le trace aurait encore devant lui
// si l'on posait maintenant la piece du haut de file sur la tete. Un lavis vert
// sur les cases atteignables, un cadre ambre sur la case de pose elle-meme --
// deux marques franches, a ne pas confondre avec les hachures du tas ni avec le
// viseur du curseur.
static void dessinerMarqueurRegion(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0x5c, 0xd0, 0x8a);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 46));
    painter.drawRect(tuile);
}

static void dessinerMarqueurPose(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0xff, 0xb1, 0x4a);

    qreal retrait = tuile.width() * 0.06;
    QRectF r = tuile.adjusted(retrait, retrait, -retrait, -retrait);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 60));
    painter.drawRect(r);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(couleur, qMax(2.0, tuile.width() * 0.05)));
    painter.drawRect(r);
}

// Overlay du tas de defausse : une teinte terne et quelques hachures sur la
// case, pour la reconnaitre d'un coup d'oeil sans masquer le tuyau qu'elle
// porte. N'apparait que quand un bot joue -- c'est un outil pour lire sa
// strategie, pas un element du jeu.
static void dessinerMarqueurTas(QPainter& painter, const QRectF& tuile) {
    static const QColor couleur(0x6e, 0x7a, 0xa8);

    painter.save();
    painter.setClipRect(tuile);

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(couleur.red(), couleur.green(), couleur.blue(), 36));
    painter.drawRect(tuile);

    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 70),
                        qMax(1.0, tuile.width() * 0.03)));

    qreal pas = tuile.width() / 4.0;
    for(qreal d = -tuile.height(); d < tuile.width(); d += pas) {
        painter.drawLine(QPointF(tuile.left() + d, tuile.top()),
                         QPointF(tuile.left() + d + tuile.height(), tuile.bottom()));
    }

    painter.restore();
}

void WGame::paintEvent(QPaintEvent *) {
    QPainter painter(this);

    painter.fillRect(rect(), Qt::black);

    if(partie == nullptr) {
        return;
    }

    Game *plateau = partie->plateau();
    Ecoulement *ecoul = partie->ecoulement();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - plateau->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - plateau->getHauteur() * spriteH) / 2;

    painter.setRenderHint(QPainter::Antialiasing, true);

    // Deux passages : toutes les pieces, puis tout le liquide. Ca evite qu'une
    // case repeigne son fond par-dessus le liquide de sa voisine.
    for(int y=0;y<plateau->getHauteur();y++) {
        for(int x=0;x<plateau->getLargeur();x++) {
            QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
            dessinerPiece(painter, dest, plateau->getTypePiece(x, y), plateau->getSens(x, y));
        }
    }

    // Plan de defausse, tout en dessous : c'est une intention, pas une piece.
    // Il se lit a travers les tuyaux poses, qui restent au-dessus.
    if(bot != nullptr && afficherPlan) {
        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);

                // Les mortes d'abord, sous le plan : une case peut etre les
                // deux a la fois -- le plan y veut quelque chose, et le terrain
                // dit que personne n'y passera. C'est justement ce desaccord
                // qu'on veut pouvoir lire.
                if(bot->caseMorte(x, y)) {
                    dessinerCaseMorte(painter, dest);
                }

                ETypePiece voulu = bot->planType(x, y);

                if(voulu != tpNone) {
                    dessinerPlan(painter, dest, voulu, bot->planObligatoire(x, y));
                }
            }
        }
    }

    // Region du pas a pas, sous le tas : une case peut etre les deux a la fois,
    // et les hachures du tas doivent rester lisibles par-dessus le lavis.
    if(region.size() == plateau->getSize()) {
        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);

                if(region[y * plateau->getLargeur() + x] == Bot::mrAtteignable) {
                    dessinerMarqueurRegion(painter, dest);
                } else if(region[y * plateau->getLargeur() + x] == Bot::mrPose) {
                    dessinerMarqueurPose(painter, dest);
                }
            }
        }
    }

    // Tas de defausse, entre les pieces et le liquide : sous le flux (une case
    // du tas peut finir raccordee), au-dessus du tuyau qu'elle porte.
    if(bot != nullptr && afficherTas) {
        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                if(bot->estTas(x, y)) {
                    QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);

                    if(bot->estAnticipee(x, y)) {
                        dessinerMarqueurAnticipation(painter, dest);
                    } else if(bot->estPari(x, y)) {
                        dessinerMarqueurPari(painter, dest);
                    } else {
                        dessinerMarqueurTas(painter, dest);
                    }
                }
            }
        }
    }

    for(int y=0;y<plateau->getHauteur();y++) {
        for(int x=0;x<plateau->getLargeur();x++) {
            // Les deux conduites de la case : une croix traversee deux fois en
            // porte deux, toute autre piece n'en remplit qu'une.
            for(int axe=0;axe<NB_AXES;axe++) {
                float p = ecoul->progression(x, y, axe);

                if(p > 0.0f) {
                    QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
                    dessinerLiquide(painter, dest, plateau->getTypePiece(x, y), plateau->getSens(x, y),
                                    ecoul->entree(x, y, axe), p);
                }
            }
        }
    }

    // Curseur en dernier, pour rester au-dessus des tuyaux et du liquide.
    if(caseSurvolee.x() >= 0 && partie->peutPoser(caseSurvolee.x(), caseSurvolee.y())) {
        QRectF dest(caseSurvolee.x()*spriteW + margeX, caseSurvolee.y()*spriteH + margeY,
                    spriteW, spriteH);
        dessinerCurseur(painter, dest);
    }
}

void WGame::mouseReleaseEvent(QMouseEvent *event) {
    if(partie == nullptr) {
        return;
    }

    Game *plateau = partie->plateau();
    int spriteW = spriteWidth();
    int spriteH = spriteHeight();
    int margeX = (size().width() - plateau->getLargeur() * spriteW) / 2;
    int margeY = (size().height() - plateau->getHauteur() * spriteH) / 2;
    int px = event->pos().x() - margeX;
    int py = event->pos().y() - margeY;

    // Un relachement hors de la grille est possible : Qt livre l'evenement au
    // widget qui a recu l'appui, meme si le curseur en est sorti. On borne en
    // pixels et non en cases, car la division entiere tronque vers zero :
    // -40 / 54 vaut 0, donc un clic a gauche de la grille retomberait sur la
    // colonne 0 au lieu d'etre rejete.
    if(px < 0 || py < 0
       || px >= plateau->getLargeur() * spriteW
       || py >= plateau->getHauteur() * spriteH) {
        return;
    }

    int col = px / spriteW;
    int row = py / spriteH;

    // Les regles du coup (case interdite, penalite de remplacement) sont dans
    // Partie : le widget ne fait que traduire un clic en coordonnees.
    bool accepte = partie->poserPiece(col, row);

    if(accepte) {
        repaint();
    }

    emit pieceDeposee(col, row, accepte);
}
