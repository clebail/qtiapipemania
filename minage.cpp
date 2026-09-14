#include "minage.h"

Minage::Minage(Game *plateau, const Ecoulement *ecoul) {
    this->plateau = plateau;
    this->ecoul = ecoul;
}

bool Minage::poser(int col, int row) {
    if(col < 0 || col >= plateau->getLargeur() || row < 0 || row >= plateau->getHauteur()) {
        return false;
    }

    // Totalement vide, et rien d'autre. Une bombe ne remplace pas : elle ne se
    // pose que la ou il n'y a rien a detruire sous elle.
    if(plateau->getTypePiece(col, row) != tpNone) {
        return false;
    }

    plateau->setTypePiece(col, row, tpBombe);
    bombes << SBombe { row * plateau->getLargeur() + col, DUREE_BOMBE };

    return true;
}

int Minage::indiceDe(int idx) const {
    for(int i = 0; i < bombes.size(); i++) {
        if(bombes.at(i).idx == idx) {
            return i;
        }
    }

    return -1;
}

void Minage::retirer(int idx) {
    int i = indiceDe(idx);

    if(i >= 0) {
        bombes.remove(i);
    }
}

bool Minage::avancer(float dt) {
    bool mortelle = false;

    for(int i = 0; i < bombes.size(); i++) {
        bombes[i].restant -= dt;
    }

    // Resolution dans l'ordre de pose, une bombe a la fois. La bombe qui saute
    // quitte la liste AVANT son propre souffle : elle ne peut donc pas
    // s'annuler elle-meme, et les bombes que le souffle emporte, elles, ne
    // s'appliqueront jamais -- c'est la regle "pas de chaine". La boucle se
    // termine, chaque tour retirant au moins une bombe.
    for(;;) {
        int i = -1;

        for(int j = 0; j < bombes.size() && i < 0; j++) {
            if(bombes.at(j).restant <= 0.0f) {
                i = j;
            }
        }

        if(i < 0) {
            break;
        }

        int idx = bombes.at(i).idx;
        bombes.remove(i);

        // L'ordre compte : une explosion mortelle ne doit pas effacer le
        // souvenir d'une precedente qui l'etait aussi.
        mortelle = exploser(idx) || mortelle;
    }

    return mortelle;
}

bool Minage::exploser(int idx) {
    int largeur = plateau->getLargeur();
    int hauteur = plateau->getHauteur();
    int col = idx % largeur;
    int row = idx / largeur;

    QVector<int> ouverts;
    bool mortelle = false;

    // Note prise avant meme de pulveriser : ce qui suit efface toute trace de
    // l'evenement, et l'affichage n'aurait plus rien a quoi se raccrocher.
    explosions << idx;

    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            int x = col + dx;
            int y = row + dy;

            if(x < 0 || x >= largeur || y < 0 || y >= hauteur) {
                continue;
            }

            ETypePiece t = plateau->getTypePiece(x, y);

            // Le reservoir est immunise, et ce test vient AVANT celui du tuyau
            // plein : il est rempli des que le flux est parti, et le compter
            // tuerait toute bombe posee dans son voisinage.
            if(t == tpReservoir) {
                continue;
            }

            // Un tuyau plein dans le souffle, et la manche est finie. On ne
            // sort pas pour autant : le 3x3 part en entier, la mort n'empeche
            // pas l'explosion d'avoir lieu.
            if(ecoul->estRempli(x, y)) {
                mortelle = true;
            }

            if(t == tpBloque) {
                ouverts << y * largeur + x;
            }

            // Une bombe prise dans le souffle saute avec lui et n'en produit
            // aucun : elle quitte simplement la liste. C'est tout ce que "sans
            // proliferation" demande d'ecrire -- aucune recursion.
            if(t == tpBombe) {
                retirer(y * largeur + x);
            }

            plateau->setTypePiece(x, y, tpNone);
        }
    }

    // Le deminage ne s'acquiert que sur une explosion qui ne tue pas. Sans
    // cette exception, bombarder son propre tuyau plein serait la facon la
    // moins chere d'ouvrir le terrain : une vie contre des blocs, et la vie se
    // regagne. Ici, se tuer avec sa bombe annule ce qu'elle rapportait.
    if(!mortelle) {
        demine += ouverts;
    }

    return mortelle;
}

void Minage::viderManche() {
    bombes.clear();
    // Une explosion que personne n'a relevee avant la fin de la manche n'a plus
    // de plateau ou s'afficher : le flash tomberait sur une grille neuve.
    explosions.clear();
}

void Minage::viderNiveau() {
    bombes.clear();
    demine.clear();
}

void Minage::appliquerDeminage() const {
    int largeur = plateau->getLargeur();

    foreach(int idx, demine) {
        // On n'efface qu'un bloc : la meme graine repose les memes cases, mais
        // rien ne garantit qu'un jour un autre tirage ne mettra pas autre chose
        // ici -- et on ne veut effacer que ce qu'on a ouvert.
        if(plateau->getTypePiece(idx % largeur, idx / largeur) == tpBloque) {
            plateau->setTypePiece(idx % largeur, idx / largeur, tpNone);
        }
    }
}

float Minage::fractionRestante(int col, int row) const {
    int i = indiceDe(row * plateau->getLargeur() + col);

    if(i < 0) {
        return -1.0f;
    }

    return qBound(0.0f, bombes.at(i).restant / DUREE_BOMBE, 1.0f);
}

int Minage::nbActives() const {
    return bombes.size();
}

QVector<int> Minage::preleverExplosions() {
    QVector<int> releve = explosions;

    explosions.clear();

    return releve;
}
