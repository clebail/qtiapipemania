#include <QtGlobal>
#include "partie.h"

// Bareme : la penalite de remplacement est celle du jeu d'origine, les points
// par case s'y calent (un remplacement coute une case de progression).
#define POINTS_PAR_CASE         50
#define PENALITE_REMPLACEMENT   50
// Longueur minimale du pipeline pour que la manche soit reussie.
#define LONGUEUR_MINIMALE       20
// Secondes laissees au joueur pour poser des tuyaux avant que le flux parte.
#define DELAI_DEPART            15.0f

Partie::Partie(int largeur, int hauteur) {
    plat = new Game(largeur, hauteur);
    ecoul = new Ecoulement(plat);
    fil = new PieceFile(FILE_SIZE);

    nouvelleManche();
}

Partie::~Partie() {
    delete ecoul;
    delete plat;
    delete fil;
}

void Partie::nouvelleManche() {
    // Le plateau n'est pas efface ici : c'est a l'appelant de le preparer avant
    // (generateur de test aujourd'hui, nouveau niveau plus tard).
    ecoul->reinitialiser();
    tempsAvantDepart = DELAI_DEPART;
    etatCourant = epAttente;
}

void Partie::lancerEcoulement() {
    tempsAvantDepart = 0.0f;
    ecoul->reinitialiser();
    ecoul->demarrer();
    etatCourant = epEcoulement;
}

void Partie::terminerManche() {
    int traversees = ecoul->nbCasesTraversees();

    pointsCourants += traversees * POINTS_PAR_CASE;
    etatCourant = (traversees >= LONGUEUR_MINIMALE) ? epReussie : epPerdue;
}

void Partie::avancer(float dt) {
    switch(etatCourant) {
    case epAttente:
        tempsAvantDepart -= dt;

        if(tempsAvantDepart <= 0.0f) {
            lancerEcoulement();
        }
        break;

    case epEcoulement:
        // Une fuite n'est pas une defaite en soi : elle arrete l'ecoulement, et
        // c'est la longueur atteinte qui decide de l'issue.
        if(ecoul->avancer(dt) != eEnCours) {
            terminerManche();
        }
        break;

    default:
        break;
    }
}

bool Partie::peutPoser(int col, int row) const {
    if(etatCourant != epAttente && etatCourant != epEcoulement) {
        return false;
    }

    if(col < 0 || col >= plat->getLargeur() || row < 0 || row >= plat->getHauteur()) {
        return false;
    }

    // Case deja traversee par le fluide, ou reservoir : intouchables.
    return !ecoul->estRempli(col, row) && plat->getTypePiece(col, row) != tpReservoir;
}

bool Partie::poserPiece(int col, int row) {
    if(!peutPoser(col, row)) {
        return false;
    }

    ETypePiece actuelle = plat->getTypePiece(col, row);

    if(actuelle != tpNone) {
        // Convention arcade : le compteur ne descend pas sous zero.
        pointsCourants = qMax(0, pointsCourants - PENALITE_REMPLACEMENT);
    }

    Piece piece = fil->depiler();
    plat->setTypePiece(col, row, piece.type);
    plat->setSens(col, row, piece.sens);

    return true;
}

EEtatPartie Partie::etat() const {
    return etatCourant;
}

int Partie::score() const {
    return pointsCourants;
}

int Partie::casesTraversees() const {
    return ecoul->nbCasesTraversees();
}

int Partie::longueurMinimale() const {
    return LONGUEUR_MINIMALE;
}

float Partie::fractionAvantDepart() const {
    return tempsAvantDepart / DELAI_DEPART;
}

Game* Partie::plateau() const {
    return plat;
}

Ecoulement* Partie::ecoulement() const {
    return ecoul;
}

PieceFile* Partie::file() const {
    return fil;
}
