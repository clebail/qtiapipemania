#ifndef SPRITEORIENTE_H
#define SPRITEORIENTE_H

#include "sprite.h"

class SpriteOriente : public Sprite
{
public:
    SpriteOriente(ESens sens);

    ESens getSens() const;

protected:
    ESens m_sens;
};

#endif // SPRITEORIENTE_H
