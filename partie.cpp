#include <QtGlobal>
#include <QtMath>
#include "partie.h"

// Bareme : la penalite de remplacement est celle du jeu d'origine, les points
// par case s'y calent (un remplacement coute une case de progression).
#define POINTS_PAR_CASE         50
#define PENALITE_REMPLACEMENT   50
// Chaque case deja raccordee en aval du flux quand l'objectif tombe.
#define POINTS_BONUS_AVANCE     25
// Progression de la difficulte, niveau apres niveau. La longueur minimale mene
// la progression parce qu'elle est lisible pour le joueur ; la vitesse du flux
// ne suit que doucement, avec un plancher, car elle agit sur l'ecart entre le
// debit du flux et celui de la pose : quelques centiemes suffisent a rendre le
// jeu injouable.
//
// Valeurs calees sur du jeu reel. Le facteur cache est le taux de pieces
// inutilisables : pour une direction donnee, seuls 4 types sur 7 offrent
// l'ouverture voulue. Il faut donc tirer une trentaine de pieces pour en poser
// vingt d'utiles, d'ou un objectif de depart modeste et un delai genereux.
#define LONGUEUR_BASE           12
#define LONGUEUR_PAS            3
#define DELAI_BASE              22.0f
#define DELAI_MIN               10.0f
#define DUREE_BASE              0.30f
#define DUREE_FACTEUR           0.95f
#define DUREE_MIN               0.12f
// Cases infranchissables : levier de difficulte qui joue sur la reflexion et
// non sur le rythme, donc sans risque d'approcher le point de bascule ou le
// flux irait plus vite que la main du joueur.
#define BLOQUEES_PAS            2
#define BLOQUEES_MAX            24
// Temps d'affichage du resultat avant d'enchainer.
#define PAUSE_REUSSIE           2.5f
#define PAUSE_PERDUE            3.5f

Partie::Partie(int largeur, int hauteur) {
    plat = new Game(largeur, hauteur);
    ecoul = new Ecoulement(plat);
    fil = new PieceFile(FILE_SIZE);

    nouvellePartie();
}

Partie::~Partie() {
    delete ecoul;
    delete plat;
    delete fil;
}

void Partie::nouvellePartie() {
    niveauCourant = 1;
    pointsCourants = 0;
    nouvelleManche();
}

void Partie::nouvelleManche() {
    plat->reinitialiser(nbCasesBloquees());
    ecoul->reinitialiser();
    ecoul->setDureeRemplissage(dureeRemplissageNiveau());

    tempsAvantDepart = delaiDepartNiveau();
    bonusCourant = 0;
    etatCourant = epAttente;
}

// Le flux accelere doucement, mais jamais au-dela du plancher : passe ce point
// il irait plus vite que la main du joueur et toute manche serait perdue.
float Partie::dureeRemplissageNiveau() const {
    return qMax(DUREE_MIN, (float)(DUREE_BASE * qPow(DUREE_FACTEUR, niveauCourant - 1)));
}

// Aucune au premier niveau : on laisse le joueur decouvrir la grille libre.
int Partie::nbCasesBloquees() const {
    return qMin(BLOQUEES_MAX, (niveauCourant - 1) * BLOQUEES_PAS);
}

float Partie::delaiDepartNiveau() const {
    return qMax(DELAI_MIN, DELAI_BASE - (niveauCourant - 1));
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

    if(traversees >= longueurMinimale()) {
        // La manche s'arrete des l'objectif atteint : sans bonus, tout ce que le
        // joueur a construit au-dela ne rapporterait rien et le score serait le
        // meme a chaque reussite. On compte donc la tuyauterie deja raccordee en
        // aval du flux, qui mesure l'avance prise.
        bonusCourant = ecoul->casesEnAval() * POINTS_BONUS_AVANCE;
        pointsCourants += bonusCourant;
        etatCourant = epReussie;
        tempsAvantSuite = PAUSE_REUSSIE;
    } else {
        etatCourant = epPerdue;
        tempsAvantSuite = PAUSE_PERDUE;
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
        if(ecoul->nbCasesTraversees() >= longueurMinimale() || etatFlux != eEnCours) {
            terminerManche();
        }
        break;
    }

    case epReussie:
    case epPerdue:
        // On laisse le resultat affiche un instant, puis on enchaine : niveau
        // suivant si la manche est reussie, nouvelle partie sinon.
        tempsAvantSuite -= dt;

        if(tempsAvantSuite <= 0.0f) {
            if(etatCourant == epReussie) {
                niveauCourant++;
                nouvelleManche();
            } else {
                nouvellePartie();
            }
        }
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

    ETypePiece actuelle = plat->getTypePiece(col, row);

    // Case deja traversee par le fluide, reservoir, obstacle : intouchables.
    return !ecoul->estRempli(col, row)
           && actuelle != tpReservoir
           && actuelle != tpBloque;
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
    return LONGUEUR_BASE + LONGUEUR_PAS * (niveauCourant - 1);
}

float Partie::fractionAvantDepart() const {
    return tempsAvantDepart / delaiDepartNiveau();
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
