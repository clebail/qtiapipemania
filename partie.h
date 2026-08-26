#ifndef PARTIE_H
#define PARTIE_H

#include "common.h"
#include "game.h"
#include "ecoulement.h"
#include "piecefile.h"

// Cycle de jeu et regles : deroulement d'une manche, score, conditions de
// reussite. Classe simple comme Game et Ecoulement : pas de timer interne, la
// fenetre appelle avancer() depuis sa propre cadence.
class Partie
{
public:
    Partie(int largeur, int hauteur);
    ~Partie();

    void nouvelleManche();
    void avancer(float dt);

    // Le coup est-il permis ? (case deja traversee par le fluide, reservoir,
    // hors grille, manche finie : autant de refus)
    bool peutPoser(int col, int row) const;
    // Pose la piece du haut de la file. Renvoie false si le coup est refuse,
    // auquel cas la file n'est pas depilee.
    bool poserPiece(int col, int row);

    EEtatPartie etat() const;
    int score() const;
    int niveau() const;
    int casesTraversees() const;
    // Points de bonus accordes a la derniere manche reussie.
    int bonusManche() const;
    int longueurMinimale() const;
    // 1 = delai entier restant avant le depart du flux, 0 = il est parti.
    float fractionAvantDepart() const;

    Game* plateau() const;
    Ecoulement* ecoulement() const;
    PieceFile* file() const;

private:
    Game *plat = nullptr;
    Ecoulement *ecoul = nullptr;
    PieceFile *fil = nullptr;
    EEtatPartie etatCourant = epAttente;
    int niveauCourant = 1;
    int pointsCourants = 0;
    int bonusCourant = 0;
    float tempsAvantDepart = 0.0f;

    void lancerEcoulement();
    void terminerManche();
};

#endif // PARTIE_H
