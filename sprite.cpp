#include "sprite.h"
#include "spritenone.h"
#include "spritehorizontal.h"
#include "spritevertical.h"
#include "spritecoudehautgauche.h"
#include "spritecoudehautdroite.h"
#include "spritecoudebasgauche.h"
#include "spritecoudebasdroite.h"
#include "spritecroix.h"
#include "spritereservoir.h"
#include "spritebombe.h"

Sprite::Sprite() {}

Sprite* Sprite::create(ETypePiece type, ESens sens) {
    switch (type) {
    case tpHorizontal:
        return new SpriteHorizontal();
    case tpVertical:
        return new SpriteVertical();
    case tpCoudeHautGauche:
        return new SpriteCoudeHautGauche();
    case tpCoudeHautDroite:
        return new SpriteCoudeHautDroite();
    case tpCoudeBasGauche:
        return new SpriteCoudeBasGauche();
    case tpCoudeBasDroite:
        return new SpriteCoudeBasDroite();
    case tpCroix:
        return new SpriteCroix();
    case tpReservoir:
        return new SpriteReservoir(sens);
    case tpBombe:
        return new SpriteBombe(sens);
    case tpNone:
        return new SpriteNone();
    }

    return nullptr;
}
