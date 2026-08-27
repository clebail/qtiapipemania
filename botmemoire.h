#ifndef BOTMEMOIRE_H
#define BOTMEMOIRE_H

#include <QHash>

#include "botspaceanticp.h"

// v4. Le v3, plus la memoire des coups fatals -- le §B de VIES.md.
//
// Depuis les vies, perdre un niveau le REJOUE : meme plateau, meme file, piece
// pour piece (Partie::nouvelleManche, graines derivees de (graine, niveau)).
// Le bot ne joue donc plus un coup unique contre le hasard mais un jeu repete
// avec information reportee. Entre deux tentatives il autopsie sa manche et
// s'interdit le coup qui l'a tue.
//
// Le rejeu etant parfait, l'interdit est chirurgical : tout ce qui precede le
// carrefour est identique, le bot y arrive dans exactement le meme etat. Il n'a
// rien a generaliser -- il rejoue, il bifurque a un endroit precis, et tout
// l'amont est du terrain deja valide. C'est une recherche arborescente a N
// echantillons, pas de l'apprentissage.
//
// Quatre pieces :
//
//   - le JOURNAL : a chaque nouvelle tete, la capacite atteignable de chaque
//     direction vivante et ce qu'il reste a parcourir ;
//   - le BLAME : a la premiere mort du niveau, le journal est classe par
//     gravite et fige. Les tentatives suivantes le descendent dans l'ordre,
//     une entree par mort, sans jamais le recalculer ;
//   - les INTERDITS : (case, entree) -> direction interdite, cumules d'un essai
//     a l'autre. Veto dur tant qu'il reste une vie a depenser, simple
//     preference sur la derniere ;
//   - le DESAMORCAGE : un veto qui condamne la tete sort de l'ensemble et est
//     remplace par un veto en amont. Sans lui il se redeclencherait a chaque
//     vie et le bot se suiciderait en boucle jusqu'au game over.
class BotMemoire : public BotSpaceAnticp {
public:
    BotMemoire(Partie *p, float cadence, quint32 seed);

protected:
    void jouer(float dt) override;

    // Le veto s'ajoute au filtre du v3 : une pose dont la sortie tombe dans une
    // direction interdite est refusee, quel que soit `strict`. Interdire porte
    // sur la DIRECTION et non sur le type -- la croix traverse tout droit, elle
    // est donc le meme chemin que le tuyau droit de son axe.
    bool poseAcceptable(const ETypePiece& type, int col, int row, ESens entree,
                        bool strict) const override;

    // Sur la derniere vie, le veto cede : le bot joue pour survivre, plus pour
    // explorer. On cherche donc d'abord un pont hors des directions interdites,
    // puis on accepte tout -- sans quoi il se suiciderait sur place au lieu de
    // reprendre la branche connue.
    int choisirPont(int col, int row, ESens entree, bool strict) const override;

private:
    // Un carrefour journalise : l'etat de la tete au moment ou le trace y est
    // arrive, et la direction qu'il a fini par prendre.
    struct Carrefour {
        int col;
        int row;
        ESens entree;
        int restant;        // objectifRestant() a cet instant
        int capacite[4];    // par ESens, 0 = direction morte
        ESens choisie;      // direction effectivement prise
        int prise;          // capacite[choisie]
        int meilleure;      // la plus grande des capacites offertes
        int gravite;        // voir classerBlame
    };

    // --- frontieres -------------------------------------------------------
    //
    // Trois signaux, et il faut les trois. numeroManche() monte a chaque
    // manche, reussie comme perdue comme neuve, donc il ne dit pas laquelle.
    // niveau() separe la manche reussie du rejeu. getGraine() seule signale la
    // partie neuve : au game over le niveau retombe au niveau de depart, ce qui
    // est indiscernable d'une victoire si l'on regarde le niveau tout seul.
    void surveillerFrontieres();
    void oublierTout();
    void consommerUnBlame();

    // --- journal ----------------------------------------------------------
    void journaliser(int col, int row, ESens entree);
    void ouvrirCarrefour(int col, int row, ESens entree);
    void fermerCarrefour();

    // --- blame ------------------------------------------------------------
    //
    // noter() remplit la gravite de TOUT le journal, classerBlame() n'en garde
    // que les carrefours significatifs. Les deux sont separes parce que le
    // desamorcage remonte le journal entier : il lui faut la gravite des
    // carrefours que le blame a ecartes, sans quoi il reposerait le veto sur le
    // premier venu.
    static void noter(QVector<Carrefour> &journal);
    static QVector<Carrefour> classerBlame(const QVector<Carrefour> &note);

    // --- interdits --------------------------------------------------------
    int cle(int col, int row, ESens entree) const;
    // Direction ou `type` emmene le flux entre par `entree`, ou false si le
    // type ne s'y raccorde pas.
    static bool sortieDe(const ETypePiece& type, ESens entree, ESens &sortie);
    bool interdite(int col, int row, ESens entree, const ETypePiece& type) const;
    void interdire(int col, int row, ESens entree, ESens direction);
    void lever(int col, int row, ESens entree);

    // --- desamorcage ------------------------------------------------------
    //
    // Un veto dur peut condamner la tete quand la direction interdite est la
    // seule vivante : le carrefour n'etait pas une bifurcation mais un passage
    // oblige. Le suicide est informatif, mais une fois. On leve donc le veto et
    // on le repose sur le carrefour d'AMONT -- si le bot ne devait pas passer
    // ici, c'est plus tot qu'il fallait bifurquer.
    void desamorcer(int col, int row, ESens entree);

    QHash<int, unsigned char> interdits;   // cle() -> masque de 1 << ESens

    // Journal de la manche en cours, et celui de l'essai 1 fige avec le blame :
    // le desamorcage a besoin de l'amont du carrefour, qui n'existe que la.
    QVector<Carrefour> journal;
    QVector<Carrefour> reference;
    QVector<Carrefour> blame;
    int blameConsomme = 0;
    bool blameFige = false;

    // Carrefour ouvert : la tete est arrivee ici, on ne sait pas encore ou elle
    // repartira.
    bool ouvert = false;
    int oCol = -1;
    int oRow = -1;
    ESens oEntree = sHaut;
    int oRestant = 0;
    int oCapacite[4] = {0, 0, 0, 0};

    quint32 graineVue = 0;
    int niveauVu = 0;
    int mancheVue = 0;
    // Le journal ne se refait que quand le plateau bouge : la tete ne peut pas
    // avoir bouge sans ca, et mesurer quatre capacites non bornees a chaque
    // battement coute pour rien.
    quint32 signatureJournal = 0;

    // Veto applique meme sur la derniere vie : la passe preferentielle de
    // choisirPont. Mutable parce que poseAcceptable et choisirPont sont const
    // par contrat du v3.
    mutable bool vetoForce = false;
};

#endif // BOTMEMOIRE_H
