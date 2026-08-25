#ifndef COMMON_H
#define COMMON_H

#define SPRITE_WIDTH                27
#define SPRITE_HEIGHT               27
#define SPRITE_SCALE                2

typedef enum _ETypePiece {
    tpNone, tpReservoir, tpHorizontal, tpVertical, tpCoudeHautGauche, tpCoudeHautDroite, tpCoudeBasGauche, tpCoudeBasDroite, tpCroix, tpBombe
} ETypePiece;

typedef enum _ESens {
    sHaut, sBas, sGauche, sDroite
} ESens;

#endif // COMMON_H
