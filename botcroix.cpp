#include <QtDebug>

#include "botcroix.h"

// Rayon de la fouille autour de la tete, en pas orthogonaux. Deux, comme decide
// avec le user : "une ou deux cases devant elle". Ce n'est pas qu'une borne de
// cout -- c'est ce qui garde l'echange LOCAL. Au-dela, le bot irait ouvrir des
// passages loin devant, sur un trace qui ne passera peut-etre jamais par la,
// en payant 25 points a chaque fois.
//
// "Devant" n'a pas besoin d'etre verifie : une croix posee derriere la tete
// n'augmente pas sa portee, donc le critere de gain l'ecarte tout seul.
#define RAYON_ECHANGE 2

// Gain minimal pour que l'echange se joue. DEUX, et le "un" exclu n'est pas un
// reglage : c'est du bruit de mesure.
//
// Dans la phase de comptage d'espaceApres, un droit pose est un mur -- "une case
// vide ou du tas se propage aux quatre cotes ; une croix deja posee ne se
// traverse que tout droit ; tout le reste arrete le parcours". Echanger un droit
// DEJA ATTEIGNABLE ouvre donc son second axe et rend +1, toujours, sans qu'aucun
// terrain nouveau soit gagne. Un gain de 1 ne dit rien.
//
// Ce qu'on cherche est l'autre cas : le mur devient passage et la region qui
// etait derriere s'ouvre d'un coup. Le gain vaut alors 1 pour le croisement plus
// la taille de cette region, et il est franc.
#ifndef GAIN_ECHANGE_MINIMAL
#define GAIN_ECHANGE_MINIMAL 2
#endif

BotCroix::BotCroix(Partie *p, float cadence, quint32 seed) : BotMemoire(p, cadence, seed) {
}

int BotCroix::porteeTete(int col, int row, ESens entree) const {
    int meilleure = 0;

    foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
        meilleure = qMax(meilleure, espaceApres(type, col, row, entree, 0));
    }

    return meilleure;
}

bool BotCroix::droitEchangeable(int col, int row) const {
    ETypePiece type = p->plateau()->getTypePiece(col, row);

    if(type != tpHorizontal && type != tpVertical) {
        return false;
    }

    // Le flux y est-il deja passe ? estRempli est vrai des qu'UN des deux axes
    // est pris, donc une croix a moitie traversee ne se retouche plus non plus.
    // C'est toute la fenetre de l'echange : en amont du flux, et pas apres.
    return p->peutPoser(col, row);
}

int BotCroix::porteeSiCroix(int cCol, int cRow, int tCol, int tRow, ESens tEntree) const {
    Game *plateau = p->plateau();

    ETypePiece avant = plateau->getTypePiece(cCol, cRow);
    ESens sensAvant = plateau->getSens(cCol, cRow);

    plateau->setTypePiece(cCol, cRow, tpCroix);

    int portee = porteeTete(tCol, tRow, tEntree);

    plateau->setTypePiece(cCol, cRow, avant);
    plateau->setSens(cCol, cRow, sensAvant);

    return portee;
}

bool BotCroix::coupSpecial(int col, int row, ESens entree) {
    // On ne pose que le haut de file : sans croix au sommet, il n'y a rien a
    // decider. C'est aussi ce qui rend la fouille bon marche -- un geste sur
    // sept, le tirage etant uniforme sur les sept types.
    if(p->file()->getPiece(0).type != tpCroix) {
        return false;
    }

    // La croix est le connecteur universel : piecesCompatibles la rend pour les
    // QUATRE entrees, donc une croix en haut de file se pose toujours sur la
    // tete. Si c'est justement le pont que le v3 attendait, on ne la detourne
    // pas -- l'echange peut attendre la croix suivante, la tete non.
    //
    // Sans cette garde, mesure a --graine 1910881835 --niveau 31 : 22 traversees
    // contre 114 pour le v4. Indolore aux premiers niveaux ou la marge est
    // enorme, fatal des que l'objectif serre la tete.
    if(choisirPont(col, row, entree, true) == 0) {
        return false;
    }

    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();
    int reference = porteeTete(col, row, entree);

    int meilleurCol = -1;
    int meilleurRow = -1;
    int meilleurePortee = reference;
    int meilleureDistance = 0;

    for(int dy = -RAYON_ECHANGE; dy <= RAYON_ECHANGE; dy++) {
        for(int dx = -RAYON_ECHANGE; dx <= RAYON_ECHANGE; dx++) {
            int distance = qAbs(dx) + qAbs(dy);

            if(distance == 0 || distance > RAYON_ECHANGE) {
                continue;
            }

            int x = col + dx;
            int y = row + dy;

            if(x < 0 || x >= largeur || y < 0 || y >= hauteur || !droitEchangeable(x, y)) {
                continue;
            }

            int portee = porteeSiCroix(x, y, col, row, entree);

            // Strictement plus loin, et a gain egal la case la plus proche : le
            // trace y arrivera plus tot, donc l'echange a moins de chances
            // d'avoir ete paye pour rien.
            if(portee < reference + GAIN_ECHANGE_MINIMAL) {
                continue;
            }

            if(portee > meilleurePortee
               || (portee == meilleurePortee && meilleurCol >= 0 && distance < meilleureDistance)) {
                meilleurCol = x;
                meilleurRow = y;
                meilleurePortee = portee;
                meilleureDistance = distance;
            }
        }
    }

    if(meilleurCol < 0) {
        return false;
    }

    // poserPiece et non poserCaseTas : la case garde le statut qu'elle avait.
    // L'inscrire au tas la rendrait reprenable alors qu'elle est, le plus
    // souvent, du trace deja construit -- et fausserait toutes les mesures de
    // place qui comptent le tas comme libre.
    if(!p->poserPiece(meilleurCol, meilleurRow)) {
        return false;
    }

    qDebug() << "croix : (" << meilleurCol << "," << meilleurRow << ") echangee --"
             << "portee de la tete (" << col << "," << row << ")"
             << reference << "->" << meilleurePortee;

    return true;
}
