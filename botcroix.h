#ifndef BOTCROIX_H
#define BOTCROIX_H

#include "botmemoire.h"

// v5. Le v4, plus l'echange d'un tuyau droit contre une croix.
//
// Ce que le v3 et le v4 ignorent : une piece deja posee n'est pas forcement un
// mur. `meneALaMort` la traite comme tel -- "une piece deja posee qu'on ne
// pourra ni traverser (seule la croix se laisse traverser) ni reprendre" -- et
// c'est vrai du coude, faux du droit.
//
// Un DROIT se remplace par une croix sans rien casser, et la preuve est deja
// dans le code : `Bot::memeRoutage(tpCroix, tpHorizontal)` rend vrai. La croix
// couvre les entrees du droit sans devier le flux, donc l'axe deja emprunte est
// preserve exactement -- et l'axe perpendiculaire s'ouvre. La case cesse d'etre
// un mur et devient un passage, pour 25 points (PENALITE_REMPLACEMENT) et un
// geste.
//
// Le marche est bon : la croix traversee deux fois compte deux cases
// (Ecoulement::avancer, "les deux conduites d'une croix comptent chacune pour
// une case"), soit 100 points contre 25, et la traversee supplementaire compte
// aussi dans l'objectif et dans la belle manche a 110.
//
// Et a partir du niveau 49 ce n'est plus une optimisation, c'est le ticket
// d'entree : l'objectif (202, puis 220 des le niveau 54) depasse les 200 cases
// traversables du plateau. Sans croisements, la manche est impossible.
//
// La regle, arretee avec le user : quand la tete va PLUS LOIN en echangeant un
// droit une ou deux cases devant elle, on echange. Pas de seuil, pas de debat --
// l'echange ne peut pas casser le trace, donc le seul critere est le gain.
//
// Les COUDES ne sont pas traites ici : les remplacer change le routage (aucune
// piece ne route comme un coude a part lui-meme, memeRoutage le dit), donc il
// faut que le motif du futur flux s'y prete. C'est un critere a etablir.
class BotCroix : public BotMemoire {
public:
    BotCroix(Partie *p, float cadence, quint32 seed);

protected:
    // L'echange, s'il y a lieu. Ne joue que si une croix est au sommet de la
    // file : on ne pose jamais que le haut de file.
    bool coupSpecial(int col, int row, ESens entree) override;

private:
    // Ce que la tete peut encore parcourir, au mieux de ses types compatibles.
    // Non bornee : on compare des portees entre elles, une mesure bornee les
    // rendrait toutes egales au plafond.
    int porteeTete(int col, int row, ESens entree) const;

    // La case porte-t-elle un droit echangeable contre une croix ? Vrai pour
    // les seuls tpHorizontal et tpVertical encore posables -- le flux n'y est
    // pas passe, ce n'est ni le reservoir ni un bloc.
    bool droitEchangeable(int col, int row) const;

    // Portee de la tete une fois `col,row` change en croix. L'echange est joue
    // pour de faux et defait, comme le fait chaineViable.
    int porteeSiCroix(int cCol, int cRow, int tCol, int tRow, ESens tEntree) const;
};

#endif // BOTCROIX_H
