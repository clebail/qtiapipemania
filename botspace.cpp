#include "botspace.h"

BotSpace::BotSpace(Partie *p, float cadence, quint32 seed) : Bot(p, cadence, seed) {
}

void BotSpace::jouer(float dt) {
    if(!prendreJeton(dt)) {
        return;
    }

    int col, row;
    ESens entree;

    if(!tete(col, row, entree)) {
        // Plus de tete : rien a construire, on deroule la fin de manche.
        demanderFoncer();
        return;
    }

    Piece piece = p->file()->getPiece(0);
    bool compatible = Ecoulement::piecesCompatibles(entree).contains(piece.type);

    // Le bon cas : une pose qui raccorde la tete, ne la condamne pas sur le
    // coup, et laisse derriere elle de quoi boucler l'objectif.
    if(compatible && !culDeSac(piece.type, col, row, entree)) {
        p->poserPiece(col, row);
        return;
    }

    // Attendre une meilleure pioche ne vaut que si elle existe. Si aucun type
    // ne passe le filtre complet sur cette tete, toutes les defausses du monde
    // n'y changeront rien : on aurait depense la manche a ne rien construire,
    // et l'exigence aurait coute la manche qu'elle pretendait proteger. Marge de
    // deux gestes, comme avant : celui qu'on gaspille a defausser, plus celui
    // qu'il faut garder pour poser vraiment quelque chose ensuite.
    if(!acculeParLeFlux(2.0f) && ouvertureUtile(col, row, entree) > 0) {
        defausser();
        return;
    }

    // Plus le luxe d'etre exigeant -- flux qui talonne, ou tete a l'etroit dont
    // aucune pioche ne sortira. On revient au critere du v2 d'avant : une pose
    // qui ne tue pas sur le coup vaut mieux qu'une defausse de plus.
    if(compatible && !meneALaMort(piece.type, col, row, entree)) {
        p->poserPiece(col, row);
        return;
    }

    // La piece ne va meme pas a ce compte-la : on attend, tant qu'une pioche
    // peut encore prolonger la tete et que le flux laisse le temps.
    if(!acculeParLeFlux(2.0f) && !teteCondamnee(col, row, entree)) {
        defausser();
        return;
    }

    // Flux qui rattrape, ou tete sans issue : plus rien a trier, on pose ce
    // qu'on peut et on fonce.
    abandonner(col, row, entree);
}
