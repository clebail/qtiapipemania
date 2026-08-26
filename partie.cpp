#include <QtGlobal>
#include "partie.h"

// Bareme : la penalite de remplacement est celle du jeu d'origine, les points
// par case s'y calent (un remplacement coute une case de progression).
#define POINTS_PAR_CASE         50
#define PENALITE_REMPLACEMENT   50
// Chaque case deja raccordee en aval du flux quand l'objectif tombe.
#define POINTS_BONUS_AVANCE     25
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
    bonusCourant = 0;
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
    bonusCourant = 0;

    if(traversees >= LONGUEUR_MINIMALE) {
        // La manche s'arrete des l'objectif atteint : sans bonus, tout ce que le
        // joueur a construit au-dela ne rapporterait rien et le score serait le
        // meme a chaque reussite. On compte donc la tuyauterie deja raccordee en
        // aval du flux, qui mesure l'avance prise.
        bonusCourant = ecoul->casesEnAval() * POINTS_BONUS_AVANCE;
        pointsCourants += bonusCourant;
        etatCourant = epReussie;
    } else {
        etatCourant = epPerdue;
    }
}

void Partie::avancer(float dt) {
    switch(etatCourant) {
    case epAttente:
        tempsAvantDepart -= dt;

        if(tempsAvantDepart <= 0.0f) {
            lancerEcoulement();
        }
        break;

    case epEcoulement: {
        EEtat etatFlux = ecoul->avancer(dt);

        // Deux facons de finir. Soit l'objectif est atteint, et la manche
        // s'arrete aussitot : sans cela, un joueur qui poserait plus vite que
        // le flux ne consomme ne perdrait jamais et la manche durerait
        // indefiniment. Soit le flux s'arrete de lui-meme, et c'est alors la
        // longueur atteinte qui decide de l'issue -- une fuite n'est pas une
        // defaite en soi.
        if(ecoul->nbCasesTraversees() >= LONGUEUR_MINIMALE || etatFlux != eEnCours) {
            terminerManche();
        }
        break;
    }

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

// Reste a 1 tant que la progression par niveaux n'est pas branchee : c'est
// nouvelleManche() qui l'incrementera, une fois la manche precedente reussie.
int Partie::niveau() const {
    return niveauCourant;
}

int Partie::bonusManche() const {
    return bonusCourant;
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
