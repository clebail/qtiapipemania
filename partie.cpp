#include <QtGlobal>
#include <QtMath>
#include <QRandomGenerator>
#include "partie.h"

// Bareme : les points par case sont ceux du jeu d'origine. La penalite de
// remplacement y vaut la moitie d'une case, et c'est ce rapport qui compte --
// il fixe a partir de quand reecrire une piece deja posee est rentable.
//
// A 50, soit une case pleine, il fallait que le remplacement rallonge le trace
// de DEUX cases pour valoir le coup. A 25, une seule suffit. C'est le geste qui
// transforme une impasse en route, et celui que les bons joueurs emploient sans
// hesiter (voir BOT.md) : le tarifer au prix fort revenait a decourager la seule
// facon de rattraper un mauvais trace.
#define POINTS_PAR_CASE         50
#define PENALITE_REMPLACEMENT   25
// Prime de depart anticipe : ce que vaut le temps de construction qu'on
// abandonne. Assez pour valoir le coup une fois l'objectif securise, pas assez
// pour qu'on la prenne systematiquement des le debut d'une manche.
#define POINTS_DEPART_ANTICIPE  200
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
//
// L'objectif ne plafonne plus a portee de jeu : il monte de quatre cases par
// niveau jusqu'a un plafond qui n'est la que pour borner l'impossible. Le
// plateau tient 225 cases, moins les 24 bloquees au maximum, soit 201 libres --
// et une croix se traversant deux fois, un trace peut depasser ce compte. Le
// plafond est donc physique, pas un reglage de difficulte : passe le niveau 53
// la manche n'est plus gagnable, et c'est la vraie fin du jeu.
//
// Le plafond valait vingt cases avant le reequilibrage du 28/08/2026, puis
// soixante, atteintes au niveau 14. Le probleme de soixante : passe ce niveau
// plus RIEN ne montait -- delai, blocs, vitesse et objectif plafonnaient tous
// entre les niveaux 9 et 14 --, et la partie devenait stationnaire. Un joueur
// qui franchissait le 14 ne pouvait plus perdre que par malchance de tirage,
// jamais par difficulte croissante. C'est desormais la longueur, seule, qui
// porte la difficulte au-dela du niveau 13.
//
// Le delai de depart, lui, ne bouge plus du tout : DELAI_MIN vaut DELAI_BASE,
// donc vingt-deux secondes a tous les niveaux. Il descendait a dix, et ne
// laissait alors plus le temps de construire un objectif long : on perdait sur
// le chronometre avant d'avoir pu montrer quoi que ce soit du trace. C'est la
// longueur qui doit tuer, pas la montre -- et elle, elle monte sans fin.
#define LONGUEUR_BASE           10
#define LONGUEUR_PAS            4
#define LONGUEUR_MAX            220
#define DELAI_BASE              22.0f
#define DELAI_MIN               22.0f
#define DUREE_BASE              1.50f
#define DUREE_FACTEUR           0.95f
#define DUREE_MIN               1.00f
// Cases infranchissables : levier de difficulte qui joue sur la reflexion et
// non sur le rythme, donc sans risque d'approcher le point de bascule ou le
// flux irait plus vite que la main du joueur.
#define BLOQUEES_PAS            2
#define BLOQUEES_MAX            24
// Plateau et file tirent chacun leur graine de manche : sans ce decoupage, les
// deux generateurs partiraient du meme etat et le placement du reservoir serait
// correle a la premiere piece.
#define CANAL_PLATEAU           0
#define CANAL_FILE              1
#define NB_CANAUX               2
// Temps d'affichage du resultat avant d'enchainer.
#define PAUSE_REUSSIE           2.5f
#define PAUSE_PERDUE            3.5f
// Vies. Trois au depart, plafond neuf. Perdre une manche coute une vie et
// rejoue le MEME niveau -- meme plateau, meme file, puisque tout derive de
// (graine de partie, niveau). Les points sont conserves ; seul le game over
// remet le compteur a zero.
#define VIES_DEPART             3
#define VIES_MAX                9
// Une vie tous les 20 000 points : la seule regle qui ne s'eteigne jamais,
// puisqu'elle paie proportionnellement au chemin parcouru. Le palier atteint se
// memorise (voir prochainPalier).
#define PALIER_VIE              20000
// Une vie par belle manche. Seuil ABSOLU de traversees, pas un multiple de
// l'objectif : recompenser objectif x 2 revenait a recompenser la petitesse de
// l'objectif, pas la performance.
//
// Reserve aux manches REUSSIES, et ce n'est pas un scrupule de bareme : passe
// le niveau 26 l'objectif vaut 110, donc une manche perdue pourrait franchir le
// seuil et rendre la vie qu'elle vient de couter, a chaque tentative. La partie
// ne se terminerait plus.
#define BELLE_MANCHE            110

// Chaque manche est tiree a partir de (graine de partie, niveau) : le niveau 13
// est donc le meme dans deux parties de meme graine, quel que soit le chemin
// suivi pour y arriver. C'est ce qui permet de comparer deux facons de jouer sur
// exactement les memes niveaux.
//
// Le melange est un vrai brassage de bits et non une simple addition : deux
// graines voisines doivent donner des suites sans rapport, ce qu'un generateur
// seme par un seul entier ne garantit pas de lui-meme.
static quint32 grainePour(quint32 graine, int niveau, quint32 canal) {
    quint32 x = graine + (quint32)(niveau * NB_CANAUX + canal) * 0x9E3779B9u;

    x ^= x >> 16;
    x *= 0x7FEB352Du;
    x ^= x >> 15;
    x *= 0x846CA68Bu;
    x ^= x >> 16;

    return x;
}

Partie::Partie(int largeur, int hauteur)
    : Partie(largeur, hauteur, QRandomGenerator::securelySeeded().generate()) {
}

Partie::Partie(int largeur, int hauteur, quint32 seed) {
    plat = new Game(largeur, hauteur, seed);
    ecoul = new Ecoulement(plat);
    fil = new PieceFile(FILE_SIZE, seed);

    nouvellePartie(seed);
}

Partie::~Partie() {
    delete ecoul;
    delete plat;
    delete fil;
}

// Une defaite relance une partie neuve, donc une nouvelle graine : on ne veut
// pas rejouer indefiniment le meme tirage apres chaque echec.
void Partie::nouvellePartie() {
    nouvellePartie(QRandomGenerator::securelySeeded().generate());
}

void Partie::setNiveauDepart(int niveau) {
    niveauDepart = qMax(1, niveau);
    niveauCourant = niveauDepart;
    nouvelleManche();
}

void Partie::nouvellePartie(quint32 seed) {
    grainePartie = seed;
    niveauCourant = niveauDepart;
    pointsCourants = 0;
    viesRestantes = VIES_DEPART;
    prochainPalier = PALIER_VIE;
    remplacements = 0;
    nouvelleManche();
}

void Partie::nouvelleManche() {
    mancheCourante++;
    plat->reinitialiser(nbCasesBloquees(), grainePour(grainePartie, niveauCourant, CANAL_PLATEAU));
    // La file repart neuve a chaque manche : c'est la condition pour qu'un
    // niveau donne soit identique d'une partie a l'autre.
    fil->reinitialiser(grainePour(grainePartie, niveauCourant, CANAL_FILE));
    ecoul->reinitialiser();
    ecoul->setDureeRemplissage(dureeRemplissageNiveau());

    tempsAvantDepart = delaiDepartNiveau();
    etatCourant = epAttente;
}

// Le flux accelere doucement, mais jamais au-dela du plancher. Ce plancher tient
// une promesse : a la cadence humaine de reference (2 poses/s), le joueur a le
// temps de defausser DEUX pieces pendant que le flux en traverse une. C'est ce
// qui fait qu'une mauvaise serie de tirages coute du terrain sans coûter la
// manche. Le jeu commence plus genereux encore (trois gestes par case) et se
// resserre jusqu'a cette garantie.
//
// Le temps ne pouvant plus tuer seul, c'est la longueur exigee qui porte la
// difficulte : son pas et son plafond ont ete releves d'autant.
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

bool Partie::lancerFluxAnticipe() {
    if(etatCourant != epAttente) {
        return false;
    }

    pointsCourants += POINTS_DEPART_ANTICIPE;
    lancerEcoulement();

    return true;
}

bool Partie::passerLaSuite() {
    if(etatCourant != epReussie && etatCourant != epPerdue && etatCourant != epGameOver) {
        return false;
    }

    // On ne duplique pas l'enchainement : avancer() le fait deja, et le
    // reecrire ici serait deux endroits a tenir d'accord sur ce que veut dire
    // "manche suivante". Il suffit que la pause soit ecoulee.
    tempsAvantSuite = 0.0f;

    return true;
}

void Partie::terminerManche() {
    int traversees = ecoul->nbCasesTraversees();

    // Tout ce que le flux a parcouru compte, et rien d'autre. La manche allant
    // jusqu'au bout du tuyau, construire au-dela de l'objectif se paie
    // directement au tarif de la case traversee : c'est ce qui a permis de
    // supprimer l'ancien bonus d'avance, qui n'etait qu'un pis-aller pour
    // compenser une manche coupee avant que le tuyau construit soit parcouru.
    pointsCourants += traversees * POINTS_PAR_CASE;

    bool reussie = traversees >= longueurMinimale();

    // Crediter AVANT de decompter : la manche qui franchit un palier en mourant
    // paie la vie qu'elle est en train de perdre. L'ordre inverse condamnerait
    // sur un game over des points deja gagnes.
    crediterVies(traversees, reussie);

    if(reussie) {
        etatCourant = epReussie;
        tempsAvantSuite = PAUSE_REUSSIE;
        return;
    }

    viesRestantes--;
    etatCourant = viesRestantes > 0 ? epPerdue : epGameOver;
    tempsAvantSuite = PAUSE_PERDUE;
}

void Partie::gagnerVie() {
    viesRestantes = qMin(VIES_MAX, viesRestantes + 1);
}

void Partie::crediterVies(int traversees, bool reussie) {
    // Le palier franchi se memorise, meme quand la vie qu'il donne est perdue
    // sur le plafond : sinon la barre suivante serait celle qu'on vient de
    // passer, et le premier ecrasement la rendrait a nouveau payante.
    while(pointsCourants >= prochainPalier) {
        gagnerVie();
        prochainPalier += PALIER_VIE;
    }

    if(reussie && traversees >= BELLE_MANCHE) {
        gagnerVie();
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

        // Une seule facon de finir : le flux s'arrete de lui-meme, faute de
        // tuyau devant lui. C'est alors la longueur parcourue qui decide -- une
        // fuite n'est pas une defaite en soi. On pose jusqu'au bout, et la
        // manche reste bornee sans qu'il faille l'interdire : on ne pose que sur
        // des cases libres, et il y en a un nombre fini.
        if(etatFlux != eEnCours) {
            terminerManche();
        }
        break;
    }

    case epReussie:
    case epPerdue:
    case epGameOver:
        // On laisse le resultat affiche un instant, puis on enchaine. Trois
        // suites et non deux : niveau suivant si la manche est reussie, MEME
        // niveau si elle est perdue -- c'est la vie qu'on vient de payer --,
        // partie neuve au game over seulement.
        tempsAvantSuite -= dt;

        if(tempsAvantSuite <= 0.0f) {
            if(etatCourant == epReussie) {
                niveauCourant++;
                nouvelleManche();
            } else if(etatCourant == epPerdue) {
                // Points conserves, niveau inchange : le rejeu redonne le meme
                // plateau et la meme file, tout l'interet de la vie depensee.
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
        // Convention arcade : le compteur ne descend pas sous zero. Le compteur
        // de remplacements, lui, compte le geste et non ce qu'il a coute : c'est
        // le geste qu'on veut suivre, et le plancher rognerait la mesure.
        pointsCourants = qMax(0, pointsCourants - PENALITE_REMPLACEMENT);
        remplacements++;
    }

    Piece piece = fil->depiler();
    plat->setTypePiece(col, row, piece.type);
    plat->setSens(col, row, piece.sens);

    return true;
}

EEtatPartie Partie::etat() const {
    return etatCourant;
}

int Partie::vies() const {
    return viesRestantes;
}

int Partie::numeroManche() const {
    return mancheCourante;
}

int Partie::score() const {
    return pointsCourants;
}

int Partie::nbRemplacements() const {
    return remplacements;
}

quint32 Partie::getGraine() const {
    return grainePartie;
}

int Partie::niveau() const {
    return niveauCourant;
}

int Partie::casesTraversees() const {
    return ecoul->nbCasesTraversees();
}

int Partie::longueurTracee() const {
    return Ecoulement::longueurTracee(plat);
}

int Partie::longueurMinimale() const {
    return qMin(LONGUEUR_MAX, LONGUEUR_BASE + LONGUEUR_PAS * (niveauCourant - 1));
}

float Partie::secondesAvantDepart() const {
    return etatCourant == epAttente ? qMax(0.0f, tempsAvantDepart) : 0.0f;
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

int Partie::getXDepart() const {
    return plat->getIdxDepart() % plat->getLargeur();
}

int Partie::getYDepart() const {
    return plat->getIdxDepart() / plat->getLargeur();
}

int Partie::getLargeur() const {
    return plat->getLargeur();
}

int Partie::getHauteur() const {
    return plat->getHauteur();
}
