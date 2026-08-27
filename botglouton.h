#ifndef BOTGLOUTON_H
#define BOTGLOUTON_H

#include "bot.h"

// Reference basse : a chaque geste il regarde la piece du haut de la file. Si
// son type ne peut pas se raccorder a l'entree de la tete, il la defausse et
// recommence au geste suivant. Sinon il la pose sur la tete -- et si cette pose
// bute sur un mur ou une piece qu'il ne peut ni traverser ni reprendre, il la
// pose quand meme puis fonce. Aucun choix entre les types viables, aucune
// notion de place : c'est la borne basse contre laquelle tout le reste se
// mesure.
class BotGlouton : public Bot {
public:
    BotGlouton(Partie *p, float cadence, quint32 seed);
protected:
    void jouer(float dt) override;
};

#endif // BOTGLOUTON_H
