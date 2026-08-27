#include <QRandomGenerator>
#include <string.h>
#include "ecoulement.h"

static ESens sensReciproques[] = {sBas, sHaut, sDroite, sGauche};
static SDelta deltas[] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};

// Indexee par ESens (sHaut, sBas, sGauche, sDroite). Chaque ligne : le tuyau
// droit qui suit cet axe, les deux coudes ouverts sur ce cote, la croix.
static const ETypePiece piecesParEntree[4][4] = {
    /* sHaut   */ {tpVertical,   tpCoudeHautGauche, tpCoudeHautDroite, tpCroix},
    /* sBas    */ {tpVertical,   tpCoudeBasGauche,  tpCoudeBasDroite,  tpCroix},
    /* sGauche */ {tpHorizontal, tpCoudeHautGauche, tpCoudeBasGauche,  tpCroix},
    /* sDroite */ {tpHorizontal, tpCoudeHautDroite, tpCoudeBasDroite,  tpCroix},
    };

Ecoulement::Ecoulement(const Game *plateau) {
    int size = plateau->getSize() * NB_AXES;

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
    int size = plateau->getSize() * NB_AXES;

    memset(remplis, 0, size*sizeof(*remplis));
    memset(progressions, 0, size*sizeof(*progressions));
    memset(entrees, 0, size*sizeof(*entrees));

    front.clear();
    casesTraversees = 0;
}

void Ecoulement::demarrer() {
    // Le reservoir n'est pas une croix : il tient sur l'axe 0 comme le reste.
    int conduite = plateau->getIdxDepart() * NB_AXES + AXE_HORIZONTAL;

    remplis[conduite] = true;
    entrees[conduite] = (unsigned char)plateau->getSens(plateau->getIdxDepart());

    front << conduite;
    // Le reservoir est la source, pas une case parcourue : il ne compte pas.
}

int Ecoulement::nbCasesTraversees() const {
    return casesTraversees;
}

void Ecoulement::setDureeRemplissage(float secondes) {
    dureeRemplissage = secondes;
}

float Ecoulement::getDureeRemplissage() const {
    return dureeRemplissage;
}

EEtat Ecoulement::avancer(float dt) {
    if(front.isEmpty()) {
        return eTermine;
    }

    int largeur = plateau->getLargeur();
    int hauteur = plateau->getHauteur();
    QVector<int> frontSuivant;
    bool fuite = false;

    foreach(int conduite, front) {
        progressions[conduite] += dt / dureeRemplissage;

        if(progressions[conduite] < 1.0f) {
            // Conduite encore en cours de remplissage : elle reste dans le front.
            frontSuivant << conduite;
            continue;
        }

        progressions[conduite] = 1.0f;

        int idx = conduite / NB_AXES;
        int x = idx % largeur;
        int y = idx / largeur;

        foreach(ESens ouverture, sorties(plateau->getTypePiece(x, y),
                                         plateau->getSens(x, y),
                                         (ESens)entrees[conduite])) {
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

            // C'est la conduite qui est revendiquee, pas la case : le second
            // passage dans une croix emprunte l'autre axe et trouve la place
            // libre, alors que toute autre piece n'en a qu'une.
            int nextConduite = nextIdx * NB_AXES + axe(nextTypePiece, sensEntre);

            if(remplis[nextConduite]) {
                // Deja revendiquee : conduite d'ou l'on vient, ou branches qui
                // se rejoignent apres une boucle.
                continue;
            }

            remplis[nextConduite] = true;
            entrees[nextConduite] = (unsigned char)sensEntre;
            progressions[nextConduite] = 0.0f;
            // Les deux conduites d'une croix comptent chacune pour une case :
            // la croiser deux fois rapporte donc deux fois les points.
            casesTraversees++;

            frontSuivant << nextConduite;
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
    int size = plateau->getSize() * NB_AXES;

    // Meme parcours que avancer(), mais a blanc : on part du front courant avec
    // une copie du visite, donc l'etat de l'ecoulement n'est pas touche.
    QVector<unsigned char> vus(size);
    memcpy(vus.data(), remplis, size*sizeof(*remplis));

    // Les cases en aval ne sont pas encore atteintes : leur cote d'entree n'est
    // connu qu'au moment ou le parcours y arrive, et il faut le retenir puisque
    // les sorties d'un tuyau en dependent.
    QVector<unsigned char> entreesVues(size);
    memcpy(entreesVues.data(), entrees, size*sizeof(*entrees));

    QVector<int> aVoir = front;
    int compte = 0;

    while(!aVoir.isEmpty()) {
        int conduite = aVoir.takeLast();
        int idx = conduite / NB_AXES;
        int suivX, suivY;
        ESens suivEntree;

        if(!suivante(idx % largeur, idx / largeur, (ESens)entreesVues[conduite],
                     suivX, suivY, suivEntree)) {
            continue;
        }

        ETypePiece suivType = plateau->getTypePiece(suivX, suivY);

        if(!ouvertures(suivType, plateau->getSens(suivX, suivY)).contains(suivEntree)) {
            continue;
        }

        int suivConduite = (suivY * largeur + suivX) * NB_AXES + axe(suivType, suivEntree);

        if(vus[suivConduite]) {
            continue;
        }

        // A blanc, l'entree de la conduite visee n'est pas encore posee : c'est
        // celle par laquelle on y arrive.
        entreesVues[suivConduite] = (unsigned char)suivEntree;
        vus[suivConduite] = true;
        compte++;
        aVoir << suivConduite;
    }

    return compte;
}

void Ecoulement::voisine(int col, int row, ESens sortie, int& vCol, int& vRow, ESens& vEntree) {
    vCol = col + deltas[(unsigned char)sortie].dx;
    vRow = row + deltas[(unsigned char)sortie].dy;
    vEntree = sensReciproques[(unsigned char)sortie];
}

bool Ecoulement::suivante(int col, int row, ESens entree,
                          int& suivCol, int& suivRow, ESens& suivEntree) const {
    QVector<ESens> sort = sorties(plateau->getTypePiece(col, row),
                                  plateau->getSens(col, row), entree);

    // Une entree, une sortie : deux ouvertures dont l'une sert d'entree, et la
    // croix va tout droit. Le tuyau est une ligne, jamais un arbre.
    if(sort.isEmpty()) {
        return false;
    }

    ESens sortie = sort.first();

    suivCol = col + deltas[(unsigned char)sortie].dx;
    suivRow = row + deltas[(unsigned char)sortie].dy;
    suivEntree = sensReciproques[(unsigned char)sortie];

    return suivCol >= 0 && suivCol < plateau->getLargeur()
           && suivRow >= 0 && suivRow < plateau->getHauteur();
}

bool Ecoulement::tete(int& col, int& row, ESens& entree) const {
    int idx = plateau->getIdxDepart();
    int x = idx % plateau->getLargeur();
    int y = idx / plateau->getLargeur();
    // Le reservoir n'a pas d'entree : ce qui y est enregistre est sa sortie.
    ESens e = plateau->getSens(idx);

    // Borne de securite plutot qu'un tableau de visitees : une croix permet de
    // refermer un circuit sur lui-meme, et la marche tournerait alors sans fin.
    for(int pas=0; pas<plateau->getSize(); pas++) {
        int suivX, suivY;
        ESens suivEntree;

        if(!suivante(x, y, e, suivX, suivY, suivEntree)) {
            // Le tuyau donne hors grille : fuite programmee, pas de tete.
            return false;
        }

        ETypePiece suivType = plateau->getTypePiece(suivX, suivY);

        if(suivType == tpNone) {
            col = suivX;
            row = suivY;
            entree = suivEntree;

            return true;
        }

        if(!ouvertures(suivType, plateau->getSens(suivX, suivY)).contains(suivEntree)) {
            // Case bloquee, ou piece posee qui ne presente rien en face.
            return false;
        }

        x = suivX;
        y = suivY;
        e = suivEntree;
    }

    return false;
}

// Seule la croix distingue ses deux axes ; ailleurs le flux ne passe qu'une
// fois, et tout tient donc sur l'axe 0.
int Ecoulement::axe(const ETypePiece& typePiece, const ESens& entree) {
    if(typePiece == tpCroix && (entree == sHaut || entree == sBas)) {
        return AXE_VERTICAL;
    }

    return AXE_HORIZONTAL;
}

bool Ecoulement::estRempli(int col, int row) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()) {
        int idx = row * plateau->getLargeur() + col;

        return remplis[idx * NB_AXES + AXE_HORIZONTAL] || remplis[idx * NB_AXES + AXE_VERTICAL];
    }

    return false;
}

float Ecoulement::progression(int col, int row, int axe) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()
       && axe >= 0 && axe < NB_AXES) {
        int idx = row * plateau->getLargeur() + col;

        return progressions[idx * NB_AXES + axe];
    }

    return 0.0;
}

ESens Ecoulement::entree(int col, int row, int axe) const {
    if(col >= 0 && col < plateau->getLargeur() && row >= 0 && row < plateau->getHauteur()
       && axe >= 0 && axe < NB_AXES) {
        int idx = row * plateau->getLargeur() + col;

        return (ESens)entrees[idx * NB_AXES + axe];
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

QVector<ETypePiece> Ecoulement::piecesCompatibles(const ESens& entree) {
    QVector<ETypePiece> resultat;

    for(int i = 0; i < 4; i++) {
        resultat << piecesParEntree[(unsigned char)entree][i];
    }

    return resultat;
}

int Ecoulement::longueurTracee(const Game *plateau) {
    int longueur = 0;
    Ecoulement ecoulement(plateau);

    ecoulement.setDureeRemplissage(1);
    ecoulement.demarrer();
    while(ecoulement.avancer(1.0f) == eEnCours) {
        longueur++;
    }

    return longueur;
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
