#include "dessinpiece.h"
#include "ecoulement.h"

// Palette retro : bleus satures et cyan franc, comme le jeu d'origine, mais
// tracee proprement au lieu d'etre pixelisee.
static const QColor cFond        (0x0b, 0x0b, 0x14);
static const QColor cGrille      (0x1b, 0x1b, 0x2c);
static const QColor cContour     (0x14, 0x14, 0x3c);
static const QColor cCorps       (0x3a, 0x3a, 0xc8);
static const QColor cReflet      (0x6e, 0xd8, 0xff);
static const QColor cCanal       (0x08, 0x08, 0x16);
static const QColor cLiquide     (0x2f, 0xbf, 0x4f);
static const QColor cLiquideVif  (0x8c, 0xf5, 0x9e);
static const QColor cCuve        (0xf0, 0xc0, 0x20);
static const QColor cCuveOmbre   (0x8a, 0x66, 0x00);
static const QColor cBombe       (0x2a, 0x2a, 0x32);
static const QColor cMeche       (0xd8, 0x50, 0x30);

// Proportions du tuyau, en fraction de la taille d'une case.
static const qreal fContour = 0.46;
static const qreal fCorps   = 0.40;
static const qreal fReflet  = 0.30;
static const qreal fCanal   = 0.20;
static const qreal fCuve    = 0.27;

static bool estCoude(ETypePiece type) {
    return type >= tpCoudeHautGauche && type <= tpCoudeBasDroite;
}

static QPointF milieuBord(const QRectF& t, ESens sens) {
    switch(sens) {
    case sHaut:   return QPointF(t.center().x(), t.top());
    case sBas:    return QPointF(t.center().x(), t.bottom());
    case sGauche: return QPointF(t.left(), t.center().y());
    case sDroite: return QPointF(t.right(), t.center().y());
    }

    return QPointF();
}

QPainterPath cheminTuyau(const QRectF& tuile, ETypePiece type, ESens sens, ESens entree, float p) {
    QPainterPath chemin;
    QPointF centre = tuile.center();
    QVector<ESens> ouv = Ecoulement::ouvertures(type, sens);

    if(ouv.isEmpty() || p <= 0.0f) {
        return chemin;
    }

    if(type == tpReservoir) {
        // Pas de demi-segment d'entree : le liquide sort de la cuve. Il demarre
        // juste sous le bord du rond (qui est redessine par-dessus), pour en
        // paraitre issu au lieu de le traverser, et parcourt le reste de la
        // case sur toute la duree.
        //   fCuve * 2 = rayon de la cuve rapporte a la demi-case ; le facteur
        //   0.9 fait mordre le depart sous le rond, sans jointure visible.
        qreal depuisCentre = fCuve * 2.0 * 0.9;

        foreach(ESens sortie, ouv) {
            QPointF bord = milieuBord(tuile, sortie);
            QPointF depart = centre + (bord - centre) * depuisCentre;
            chemin.moveTo(depart);
            chemin.lineTo(depart + (bord - depart) * p);
        }

        return chemin;
    }

    if(estCoude(type)) {
        // Quart de cercle de rayon une demi-case, centre sur le coin commun aux
        // deux ouvertures : il passe exactement par le milieu des deux bords.
        ESens vertical = (type == tpCoudeHautGauche || type == tpCoudeHautDroite) ? sHaut : sBas;
        ESens horizontal = (type == tpCoudeHautGauche || type == tpCoudeBasGauche) ? sGauche : sDroite;

        qreal rayon = tuile.width() / 2.0;
        QPointF coin(horizontal == sGauche ? tuile.left() : tuile.right(),
                     vertical == sHaut ? tuile.top() : tuile.bottom());
        QRectF cercle(coin.x() - rayon, coin.y() - rayon, 2*rayon, 2*rayon);

        qreal angleVertical = (horizontal == sGauche) ? 0.0 : 180.0;
        qreal angleHorizontal = (vertical == sHaut) ? 270.0 : 90.0;
        // Sens de parcours qui garde l'arc a l'interieur de la case.
        qreal balayage = ((horizontal == sGauche) == (vertical == sHaut)) ? -90.0 : 90.0;

        qreal depart = (entree == vertical) ? angleVertical : angleHorizontal;
        if(entree != vertical) {
            balayage = -balayage;
        }

        chemin.arcMoveTo(cercle, depart);
        chemin.arcTo(cercle, depart, balayage * p);

        return chemin;
    }

    // Droits et croix : demi-segment d'entree, puis demi-segments de sortie.
    QPointF depart = milieuBord(tuile, entree);
    chemin.moveTo(depart);
    chemin.lineTo(depart + (centre - depart) * qMin(p * 2.0f, 1.0f));

    if(p > 0.5f) {
        float ratioSortie = (p - 0.5f) * 2.0f;

        foreach(ESens sortie, ouv) {
            if(sortie != entree) {
                chemin.moveTo(centre);
                chemin.lineTo(centre + (milieuBord(tuile, sortie) - centre) * ratioSortie);
            }
        }
    }

    return chemin;
}

static void dessinerCuve(QPainter& painter, const QRectF& tuile) {
    qreal rayon = tuile.width() * fCuve;
    QRectF cuve(tuile.center().x() - rayon, tuile.center().y() - rayon, 2*rayon, 2*rayon);

    painter.setPen(QPen(cCuveOmbre, tuile.width() * 0.05));
    painter.setBrush(cCuve);
    painter.drawEllipse(cuve);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 90));
    painter.drawEllipse(cuve.adjusted(rayon*0.35, rayon*0.3, -rayon*1.1, -rayon*1.15));
}

static void dessinerBombe(QPainter& painter, const QRectF& tuile) {
    qreal rayon = tuile.width() * 0.28;
    QRectF corps(tuile.center().x() - rayon, tuile.center().y() - rayon*0.9, 2*rayon, 2*rayon);

    painter.setPen(QPen(QColor(0x50, 0x50, 0x60), tuile.width() * 0.04));
    painter.setBrush(cBombe);
    painter.drawEllipse(corps);

    QPainterPath meche;
    meche.moveTo(corps.center().x() + rayon*0.4, corps.top() + rayon*0.2);
    meche.quadTo(corps.center().x() + rayon*1.1, corps.top() - rayon*0.6,
                 corps.center().x() + rayon*0.3, corps.top() - rayon*0.9);
    painter.strokePath(meche, QPen(cMeche, tuile.width() * 0.06));

    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(255, 255, 255, 70));
    painter.drawEllipse(corps.adjusted(rayon*0.35, rayon*0.3, -rayon*1.1, -rayon*1.15));
}

static void tracerPiece(QPainter& painter, const QRectF& tuile, ETypePiece type, ESens sens) {
    painter.save();
    painter.fillRect(tuile, cFond);
    painter.setPen(QPen(cGrille, 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(tuile.adjusted(0.5, 0.5, -0.5, -0.5));

    if(type == tpNone) {
        painter.restore();
        return;
    }

    if(type == tpBombe) {
        dessinerBombe(painter, tuile);
        painter.restore();
        return;
    }

    QVector<ESens> ouv = Ecoulement::ouvertures(type, sens);
    QPainterPath chemin = cheminTuyau(tuile, type, sens, ouv.first(), 1.0f);
    qreal taille = tuile.width();

    // Couches concentriques du plus large au plus etroit : contour, corps,
    // reflet, puis le canal creux dans lequel coulera le liquide.
    const struct { qreal fraction; QColor couleur; } couches[] = {
        { fContour, cContour },
        { fCorps,   cCorps   },
        { fReflet,  cReflet  },
        { fCanal,   cCanal   },
    };

    for(const auto& couche : couches) {
        painter.strokePath(chemin, QPen(couche.couleur, taille * couche.fraction,
                                        Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    }

    if(type == tpReservoir) {
        dessinerCuve(painter, tuile);
    }

    painter.restore();
}

void dessinerPiece(QPainter& painter, const QRectF& tuile, ETypePiece type, ESens sens) {
    // Une piece ne change pas d'aspect : on la trace une fois par couple
    // (type, sens) et par taille, puis on recopie l'image. Le trace vectoriel
    // ne coute donc rien pendant l'animation de l'ecoulement.
    static QHash<quint32, QPixmap> cache;
    static int tailleCache = 0;

    int taille = qRound(tuile.width());
    if(taille != tailleCache) {
        cache.clear();
        tailleCache = taille;
    }

    quint32 cle = ((quint32)type << 8) | (quint32)sens;
    auto trouve = cache.constFind(cle);

    if(trouve == cache.constEnd()) {
        QPixmap image(taille, taille);
        QPainter peintre(&image);
        peintre.setRenderHint(QPainter::Antialiasing, true);
        tracerPiece(peintre, QRectF(0, 0, taille, taille), type, sens);
        trouve = cache.insert(cle, image);
    }

    painter.drawPixmap(tuile.topLeft(), *trouve);
}

void dessinerLiquide(QPainter& painter, const QRectF& tuile, ETypePiece type, ESens sens,
                     ESens entree, float progression) {
    if(progression <= 0.0f || type == tpNone || type == tpBombe) {
        return;
    }

    QPainterPath chemin = cheminTuyau(tuile, type, sens, entree, progression);
    qreal taille = tuile.width();

    painter.save();
    // Meme largeur que le canal : le liquide le remplit exactement, par
    // construction, puisque c'est le meme chemin.
    painter.strokePath(chemin, QPen(cLiquide, taille * fCanal,
                                    Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));
    painter.strokePath(chemin, QPen(cLiquideVif, taille * fCanal * 0.35,
                                    Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin));

    if(type == tpReservoir) {
        // La cuve repasse par-dessus : le depart du liquide, coupe droit, se
        // trouve cache sous le rond, qu'il epouse donc exactement.
        dessinerCuve(painter, tuile);
    }

    painter.restore();
}
