#ifndef PARTIE_H
#define PARTIE_H

#include <QtGlobal>

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
    Partie(int largeur, int hauteur, quint32 seed);
    ~Partie();

    // Nouvelle partie sur une graine tiree au hasard.
    void nouvellePartie();
    // Nouvelle partie rejouable : a graine egale, tous les niveaux seront
    // identiques, plateau comme file.
    void nouvellePartie(quint32 seed);
    void nouvelleManche();
    // Niveau auquel une partie demarre. Vaut 1 en jeu normal ; la ligne de
    // commande le releve (--niveau) pour aller regarder directement une manche
    // difficile, sans avoir a jouer les huit precedentes. La partie perdue
    // repart au meme niveau, sinon l'outil ne servirait qu'une fois.
    void setNiveauDepart(int niveau);
    void avancer(float dt);

    // Lance le flux sans attendre la fin du delai, contre une prime. Le joueur
    // echange ce qui lui restait de temps de construction contre des points :
    // c'est un pari, pas un raccourci gratuit. Renvoie false hors de l'attente.
    bool lancerFluxAnticipe();

    // Ecourte la pause d'affichage du resultat et enchaine tout de suite :
    // niveau suivant si la manche est reussie, nouvelle partie sinon. Renvoie
    // false hors des deux etats de fin. Aucune prime et aucune penalite -- la
    // pause n'est qu'un temps de lecture, la sauter ne s'achete pas.
    bool passerLaSuite();

    // Le coup est-il permis ? (case deja traversee par le fluide, reservoir,
    // hors grille, manche finie : autant de refus)
    bool peutPoser(int col, int row) const;
    // Pose la piece du haut de la file. Renvoie false si le coup est refuse,
    // auquel cas la file n'est pas depilee.
    bool poserPiece(int col, int row);

    EEtatPartie etat() const;
    int score() const;
    int niveau() const;
    // Vies restantes, celle qu'on est en train de jouer comprise. Zero ne se
    // voit qu'a l'etat epGameOver : tant qu'il en reste une, la manche perdue
    // se rejoue.
    int vies() const;
    int casesTraversees() const;

    int longueurMinimale() const;
    // Longueur du tuyau deja raccorde au reservoir, objectif compris ou non :
    // ce que le flux parcourra s'il partait maintenant. Se lit avant le depart,
    // la ou casesTraversees() vaut encore zero.
    int longueurTracee() const;
    // Graine de la partie en cours : suffit a la rejouer entierement.
    quint32 getGraine() const;
    // Numero de la manche depuis la construction, croissant sans jamais
    // revenir en arriere. C'est le seul signal fiable d'un changement de manche
    // depuis que la defaite REJOUE le meme niveau : plateau, file et graine
    // derivee sont alors identiques a ceux de la manche precedente, et les
    // comparer ne dit plus rien.
    int numeroManche() const;
    // Remplacements payes depuis le debut de la partie. Le score seul ne se
    // decompose pas -- il melange traversees, primes et penalites -- et c'est ce
    // qui a laisse inexpliquees plusieurs mesures (voir BOT.md).
    int nbRemplacements() const;
    // 1 = delai entier restant avant le depart du flux, 0 = il est parti.
    float fractionAvantDepart() const;
    // Le meme, en secondes : ce qu'il reste de temps de construction. Zero des
    // que le flux est parti.
    float secondesAvantDepart() const;

    Game* plateau() const;
    Ecoulement* ecoulement() const;
    PieceFile* file() const;

    int getXDepart() const;
    int getYDepart() const;
    int getLargeur() const;
    int getHauteur() const;
private:
    // Non copiable : proprietaire de son plateau, de son ecoulement et de sa
    // file par pointeur nu.
    Q_DISABLE_COPY(Partie)

    Game *plat = nullptr;
    Ecoulement *ecoul = nullptr;
    PieceFile *fil = nullptr;
    EEtatPartie etatCourant = epAttente;
    quint32 grainePartie = 0;
    int niveauCourant = 1;
    int niveauDepart = 1;
    int mancheCourante = 0;
    int pointsCourants = 0;
    int viesRestantes = 0;
    // Points a atteindre pour la prochaine vie de rythme. Il MONTE et ne
    // redescend jamais : le score, lui, descend de 25 a chaque ecrasement, donc
    // sans ce cliquet la meme barre se paierait plusieurs fois par partie
    // (mesure : +21 % de vies, voir VIES.md).
    int prochainPalier = 0;
    int remplacements = 0;
    float tempsAvantDepart = 0.0f;
    float tempsAvantSuite = 0.0f;

    int nbCasesBloquees() const;
    float dureeRemplissageNiveau() const;
    float delaiDepartNiveau() const;
    void lancerEcoulement();
    void terminerManche();
    // Vies gagnees en fin de manche : les paliers de points, et la belle
    // manche. Appelee avant de decompter la vie perdue, pour qu'une manche qui
    // finit bien puisse payer la defaite qu'elle vient de subir.
    void crediterVies(int traversees, bool reussie);
    void gagnerVie();
};

#endif // PARTIE_H
