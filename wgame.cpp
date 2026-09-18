#include <QPainter>
#include <QMouseEvent>
#include <QTimer>
#include <QtMath>
#include <QtDebug>
#include "wgame.h"
#include "bot.h"
#include "dessinpiece.h"

// Duree du flash d'explosion, en millisecondes d'horloge d'AFFICHAGE.
//
// La destruction est instantanee cote moteur : l'animation ne decide de rien,
// elle peut donc garder son quart de seconde meme a 8x, la ou le timer de la
// bombe, lui, est comprime par l'acceleration (BOMBES.md, §2 et §5). La
// simulation reste maitresse, le decor est libre.
#define DUREE_FLASH_MS  250

// Cadence de l'animation, en millisecondes : celle de la fenetre.
#define PAS_FLASH_MS    16

WGame::WGame(QWidget *parent) : QWidget{parent} {
    // Sans cela Qt n'envoie mouseMoveEvent que bouton enfonce.
    setMouseTracking(true);

    horlogeEcran.start();

    // Notre propre cadence, et non les battements de la fenetre : celle-ci
    // cesse de repeindre des que la manche est finie -- exactement le sort de
    // l'explosion qui tue, dont le flash resterait fige sur sa premiere image.
    // Il ne tourne que tant qu'il reste un flash a montrer.
    animation = new QTimer(this);
    animation->setInterval(PAS_FLASH_MS);
    connect(animation, &QTimer::timeout, this, [this]() {
        qint64 maintenant = horlogeEcran.elapsed();

        for(int i = flashs.size() - 1; i >= 0; i--) {
            if(maintenant - flashs.at(i).debut >= DUREE_FLASH_MS) {
                flashs.remove(i);
            }
        }

        if(flashs.isEmpty()) {
            animation->stop();
        }

        update();
    });
}

void WGame::releverExplosions() {
    if(partie == nullptr) {
        return;
    }

    // Toutes les bombes d'un meme battement partagent l'instant de leur releve,
    // et c'est bien ce qu'on veut : elles ont saute dans le meme pas de
    // simulation, elles doivent s'eteindre ensemble.
    qint64 maintenant = horlogeEcran.elapsed();

    foreach(int idx, partie->minage()->preleverExplosions()) {
        flashs << SFlash { idx, maintenant };
    }

    if(!flashs.isEmpty() && !animation->isActive()) {
        animation->start();
    }
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

void WGame::setAfficherTrace(bool afficher) {
    afficherTrace = afficher;
    update();
}

// Le trace du bot fait autorite des qu'il y en a un : lui seul sait qu'il a
// replanifie, et la grille ne doit pas montrer une autre intention que celle
// qu'elle regarde s'executer.
const Trace *WGame::traceAffiche() const {
    if(bot != nullptr && bot->tracePlanifie() != nullptr) {
        return bot->tracePlanifie();
    }

    return &trace;
}

quint32 WGame::signatureBlocs() const {
    const Game *plateau = partie->plateau();
    quint32 h = 2166136261u;

    for(int i = 0; i < plateau->getSize(); i++) {
        if(plateau->getTypePiece(i % plateau->getLargeur(),
                                 i / plateau->getLargeur()) == tpBloque) {
            h = (h ^ (quint32)i) * 16777619u;
        }
    }

    return h;
}

void WGame::rafraichirTrace() {
    if(partie == nullptr || (bot != nullptr && bot->tracePlanifie() != nullptr)) {
        return;
    }

    const Game *plateau = partie->plateau();
    quint32 blocs = signatureBlocs();

    // La graine ET les blocs. La premiere change a chaque manche ; les seconds
    // changent quand une bombe saute, au milieu d'une manche et sans que rien
    // d'autre ne bouge -- et un trace qui contourne des blocs disparus n'est
    // plus celui qu'il faut suivre.
    if(trace.estCalcule() && trace.graine() == plateau->getGraine()
       && blocs == empreinteBlocs) {
        return;
    }

    empreinteBlocs = blocs;

    // Le trace se planifie sur un terrain nu : Game est copiable exactement
    // pour ce genre d'usage, et on n'y garde que ce qui ne bougera pas de la
    // manche -- les blocs et le reservoir.
    Game vierge(*plateau);

    for(int i = 0; i < vierge.getSize(); i++) {
        int col = i % vierge.getLargeur();
        int row = i / vierge.getLargeur();
        ETypePiece t = vierge.getTypePiece(col, row);

        if(t != tpBloque && t != tpReservoir) {
            vierge.setTypePiece(col, row, tpNone);
        }
    }

    trace.calculer(&vierge, partie->longueurMinimale());
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

// Le trace planifie : le chemin de la gagne, du reservoir a l'objectif. Meme
// trait que le plan de defausse -- une intention, pas une piece -- mais en VERT
// et un peu plus epais. Les deux se superposent sans se confondre : l'ambre
// pave le plateau de circuits ou jeter, le vert est le tuyau qu'on veut
// vraiment. La premiere case porte un point plein, pour lire le sens.
static void dessinerTrace(QPainter& painter, const QRectF& tuile,
                          const ETypePiece& type, bool depart) {
    static const QColor couleur(0x4f, 0xd6, 0x7a);

    QPointF centre = tuile.center();
    qreal demi = tuile.width() / 2.0;

    painter.save();
    painter.setPen(QPen(QColor(couleur.red(), couleur.green(), couleur.blue(), 190),
                        qMax(1.0, tuile.width() * 0.055),
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

    if(depart) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(couleur);
        painter.drawEllipse(centre, tuile.width() * 0.13, tuile.width() * 0.13);
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

// Une case du tas posee sur le TRAJET ANTICIPE : la defausse a vise un rang 2
// ou 3 de la chaine projetee, par-dessus les trous qu'elle a enjambes. Hachures
// rouges, a 45 comme les deux autres origines -- c'est la TEINTE qui les
// separe, pas la pente. Ici le plan de defausse n'a pas son mot a dire : seuls
// l'orientation de l'entree et le refus de se condamner ont autorise la pose,
// et une supposition fausse est bien une piece depensee. La teinte signale ce
// risque-la.
// Fraction en dessous de laquelle la bombe s'affole. Le dernier tiers de 2,5 s
// fait moins d'une seconde : assez pour comprendre, trop peu pour s'ecarter --
// c'est l'intention.
#define SEUIL_URGENCE   0.34f

// Le compte a rebours d'une bombe posee, par-dessus la piece.
//
// L'anneau donne la mesure, la meche la donne aussi mais dans l'objet (voir
// dessinerBombe), et la pulsation du dernier tiers ne mesure rien : elle attrape
// l'oeil quand on regarde ailleurs sur le plateau. Les trois ensemble, parce
// qu'ils ne parlent pas au meme moment.
//
// La pulsation se calcule sur la FRACTION, qui decroit lineairement, et jamais
// sur une horloge : le dessin reste sans etat, et l'affolement suit
// l'acceleration exactement comme la bombe qu'il annonce.
static void dessinerCompteARebours(QPainter& painter, const QRectF& tuile, float fraction) {
    static const QColor cAmbre(0xf0, 0x9a, 0x2c);
    static const QColor cUrgence(0xe8, 0x48, 0x38);

    qreal rayon = tuile.width() * 0.38;
    QRectF anneau(tuile.center().x() - rayon, tuile.center().y() - rayon, 2*rayon, 2*rayon);
    qreal trait = qMax(1.5, tuile.width() * 0.07);

    painter.save();
    painter.setBrush(Qt::NoBrush);

    // Le tour entier en terne : sans lui, l'arc qui reste ne se rapporte a rien.
    painter.setPen(QPen(QColor(255, 255, 255, 40), trait));
    painter.drawEllipse(anneau);

    // Ce qu'il reste, de midi et dans le sens des aiguilles. Qt compte en
    // seiziemes de degre, a partir de trois heures et vers la gauche : d'ou le
    // depart a 90 et le span negatif.
    painter.setPen(QPen(cAmbre, trait, Qt::SolidLine, Qt::RoundCap));
    painter.drawArc(anneau, 90 * 16, -(int)(fraction * 360.0f * 16));

    if(fraction < SEUIL_URGENCE) {
        float u = (SEUIL_URGENCE - fraction) / SEUIL_URGENCE;
        // u au carre : la pulsation accelere au lieu de battre regulierement.
        float onde = 0.5f + 0.5f * qCos(u * u * 30.0f);
        QColor halo = cUrgence;
        halo.setAlpha(30 + (int)(150 * onde));

        painter.setPen(QPen(halo, trait * 1.4));
        painter.drawEllipse(anneau.adjusted(-trait * 0.8, -trait * 0.8, trait * 0.8, trait * 0.8));
    }

    painter.restore();
}

// Une case dans le souffle d'une bombe ARMEE. Teinte ambre legere et lisere :
// la meme grammaire que les marqueurs du bot, pour que le plateau n'apprenne
// pas un troisieme langage.
//
// Une case PLEINE, elle, ne prevoit pas une perte mais une mort : elle passe en
// rouge franc et pulse au rythme de la bombe la plus urgente qui la menace. La
// mort immediate cesse d'etre une punition pour devenir un avertissement -- et
// ca se lit aussi bien quand c'est un bot qui joue : on voit ce qu'il a choisi
// de risquer.
static void dessinerSouffle(QPainter& painter, const QRectF& tuile, float fraction, bool pleine) {
    static const QColor cAmbre(0xf0, 0x9a, 0x2c);
    static const QColor cMort(0xe8, 0x48, 0x38);

    const QColor &c = pleine ? cMort : cAmbre;
    int fond = pleine ? 55 : 26;
    int bord = pleine ? 150 : 70;

    if(pleine) {
        // Meme onde que le compte a rebours, et pour la meme raison : calculee
        // sur la fraction, donc sans etat et solidaire de l'acceleration.
        float u = 1.0f - qBound(0.0f, fraction, 1.0f);
        float onde = 0.5f + 0.5f * qCos(u * u * 30.0f);

        fond += (int)(70 * onde);
        bord = qMin(255, bord + (int)(90 * onde));
    }

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(c.red(), c.green(), c.blue(), fond));
    painter.drawRect(tuile);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QColor(c.red(), c.green(), c.blue(), bord), qMax(1.0, tuile.width() * 0.04)));
    painter.drawRect(tuile.adjusted(1, 1, -1, -1));

    painter.restore();
}

// Le meme 3x3, mais avant le clic : ce que la bombe emporterait si on la posait
// la. Pointille et sans remplissage -- il ne faut pas confondre ce qui est arme
// avec ce qu'on envisage. Le rouge, lui, est deja franc : c'est justement
// l'erreur que la previsualisation existe pour eviter.
static void dessinerSouffleFantome(QPainter& painter, const QRectF& tuile, bool pleine) {
    static const QColor cAmbre(0xf0, 0x9a, 0x2c);
    static const QColor cMort(0xe8, 0x48, 0x38);

    const QColor &c = pleine ? cMort : cAmbre;

    painter.save();

    if(pleine) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(c.red(), c.green(), c.blue(), 70));
        painter.drawRect(tuile);
    }

    QPen pointille(QColor(c.red(), c.green(), c.blue(), pleine ? 190 : 110),
                   qMax(1.0, tuile.width() * 0.035), Qt::DotLine);

    painter.setBrush(Qt::NoBrush);
    painter.setPen(pointille);
    painter.drawRect(tuile.adjusted(1, 1, -1, -1));

    painter.restore();
}

// Une case emportee par une explosion qui vient d'avoir lieu. Blanc franc qui
// s'efface : le seul moment ou le plateau parle plus fort que l'ambre, parce
// que la case a change d'etat et qu'il faut le voir sans l'avoir cherche.
//
// La decroissance est en (1-t) au carre -- l'essentiel est passe a mi-course.
// Un flash qui s'eteint lineairement a l'air de trainer.
static void dessinerFlash(QPainter& painter, const QRectF& tuile, float t) {
    float reste = (1.0f - t) * (1.0f - t);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, (int)(210 * reste)));
    painter.drawRect(tuile);
    painter.restore();
}

// Le lisere qui se dilate autour du 3x3 detruit. Il part vite et ralentit,
// l'inverse exact du flash : deux gestes qui decroissent pareil se confondent
// en un seul fondu. Il DEBORDE la zone detruite -- le souffle s'arrete la, et
// l'oeil a besoin de voir l'onde y arriver pour comprendre ou elle s'arrete.
//
// Ambre, comme la bombe et comme le souffle qu'il vient d'accomplir : c'est la
// meme chose qu'on montre, une fois annoncee et une fois faite.
static void dessinerOndeFlash(QPainter& painter, const QRectF& zone, float t) {
    static const QColor cAmbre(0xf0, 0x9a, 0x2c);

    // La zone fait trois cases de large : c'est la case qui donne l'echelle du
    // debord comme de l'epaisseur, pour que l'onde suive la taille du plateau.
    qreal cote = zone.width() / 3.0;
    float e = 1.0f - (1.0f - t) * (1.0f - t);
    qreal debord = cote * 0.75 * e;
    QColor onde = cAmbre;

    onde.setAlpha((int)(220 * (1.0f - t)));

    painter.save();
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(onde, qMax(1.5, cote * 0.12 * (1.0f - t))));
    painter.drawRect(zone.adjusted(-debord, -debord, debord, debord));
    painter.restore();
}

// LE VERDICT, en grand sur le plateau. Par-dessus tout, voile compris : la
// partie est finie, le plateau derriere n'est plus qu'un decor -- mais un
// decor qu'on veut encore lire, d'ou le voile plutot qu'un aplat.
//
// La taille se cherche au lieu de se fixer : le mot doit tenir dans la grille,
// et la grille n'a pas la meme largeur selon l'ecran. Meme methode que le
// panneau (WPanneau::tailleQuiTient), pour que les deux grandissent ensemble.
//
// Le double trait -- contour sombre epais, puis remplissage -- est ce qui rend
// le mot lisible sur n'importe quel fond : sans lui, un tuyau clair sous une
// lettre claire la mange.
static void dessinerVerdict(QPainter& painter, const QRectF& grille, const QString& texte,
                            const QColor& couleur) {
    QFont police("monospace");

    police.setStyleHint(QFont::TypeWriter);
    police.setBold(true);

    // On vise les quatre cinquiemes de la largeur utile : le mot doit respirer
    // sur ses cotes, sinon il a l'air d'un bandeau et non d'un verdict.
    int large = (int)(grille.width() * 0.8);
    int retenue = 12;

    for(int essai = 13; essai <= 400; essai++) {
        police.setPixelSize(essai);

        if(QFontMetrics(police).horizontalAdvance(texte) > large) {
            break;
        }

        retenue = essai;
    }

    police.setPixelSize(retenue);

    QFontMetrics mesure(police);
    QPainterPath chemin;

    chemin.addText(grille.center().x() - mesure.horizontalAdvance(texte) / 2.0,
                   grille.center().y() + (mesure.ascent() - mesure.descent()) / 2.0,
                   police, texte);

    painter.save();
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0, 150));
    painter.drawRect(grille);

    painter.setPen(QPen(QColor(0, 0, 0, 220), qMax(2.0, retenue * 0.08),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(couleur);
    painter.drawPath(chemin);
    painter.restore();
}

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
    for(qreal d = -tuile.height(); d < tuile.width(); d += pas) {
        painter.drawLine(QPointF(tuile.left() + d, tuile.top()),
                         QPointF(tuile.left() + d + tuile.height(), tuile.bottom()));
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
    Minage *mines = partie->minage();
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
            ETypePiece type = plateau->getTypePiece(x, y);
            // La meche ne se consume que pour une bombe ; ailleurs la fraction
            // vaut -1 et la valeur par defaut fait l'affaire.
            float reste = type == tpBombe ? mines->fractionRestante(x, y) : 1.0f;

            dessinerPiece(painter, dest, type, plateau->getSens(x, y), reste);

            // Le compte a rebours par-dessus sa propre piece, et sous tout le
            // reste : ni le plan ni le tas ne peuvent occuper une case minee.
            if(type == tpBombe && reste >= 0.0f) {
                dessinerCompteARebours(painter, dest, reste);
            }
        }
    }

    // Le trace planifie, sous les tuyaux comme le plan : c'est une intention.
    // Il ne demande aucun bot -- il ne depend que du plateau.
    if(afficherTrace) {
        rafraichirTrace();

        const Trace *dessine = traceAffiche();

        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                ETypePiece voulu = dessine->type(x, y);

                if(voulu == tpNone) {
                    continue;
                }

                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);

                dessinerTrace(painter, dest, voulu, dessine->rang(x, y) == 0);
            }
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

    // Zone de souffle des bombes armees, par-dessus le liquide : ce qu'elle
    // annonce concerne justement les cases pleines.
    //
    // On ne demande pas sa liste au Minage : on relit le plateau. Une case
    // menacee retient la bombe la PLUS URGENTE qui la couvre -- c'est celle-la
    // qui decide du rythme, et deux bombes voisines ne doivent pas battre a
    // contretemps sur la meme case.
    if(mines->nbActives() > 0) {
        QVector<float> menace(plateau->getLargeur() * plateau->getHauteur(), 2.0f);

        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                if(plateau->getTypePiece(x, y) != tpBombe) {
                    continue;
                }

                float f = mines->fractionRestante(x, y);

                for(int dy=-1;dy<=1;dy++) {
                    for(int dx=-1;dx<=1;dx++) {
                        int nx = x + dx, ny = y + dy;

                        if(nx < 0 || nx >= plateau->getLargeur()
                           || ny < 0 || ny >= plateau->getHauteur()) {
                            continue;
                        }

                        float &m = menace[ny * plateau->getLargeur() + nx];
                        m = qMin(m, f);
                    }
                }
            }
        }

        for(int y=0;y<plateau->getHauteur();y++) {
            for(int x=0;x<plateau->getLargeur();x++) {
                float f = menace.at(y * plateau->getLargeur() + x);

                // Le reservoir est immunise : le teinter promettrait une
                // destruction qui n'aura pas lieu.
                if(f > 1.0f || plateau->getTypePiece(x, y) == tpReservoir) {
                    continue;
                }

                QRectF dest(x*spriteW + margeX, y*spriteH + margeY, spriteW, spriteH);
                dessinerSouffle(painter, dest, f, ecoul->estRempli(x, y));
            }
        }
    }

    // Previsualisation : ce que la bombe emporterait si on la posait sous le
    // curseur. Seulement la ou le clic droit aboutirait -- la question posee au
    // survol est exactement celle du clic, Partie::peutMiner pour les deux.
    if(partie->peutMiner(caseSurvolee.x(), caseSurvolee.y())) {
        for(int dy=-1;dy<=1;dy++) {
            for(int dx=-1;dx<=1;dx++) {
                int nx = caseSurvolee.x() + dx, ny = caseSurvolee.y() + dy;

                if(nx < 0 || nx >= plateau->getLargeur()
                   || ny < 0 || ny >= plateau->getHauteur()
                   || plateau->getTypePiece(nx, ny) == tpReservoir) {
                    continue;
                }

                QRectF dest(nx*spriteW + margeX, ny*spriteH + margeY, spriteW, spriteH);
                dessinerSouffleFantome(painter, dest, ecoul->estRempli(nx, ny));
            }
        }
    }

    // Curseur avant-dernier, pour rester au-dessus des tuyaux et du liquide.
    if(caseSurvolee.x() >= 0 && partie->peutPoser(caseSurvolee.x(), caseSurvolee.y())) {
        QRectF dest(caseSurvolee.x()*spriteW + margeX, caseSurvolee.y()*spriteH + margeY,
                    spriteW, spriteH);
        dessinerCurseur(painter, dest);
    }

    // Les explosions, au-dessus de tout le reste : ca dure un quart de seconde,
    // et pendant ce quart de seconde il n'y a rien d'autre a regarder. Le
    // plateau, lui, est deja pulverise -- ce qui suit ne montre que ce qui
    // vient de disparaitre.
    if(!flashs.isEmpty()) {
        qint64 maintenant = horlogeEcran.elapsed();
        QRectF grille(margeX, margeY,
                      plateau->getLargeur() * spriteW, plateau->getHauteur() * spriteH);

        painter.save();
        // Le souffle s'est arrete au bord du plateau (Minage::exploser borne le
        // 3x3) : l'onde qui le double ne doit pas deborder sur la marge noire.
        painter.setClipRect(grille);

        foreach(const SFlash &flash, flashs) {
            float t = qBound(0.0f, (maintenant - flash.debut) / (float)DUREE_FLASH_MS, 1.0f);
            int col = flash.idx % plateau->getLargeur();
            int row = flash.idx / plateau->getLargeur();

            for(int dy=-1;dy<=1;dy++) {
                for(int dx=-1;dx<=1;dx++) {
                    int nx = col + dx, ny = row + dy;

                    // Meme exception que partout ailleurs : le reservoir a
                    // survecu au souffle, le blanchir dirait le contraire.
                    if(nx < 0 || nx >= plateau->getLargeur()
                       || ny < 0 || ny >= plateau->getHauteur()
                       || plateau->getTypePiece(nx, ny) == tpReservoir) {
                        continue;
                    }

                    QRectF dest(nx*spriteW + margeX, ny*spriteH + margeY, spriteW, spriteH);
                    dessinerFlash(painter, dest, t);
                }
            }

            QRectF zone((col-1)*spriteW + margeX, (row-1)*spriteH + margeY,
                        3*spriteW, 3*spriteH);

            dessinerOndeFlash(painter, zone, t);
        }

        painter.restore();
    }

    // Et le verdict tout en haut de la pile, quand il y en a un. Les fins de
    // MANCHE n'en ont pas : le panneau les annonce deja, elles durent une
    // seconde, et masquer le plateau a chaque niveau rendrait la partie
    // illisible. Seules les fins de PARTIE s'affichent ici -- elles ne
    // s'enchainent sur rien, on reste dessus.
    if(partie->etat() == epGameOver || partie->etat() == epAbandon) {
        static const QColor cPerdu(0xd8, 0x50, 0x40);
        static const QColor cAbandon(0xc8, 0xa8, 0x40);

        QRectF grille(margeX, margeY,
                      plateau->getLargeur() * spriteW, plateau->getHauteur() * spriteH);

        dessinerVerdict(painter, grille,
                        partie->etat() == epGameOver ? tr("GAME OVER") : tr("ABANDON"),
                        partie->etat() == epGameOver ? cPerdu : cAbandon);
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

    // Le bouton droit pose une bombe, le gauche un tuyau. Tout autre bouton ne
    // fait rien : sans ce filtre, un clic du milieu deposerait une piece.
    if(event->button() == Qt::RightButton) {
        // Meme partage qu'au-dessus : le widget traduit un clic, Partie decide
        // -- stock vide, manche finie, case occupee.
        bool accepte = partie->poserBombe(col, row);

        if(accepte) {
            repaint();
        }

        emit bombePosee(col, row, accepte);
        return;
    }

    if(event->button() != Qt::LeftButton) {
        return;
    }

    // Les regles du coup (case interdite, penalite de remplacement) sont dans
    // Partie : le widget ne fait que traduire un clic en coordonnees.
    bool accepte = partie->poserPiece(col, row);

    if(accepte) {
        repaint();
    }

    emit pieceDeposee(col, row, accepte);
}
