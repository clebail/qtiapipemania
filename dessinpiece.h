#ifndef DESSINPIECE_H
#define DESSINPIECE_H

#include <QPainter>
#include <QPainterPath>
#include <QRectF>
#include <QPixmap>
#include <QHash>
#include "common.h"

// Les pieces sont tracees en vectoriel plutot que decoupees dans une image :
// la ligne mediane du tuyau est ainsi connue par construction, donc le liquide
// la suit exactement. Le rendu est aussi independant de la resolution.

// Ligne mediane d'une piece, tronquee a la fraction p (0 a 1) en partant du
// cote `entree`. Avec p = 1 on obtient la piece entiere.
// suivreLeFlux : restreint le trace aux sorties reellement empruntees par le
// fluide (une croix est alors traversee tout droit). A false, on obtient la
// geometrie complete de la piece, celle qu'il faut dessiner.
QPainterPath cheminTuyau(const QRectF& tuile, ETypePiece type, ESens sens, ESens entree, float p,
                         bool suivreLeFlux = false);

// `meche` : ce qu'il reste a bruler sur une bombe, de 1 a la pose a 0 quand
// elle part. Ignore par tout autre type. Valeur par defaut pour les appelants
// qui dessinent une piece sans notion de temps -- la file, le plan.
void dessinerPiece(QPainter& painter, const QRectF& tuile, ETypePiece type, ESens sens,
                   float meche = 1.0f);
void dessinerLiquide(QPainter& painter, const QRectF& tuile, ETypePiece type, ESens sens,
                     ESens entree, float progression);

#endif // DESSINPIECE_H
