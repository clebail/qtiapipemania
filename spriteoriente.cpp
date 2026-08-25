#include "spriteoriente.h"

SpriteOriente::SpriteOriente(ESens sens) : m_sens(sens) {
}

ESens SpriteOriente::getSens() const {
    return m_sens;
}
