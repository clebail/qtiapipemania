#ifndef BOTSPACEANTICP_H
#define BOTSPACEANTICP_H

#include "bot.h"

// v3. Le v2, plus l'anticipation. A chaque geste il choisit dans la file la
// piece `pont` a amener sur la tete (choisirPont) -- ici la premiere qui s'y
// raccorde en laissant de quoi finir, ou, si aucune ne le fait, la premiere qui
// s'y raccorde sans se condamner sur le coup. Si ce pont est plus loin que le haut de file, il
// projette la chaine de cases que le trace suivra apres lui et pre-pose le haut
// de file sur la premiere case libre de cette chaine -- comptee comme du tas.
// Geste apres geste, les pieces d'avant le pont s'y enchainent ; quand le pont
// arrive au sommet il va sur la tete, et Bot::tete() sort la chaine du tas au
// passage. Le reste (pose directe, defausse, fin de manche) est celui du v2.
class BotSpaceAnticp : public Bot {
public:
    BotSpaceAnticp(Partie *p, float cadence, quint32 seed);
protected:
    void jouer(float dt) override;

    // Renseigne Bot::ancrageDefausse avec la case de RANG 2 du trajet anticipe :
    // le trou qui suit la premiere case libre de la chaine. Le rang 1 est
    // determine et le bot y gare deja ; le rang 2 depend de ce qu'on mettra
    // dans le rang 1, donc c'est un pari -- mais un pari a un seul coup, qui se
    // tranche des que le rang 1 est rempli. Mesure : il tombe juste 39 % du
    // temps, contre 31 % de rendement pour une defausse ordinaire.
    //
    // La defausse s'en sert deux fois : comme case preferee, et comme point
    // d'ancrage tant qu'aucun troncon n'existe pour s'accrocher.
    void ancrerDefausse(int col, int row, ESens entree);

    // Indice de la piece de la file a poser sur la tete (le "pont"), ou -1 si
    // aucune ne convient. v3 prend la premiere qui va ; les sous-classes
    // raffinent le choix. `strict` est passe tel quel a poseAcceptable.
    virtual int choisirPont(int col, int row, ESens entree, bool strict) const;

    // Vrai si poser `type` sur cette case (avec cette entree) est acceptable.
    // A `strict` faux, il suffit de ne pas se condamner sur le coup
    // (meneALaMort) ; a vrai, la pose doit en plus laisser de quoi boucler
    // l'objectif (culDeSac). Sert aussi bien au pont qu'aux pieces pre-posees
    // le long de la chaine (caseAnticipee) : une sous-classe qui durcit ce
    // filtre le durcit donc aux deux endroits, pas seulement au choix du pont.
    virtual bool poseAcceptable(const ETypePiece& type, int col, int row, ESens entree,
                                bool strict) const;

    // Un coup que le v3 ne connait pas. Appele une fois la tete connue et avant
    // le choix du pont : une sous-classe qui rend vrai a joue son geste, et le
    // v3 lui laisse la main. Le v3 lui-meme n'en a aucun, d'ou le faux -- son
    // deroulement est donc inchange, bit pour bit.
    virtual bool coupSpecial(int col, int row, ESens entree);

    // Premiere case libre de la chaine que le trace suivra apres avoir pose
    // file[pont] sur la tete (tCol,tRow,tEntree) : les cases deja pre-posees
    // d'un tour precedent sont traversees, la chaine s'arrete sur une case en
    // travers ou hors grille. Renvoie false si aucune case libre. Pure
    // projection, ne touche a rien.
    // `chaine`, si non nul, recoit toutes les cases du trajet projete --
    // celles deja posees comme celles encore vides. Ces dernieres sont les
    // "trous du flux" : le trace y passera, elles ne sont donc pas de la place
    // disponible, et les compter comme telles gonflait l'espace percu.
    bool caseAnticipee(int tCol, int tRow, ESens tEntree, int pont,
                       int &fCol, int &fRow, ESens &fEntree,
                       QVector<int> *chaine = nullptr) const;

    // Vrai si, une fois `type` pose en (fCol,fRow), le pont sur la tete mene
    // encore quelque part. Juge la CHAINE, pas le maillon : voir le commentaire
    // dans botspaceanticp.cpp.
    bool chaineViable(int col, int row, ESens entree, int pont,
                      int fCol, int fRow, const ETypePiece &type,
                      const QVector<int> &chaine) const;
};

#endif // BOTSPACEANTICP_H
