#ifndef MINAGE_H
#define MINAGE_H

#include <QVector>
#include "game.h"
#include "ecoulement.h"

// Ce qu'une bombe met a sauter, en secondes de SIMULATION -- le meme temps que
// le flux. L'acceleration la comprime donc exactement comme elle comprime tout
// le reste : la bombe saute toujours apres le meme nombre de cases remplies, a
// 1x comme a 8x. Sur l'horloge du mur, foncer changerait la regle et le banc ne
// pourrait plus rejouer une partie. Voir BOMBES.md.
//
// La valeur est un point de reglage, pas une constante physique : il faut qu'un
// humain ait le temps de voir la bombe avant qu'elle parte. Essayee manette en
// main et retenue telle quelle le 14 septembre 2026.
#define DUREE_BOMBE     2.5f

// Une bombe posee : sa case, et ce qu'il lui reste a vivre. Pas une classe --
// elle n'a aucun comportement propre. Ce qui en a, c'est la resolution
// COLLECTIVE : une bombe prise dans un souffle est annulee sans exploser, une
// explosion mortelle produit UNE defaite, et le deminage est un etat commun.
typedef struct _SBombe {
    int idx;
    float restant;
} SBombe;

// Les bombes posees, et les blocs fixes qu'elles ont ouverts.
//
// Range comme Ecoulement : aucune horloge interne, la Partie l'avance avec son
// dt. Deux etats, et c'est toute la raison de la classe -- ils n'ont pas la
// meme duree de vie. Les bombes actives ne passent pas la manche ; les blocs
// demines, eux, valent pour tous les rejeux du niveau.
//
// La geometrie et le temps sont ici ; les regles restent a Partie -- le stock,
// la defaite, et le fait qu'une explosion qui tue ne credite aucun deminage.
class Minage
{
public:
    // Le plateau n'est pas const : c'est ici qu'on le pulverise. L'ecoulement
    // l'est : on lui demande seulement ce qui est deja rempli.
    Minage(Game *plateau, const Ecoulement *ecoul);

    // Pose une bombe sur une case TOTALEMENT vide, et rien d'autre : ni sur un
    // bloc, ni sur un tuyau, ni sur un rebut. Ne regarde pas le stock, qui est
    // une regle de Partie. False si la case n'est pas libre.
    bool poser(int col, int row);

    // Avance tous les timers et resout les explosions arrivees a terme.
    // Renvoie vrai si l'une d'elles a emporte un tuyau PLEIN : c'est la mort
    // immediate, et c'est Partie qui en tire les consequences.
    bool avancer(float dt);

    // Les bombes posees ne survivent pas a la manche : le plateau repart vide,
    // et elles etaient deja decomptees du stock. Une bombe qui n'a pas eu le
    // temps de sauter est perdue, comme une bombe depensee.
    void viderManche();
    // Fin du niveau : le deminage tombe avec lui. Il ne vaut que pour les
    // rejeux du niveau qui l'a paye.
    void viderNiveau();

    // A appeler juste apres Game::reinitialiser() : efface les blocs deja
    // ouverts. La graine etant la meme d'un rejeu a l'autre, les blocs
    // retombent aux memes cases -- les effacer apres coup suffit, et Game n'a
    // pas a connaitre l'existence des bombes.
    void appliquerDeminage() const;

    // 1 au moment de la pose, 0 a l'explosion, -1 s'il n'y a pas de bombe ici.
    // C'est ce que le compte a rebours dessine.
    float fractionRestante(int col, int row) const;
    int nbActives() const;

    // Les cases des bombes qui viennent de sauter, retirees de la liste au
    // passage : l'appelant les consomme une fois, et qui ne les releve pas ne
    // paie rien.
    //
    // C'est le seul moyen de savoir qu'une explosion a eu lieu : elle ne laisse
    // aucune trace sur le plateau -- un 3x3 pulverise ressemble exactement a un
    // 3x3 jamais occupe. L'affichage s'en sert pour son flash ; le moteur, lui,
    // a deja tout fait.
    QVector<int> preleverExplosions();

private:
    Q_DISABLE_COPY(Minage)

    // Pulverise le 3x3 autour de la case. Renvoie vrai si un tuyau plein s'y
    // trouvait. Les blocs ouverts ne sont acquis que si l'explosion n'a pas
    // tue : se tuer avec sa propre bombe annule exactement ce qu'elle
    // rapportait, sinon bombarder son tuyau plein serait la facon la moins
    // chere de deminer.
    bool exploser(int idx);
    void retirer(int idx);
    int indiceDe(int idx) const;

    Game *plateau = nullptr;
    const Ecoulement *ecoul = nullptr;
    QVector<SBombe> bombes;
    // Ce qui a saute depuis le dernier releve. Ne sert a aucune regle : c'est
    // un tampon d'evenements pour l'affichage, vide des qu'on le lit.
    QVector<int> explosions;
    // Cases ou un bloc fixe a ete ouvert, et ou il ne doit plus revenir tant
    // qu'on rejoue ce niveau.
    QVector<int> demine;
};

#endif // MINAGE_H
