#ifndef PARTIE_H
#define PARTIE_H

#include <QtGlobal>

#include "common.h"
#include "game.h"
#include "ecoulement.h"
#include "piecefile.h"
#include "minage.h"

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
    // Vies et bombes du depart de partie (--vies, --bombes). Meme role que
    // setNiveauDepart : regler la fin de vie du bot demande de la voir se
    // produire, et l'attendre depuis trois vies et zero bombe coute une partie
    // entiere par essai. La valeur tient pour la partie en cours ET pour celles
    // qui la suivent, sans quoi l'outil ne servirait qu'une fois.
    void setViesDepart(int vies);
    void setBombesDepart(int bombes);
    void avancer(float dt);

    // Lance le flux sans attendre la fin du delai, contre une prime. Le joueur
    // echange ce qui lui restait de temps de construction contre des points :
    // c'est un pari, pas un raccourci gratuit. Renvoie false hors de l'attente.
    bool lancerFluxAnticipe();

    // Ecourte la pause d'affichage du resultat et enchaine tout de suite :
    // niveau suivant si la manche est reussie, MEME niveau si elle est perdue.
    // Renvoie false hors de ces deux etats -- une partie finie ou abandonnee
    // ne s'enchaine plus, elle reste ou elle est. Aucune prime et aucune
    // penalite : la pause n'est qu'un temps de lecture, la sauter ne s'achete
    // pas.
    bool passerLaSuite();

    // JETER L'EPONGE. La partie s'arrete la, plateau et compteurs intacts, et
    // rien ne repart -- exactement comme au game over, dont elle ne se
    // distingue que par le mot que la grille affiche en grand.
    //
    // Elle existe pour le bot : en rejeu, une manche perdue d'avance se rejoue
    // a l'identique tant qu'il reste des vies, et regarder cinq fois la meme
    // mort n'apprend rien a personne. Rien n'empeche le joueur d'y avoir droit
    // aussi, le jour ou un bouton la lui offrira.
    //
    // Ce que le flux a deja traverse est encaisse au passage : c'est acquis, la
    // manche ne s'arreterait pas autrement. Les vies, elles, ne bougent pas --
    // on n'en perd pas une en refusant de jouer, on renonce a toutes.
    void abandonner();

    // Le coup est-il permis ? (case deja traversee par le fluide, reservoir,
    // hors grille, manche finie : autant de refus)
    bool peutPoser(int col, int row) const;
    // Pose la piece du haut de la file. Renvoie false si le coup est refuse,
    // auquel cas la file n'est pas depilee.
    bool poserPiece(int col, int row);
    // Le minage est-il permis ici ? Stock non vide, manche en cours, et case
    // TOTALEMENT vide. Les trois refus sont reunis ici plutot que dans la
    // grille : celle-ci ne fait que traduire un clic, et l'affichage du souffle
    // au survol doit poser exactement la meme question que le clic.
    bool peutMiner(int col, int row) const;
    // Pose une bombe sur une case totalement vide et la retire du stock. Elle
    // est perdue pour de bon : elle ne revient ni a l'explosion, ni au rejeu,
    // ni si la manche se finit avant qu'elle ait saute. False si le stock est
    // vide, si la manche est finie, ou si la case n'est pas libre.
    bool poserBombe(int col, int row);

    EEtatPartie etat() const;
    int score() const;
    int niveau() const;
    // Vies restantes, celle qu'on est en train de jouer comprise. Zero ne se
    // voit qu'a l'etat epGameOver : tant qu'il en reste une, la manche perdue
    // se rejoue.
    int vies() const;
    // Bombes en stock. Une par niveau reussi comportant des blocs fixes,
    // plafond BOMBES_MAX comme les vies, et jamais rendues une fois posees.
    // Voir BOMBES.md.
    int bombes() const;
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
    // Les bombes posees et les blocs qu'elles ont ouverts : la grille y lit le
    // compte a rebours a dessiner.
    Minage* minage() const;
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
    Minage *mines = nullptr;
    EEtatPartie etatCourant = epAttente;
    quint32 grainePartie = 0;
    int niveauCourant = 1;
    int niveauDepart = 1;
    int mancheCourante = 0;
    int pointsCourants = 0;
    int viesRestantes = 0;
    int bombesRestantes = 0;
    // Ce dont une partie neuve part. Regles par la ligne de commande, sinon
    // VIES_DEPART et BOMBES_DEPART (partie.cpp).
    int viesDepart = 0;
    int bombesDepart = 0;
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
    // `mortSubite` : la manche n'a pas fini d'elle-meme, une explosion a
    // emporte un tuyau plein. C'est une defaite franche, meme si le flux avait
    // deja depasse l'objectif -- on s'est tue soi-meme.
    void terminerManche(bool mortSubite = false);
    // Vies gagnees en fin de manche : les paliers de points, et la belle
    // manche. Appelee avant de decompter la vie perdue, pour qu'une manche qui
    // finit bien puisse payer la defaite qu'elle vient de subir.
    void crediterVies(int traversees, bool reussie);
    void gagnerVie();
    // Bombe gagnee en fin de manche : une par niveau reussi qui comportait des
    // blocs fixes. Appelee au meme endroit que crediterVies, avant le decompte
    // de la vie perdue.
    void crediterBombes(bool reussie);
    void gagnerBombe();
};

#endif // PARTIE_H
