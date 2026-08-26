#include <QRandomGenerator>
#include <string.h>
#include "ecoulement.h"

static ESens sensReciproques[] = {sBas, sHaut, sDroite, sGauche};
static SDelta deltas[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

Ecoulement::Ecoulement(const Game *plateau) {
    int size = plateau->getSize();

    remplis = new unsigned char[size];
    progressions = new float[size];
    entrees = new unsigned char[size];

    this->plateau = plateau;

    reinitialiser();
}

Ecoulement::~Ecoulement() {
    delete[] remplis;
    delete[] progressions;
    delete[] entrees;
}

void Ecoulement::reinitialiser() {
    int size = plateau->getSize();

    memset(remplis, 0, size*sizeof(*remplis));
    memset(progressions, 0, size*sizeof(*progressions));
    memset(entrees, 0, size*sizeof(*entrees));

    front.clear();
    casesTraversees = 0;
}

void Ecoulement::demarrer() {
    remplis[plateau->getIdxDepart()] = true;
    entrees[plateau->getIdxDepart()] = (unsigned char)plateau->getSens(plateau->getIdxDepart());

    front << plateau->getIdxDepart();
    // Le reservoir est la source, pas une case parcourue : il ne compte pas.
}

int Ecoulement::nbCasesTraversees() const {
    return casesTraversees;
}

void Ecoulement::setDureeRemplissage(float secondes) {
    dureeRemplissage = secondes;
}

EEtat Ecoulement::avancer(float dt) {
    if(front.isEmpty()) {
        return eTermine;
    }

    int largeur = plateau->getLargeur();
    int hauteur = plateau->getHauteur();
    QVector<int> frontSuivant;
    bool fuite = false;

    foreach(int idx, front) {
        progressions[idx] += dt / dureeRemplissage;

        if(progressions[idx] < 1.0f) {
            // Case encore en cours de remplissage : elle reste dans le front.
            frontSuivant << idx;
            continue;
        }

        progressions[idx] = 1.0f;

        int x = idx % largeur;
        int y = idx / largeur;

        foreach(ESens ouverture, sorties(plateau->getTypePiece(x, y),
                                         plateau->getSens(x, y),
                                         (ESens)entrees[idx])) {
            ESens sensEntre = sensReciproques[(unsigned char)ouverture];
            int nextX = x + deltas[(unsigned char)ouverture].dx;
            int nextY = y + deltas[(unsigned char)ouverture].dy;

            if(nextX < 0 || nextX >= largeur || nextY < 0 || nextY >= hauteur) {
                // Ouverture donnant hors grille : le fluide se deverse.
                fuite = true;
                continue;
            }

            int nextIdx = nextY * largeur + nextX;
            ETypePiece nextTypePiece = plateau->getTypePiece(nextX, nextY);
            ESens nextSens = plateau->getSens(nextX, nextY);

            if(!ouvertures(nextTypePiece, nextSens).contains(sensEntre)) {
                // La voisine ne presente aucune ouverture en face : le fluide
                // se deverse la aussi (bout de tuyau ouvert).
                fuite = true;
                continue;
            }

            if(remplis[nextIdx]) {
                // Deja revendiquee : case d'ou l'on vient, ou branches qui se
                // rejoignent apres une boucle.
                continue;
            }

            remplis[nextIdx] = true;
            entrees[nextIdx] = (unsigned char)sensEntre;
            progressions[nextIdx] = 0.0f;
            casesTraversees++;

            frontSuivant << nextIdx;
        }
    }

    // Echange du front : les cases ajoutees pendant cet appel n'avancent qu'au
    // prochain, ce qui fait bien progresser toutes les branches d'une croix
    // ensemble plutot que l'une apres l'autre.
    front = frontSuivant;

    if(fuite) {
        return eFuite;
    }

    return front.isEmpty() ? eTermine : eEnCours;
}

bool Ecoulement::enCours() const {
    return !front.isEmpty();
}

int Ecoulement::casesEnAval() const {
    int largeur = plateau->getLargeur();
    int hauteur = plateau->getHauteur();
    int size = plateau->getSize();

    // Meme parcours que avancer(), mais a blanc : on part du front courant avec
    // une copie du visite, donc l'etat de l'ecoulement n'est pas touche.
    QVector<unsigned char> vus(size);
    memcpy(vus.data(), remplis, size*sizeof(*remplis));

    QVector<int> aVoir = front;
    int compte = 0;

    while(!aVoir.isEmpty()) {
        int idx = aVoir.takeLast();
        int x = idx % largeur;
        int y = idx / largeur;

        foreach(ESens ouverture, sorties(plateau->getTypePiece(x, y),
                                         plateau->getSens(x, y),
                                         (ESens)entrees[idx])) {
            ESens sensEntre = sensReciproques[(unsigned char)ouverture];
            int nextX = x + deltas[(unsigned char)ouverture].dx;
            int nextY = y + deltas[(unsigned char)ouverture].dy;

            if(nextX < 0 || nextX >= largeur || nextY < 0 || nextY >= hauteur) {
                continue;
            }

            int nextIdx = nextY * largeur + nextX;

            if(vus[nextIdx]) {
                continue;
            }

            if(!ouvertures(plateau->getTypePiece(nextX, nextY),
                           plateau->getSens(nextX, nextY)).contains(sensEntre)) {
                continue;
            }

            vus[nextIdx] = true;
            compte++;
            aVoir << nextIdx;
        }
    }

    return compte;
}

bool Ecoulement::estRempli(int col, int row) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()) {
        int idx = row * plateau->getLargeur() + col;

        return remplis[idx];
    }

    return false;
}

float Ecoulement::progression(int col, int row) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()) {
        int idx = row * plateau->getLargeur() + col;

        return progressions[idx];
    }

    return 0.0;
}

ESens Ecoulement::entree(int col, int row) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()) {
        int idx = row * plateau->getLargeur() + col;

        return (ESens)entrees[idx];
    }

    return sHaut;
}

QVector<ESens> Ecoulement::sorties(const ETypePiece& typePiece, const ESens& sens, const ESens& entree) {
    QVector<ESens> ouv = ouvertures(typePiece, sens);
    QVector<ESens> resultat;

    // Le reservoir n'a pas d'entree : ce qui y est enregistre est sa sortie.
    if(typePiece == tpReservoir) {
        return ouv;
    }

    // La croix est un croisement, pas un carrefour : le flux la traverse tout
    // droit. La faire diverger vers ses trois autres cotes ouvrirait a chaque
    // fois deux branches presque jamais raccordees, donc une fuite immediate :
    // la piece deviendrait un piege au lieu d'un moyen de croiser son tuyau.
    if(typePiece == tpCroix) {
        ESens tout_droit = sensReciproques[(unsigned char)entree];

        if(ouv.contains(tout_droit)) {
            resultat << tout_droit;
        }

        return resultat;
    }

    foreach(ESens ouverture, ouv) {
        if(ouverture != entree) {
            resultat << ouverture;
        }
    }

    return resultat;
}

QVector<ESens> Ecoulement::ouvertures(const ETypePiece& typePiece, const ESens& sens) {
    switch(typePiece) {
    case tpReservoir:
        return QVector<ESens>(1, sens);
    case tpHorizontal:
        return (QVector<ESens>() << sDroite << sGauche);
    case tpVertical:
        return (QVector<ESens>() << sHaut << sBas);
    case tpCoudeHautGauche:
        return (QVector<ESens>() << sHaut << sGauche);
    case tpCoudeHautDroite:
        return (QVector<ESens>() << sHaut << sDroite);
    case tpCoudeBasGauche:
        return (QVector<ESens>() << sBas << sGauche);
    case tpCoudeBasDroite:
        return (QVector<ESens>() << sBas << sDroite);
    case tpCroix:
        return (QVector<ESens>() << sHaut << sDroite << sBas << sGauche);
    default:
        return QVector<ESens>();
    }
}
