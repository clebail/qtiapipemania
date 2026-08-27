#ifndef BOTSPACE_H
#define BOTSPACE_H

#include "bot.h"

// v2. Le glouton, mais qui trie ses poses. Trois etages, du plus exigeant au
// plus resigne :
//
//   - la piece raccorde la tete et laisse derriere elle de quoi boucler
//     l'objectif (culDeSac) : on pose ;
//   - sinon, tant que le flux ne talonne pas ET qu'un type existe encore qui
//     passerait ce filtre (ouvertureUtile), on defausse et on attend mieux ;
//   - sinon on retombe sur le seul "ne pas mourir sur le coup" (meneALaMort),
//     et faute de mieux on abandonne et on fonce.
//
// Le deuxieme etage est celui qui fait le bot. Sans sa condition de sortie --
// aucune pioche ne sauve cette tete -- l'exigence tournerait en defausse
// perpetuelle et couterait la manche qu'elle protege ; sans lui tout court, le
// bot s'engage dans des culs-de-sac qu'il voit venir.
class BotSpace : public Bot {
public:
    BotSpace(Partie *p, float cadence, quint32 seed);
protected:
    void jouer(float dt) override;
};

#endif // BOTSPACE_H
