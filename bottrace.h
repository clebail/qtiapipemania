#ifndef BOTTRACE_H
#define BOTTRACE_H

#include <atomic>
#include <memory>

#include "botmemoire.h"
#include "trace.h"

// v5. Le trace planifie -- TRACE.md.
//
// Tous les bots precedents (glouton -> space -> spaceAnticp -> memoire)
// partagent la meme forme : ils regardent la tete de construction, la piece du
// haut de file, et decident ici et maintenant. Ce qu'ils savent du futur tient
// dans un pont de quelques cases et dans un flood-fill qui mesure la place
// devant.
//
// Celui-ci renverse la chose. Le chemin complet du reservoir jusqu'a l'objectif
// est calcule UNE FOIS, au debut de la manche, sur le plateau de la manche
// (Trace). Chaque case du trace a des lors son type determine -- le type d'une
// case se deduit entierement du couple (entree, sortie). Le bot ne decide plus
// ou aller : il affecte des pieces a des cases.
//
// Le fait technique qui porte tout. Sans trace, la piece en main est utilisable
// a UN SEUL endroit, la tete : elle y passe 4 fois sur 7, et sinon elle part a
// la defausse. Avec un trace, elle est utilisable PARTOUT ou le trace reclame
// son type -- et le generateur equilibre son histogramme pour que les sept
// types tires aient tous un emploi (la croix via les droits, qu'elle traverse).
// La contrainte cesse d'etre temporelle ("il me faut ca, ici, maintenant") et
// devient une affectation ("il me faut ca, quelque part").
//
// Mesure d'avant, a battre : le v4 fait servir 47,7 % de ses pieces, et 63,5 %
// de ses gestes sont des defausses (TRACE.md, §2).
//
// Trois regles, et elles tiennent en trois lignes de code :
//
//   - POSER sur la case la plus EN AMONT du trace qui reclame le type en main.
//     On ne va loin que quand rien de proche ne l'accepte. Ce n'est pas un
//     reglage : le flux s'arrete a la premiere case trouee, donc ce qui compte
//     est la longueur du prefixe contigu. En servant l'amont il avance par
//     bonds au rythme du tirage ; en servant l'aval il n'avance que quand tombe
//     le type exact de la case bloquante, une chance sur sept, contre une
//     seconde par case pour le flux (TRACE.md, §3).
//   - DEFAUSSER le reste sur le pavage, qui ne couvre plus que le hors trace.
//   - NE JAMAIS defausser sur une case du trace (reserveeAuTrace).
//
// Ce que ce bot ne fait pas encore, et qui est ecrit noir sur blanc dans
// TRACE.md : les redemarrages perturbes quand l'objectif n'est pas atteint
// (§6 ter, palier 1 -- la perturbation reste a trancher, §7), le bombardement
// pour replanifier (palier 2), et l'auto-croisement sans lequel il n'existe
// aucun trace au-dela du niveau 49 (§4). Quand le generateur rend trop court,
// le bot joue donc le plus long trace trouve -- palier 3, qui n'est jamais pire
// qu'improviser.
// DEUX RELEVES, et elles n'ont pas le meme motif. Le trace est calcule pour
// atteindre `longueurMinimale` : il n'existe ni au-dela de l'objectif, ni quand
// l'objectif est hors d'atteinte. Dans les deux cas il n'a plus rien a dire, et
// c'est un bot qui improvise devant le flux qui doit prendre la main.
//
//   - OBJECTIF ACQUIS -> le v3. Il n'y a plus de manche a gagner, seulement des
//     traversees a encaisser (50 points chacune), et prolonger la tete de
//     proche en proche est exactement ce qu'il sait faire.
//   - MANCHE IMPRENABLE (trace plus court que l'objectif, et plus une bombe
//     pour le rallonger) -> le v4. Mesure du 18 septembre, niveau 51 : le trace
//     rendait UNE case pour un objectif de 210, le bot posait deux pieces et
//     appuyait sur la barre d'espace -- six fois de suite, une par vie, plateau
//     vide a l'ecran. La manche etait perdue de toute facon ; autant la remplir
//     et encaisser ce qui passe. C'est la regle "pour le spectacle" demandee
//     dans BOMBES.md.
//
// D'ou l'heritage : BotTrace EST un BotMemoire, donc aussi un BotSpaceAnticp,
// et il delegue son jouer() a l'un ou a l'autre.
class BotTrace : public BotMemoire {
public:
    BotTrace(Partie *p, float cadence, quint32 seed);

    // Le trace que le bot suit reellement -- c'est lui que la grille dessine
    // des que ce bot joue, et non le calcul qu'elle refait dans son coin.
    const Trace *tracePlanifie() const override;

protected:
    void jouer(float dt) override;

    // LES BOMBES, et c'est le palier 2 de TRACE.md §6 ter enfin ecrit. La regle
    // de base -- une bombe par essai, rien avant le premier rejeu -- a ete
    // posee quand aucun bot ne pouvait savoir qu'une manche etait perdue
    // d'avance. Celui-ci le sait au premier battement : si le trace planifie
    // est plus court que l'objectif, la manche est imprenable, et la jouer
    // coute une vie pour rien.
    //
    // On bombe donc tant que le trace est trop court et qu'il reste du stock,
    // une bombe a la fois : chaque explosion refait le plan, donc le trace, et
    // la boucle s'arrete d'elle-meme des que l'objectif redevient atteignable.
    // Mesure du 18 septembre 2026 sur `--graine 791501436 --niveau 45`
    // (objectif 186) : sans bombe le trace fait 128, avec une 184, avec DEUX
    // 186. Au-dela ca plafonne -- ce n'est plus le terrain qui bloque mais le
    // budget de recherche.
    //
    // Une a la fois, et pas trois d'un coup : une bombe prise dans le souffle
    // d'une autre saute sans exploser (minage.cpp), et surtout on ne sait pas
    // avant de replanifier combien il en faut.
    bool bomberMaintenant() const override;

    // LA RELEVE N'A RIEN A PERDRE. Quand la manche est imprenable, le v4 joue
    // sans filet : plus aucune verification de place, il fonce en anticipant.
    //
    // L'exigence de place -- culDeSac, placeExigee, chaineViable -- n'a jamais
    // eu qu'un but : garder de quoi BOUCLER L'OBJECTIF. Sur une manche perdue
    // d'avance elle devient une exigence de garder de quoi faire ce qu'on ne
    // fera pas, et comme `objectifRestant` y vaut tout l'objectif, elle est
    // maximale exactement la ou elle ne sert a rien : culDeSac refuse tout,
    // ouvertureUtile tombe a zero, le bot defausse en boucle devant un plateau
    // vide. Il finissait la manche perdue ET sans points, quand il n'y avait
    // plus que des points a prendre -- 50 par traversee.
    //
    // Tout passe par objectifRestant, qui rend zero : le bot se retrouve dans
    // l'etat "objectif acquis", celui ou il construit du bonus sans se garder
    // de reserve. Il ne reste que meneALaMort, et c'est bien le seul filtre
    // qu'on veuille garder : mourir sur le coup arrete le comptage.
    bool rienAPerdre() const override;

    // SANS ESPOIR : le trace planifie ne vaut pas l'objectif, et plus aucune
    // bombe ne viendra le rallonger. C'est le seul signal stable -- les
    // traversees d'un rejeu, elles, sautent du simple au sextuple pour un
    // meme plan. Voir Bot::surveillerRejeu.
    bool niveauSansEspoir() const override;

    // OU BOMBER, pour un bot qui sait ce qu'une bombe doit acheter. Lance
    // l'etude au premier appel, rend false tant qu'elle vole, puis la case
    // retenue -- ou false pour de bon si aucune ne vaut le coup, auquel cas la
    // bombe reste en stock.
    bool choisirPaquetDeBlocs(int &col, int &row) const override;
    // Lance l'etude sur ce terrain-la. Separee de choisirPaquetDeBlocs parce
    // qu'elle sert deux fois : quand on cherche ou bomber, et JUSTE APRES
    // avoir choisi -- sur le terrain tel qu'il sera une fois le souffle passe.
    //
    // C'est ce second appel qui rend la chose jouable en temps reel. Une bombe
    // coute 3 s d'etude plus 2,5 s de meche ; enchainees, quatre bombes
    // demandent 22 s et le delai de depart en fait justement 22. En etudiant
    // pendant que la meche brule, le cycle retombe au plus long des deux.
    // Signale par le user, qui voyait la fenetre n'en poser que deux la ou le
    // banc -- ou le temps est simule -- en posait quatre.
    void lancerEtude(const Game &vierge, int courant, bool allonger,
                     bool depenser) const;

    // Le trace d'abord, le pavage ensuite : construirePlan doit voir les cases
    // reservees pour les masquer.
    void construirePlan() override;
    bool reserveeAuTrace(int col, int row) const override;

private:
    // Calcule le trace de la manche, sur un plateau NU : blocs et reservoir,
    // rien d'autre. Une piece deja posee est un obstacle pour Trace, et la
    // notre n'en est pas une -- c'est du trace deja servi.
    //
    // EN THREAD quand la fenetre l'a demande (setPlanificationAsynchrone), et
    // c'est la reponse a un gel que le user voyait a l'oeil : la recherche
    // coute jusqu'a 1,3 s sur un plateau charge (mesure du 2026-09-19, 20
    // graines par niveau : 0 ms jusqu'au niveau 20, 484 ms de moyenne et 876
    // ms au pire au niveau 30, 1325 ms au pire au niveau 35). Une par manche
    // passe encore, mais une bombe REPLANIFIE, et sur les gros niveaux le bot
    // en pose plusieurs d'affilee -- une seconde d'image figee a chaque
    // souffle.
    //
    // Le calcul ne touche a rien de partage : Trace est de la geometrie pure,
    // il recoit une COPIE du plateau nu et rend un Trace neuf. L'echange se
    // fait dans le thread appelant, au battement suivant (recolterPlan).
    void planifier();
    // Le calcul est-il en vol ? Tant qu'il l'est, le trace en place est perime
    // -- c'est celui d'avant l'explosion -- et le bot ne joue pas : ce qu'il
    // poserait serait decide sur un plan mort. C'est le meme silence que
    // pendant que la meche brule, prolonge de quelques dixiemes de seconde.
    bool planEnVol() const;
    // Une bombe est-elle encore a venir sur cette manche ? Soit une meche
    // brule, soit l'etude cherche encore ou la poser.
    //
    // Tant que la reponse est oui, le bot ne doit RIEN precipiter -- et
    // surtout pas demander la barre d'espace. Avec un trace d'une case il n'a
    // rien a servir, donc il foncait aussitot ; dans la fenetre, ou l'etude
    // tourne en thread, la manche etait finie avant que la bombe existe, et le
    // bot brulait ses vies sans jamais bomber. Au banc, ou l'etude est
    // synchrone, la bombe part dans le meme battement et la panne est
    // invisible : elle ne se voit qu'a l'ecran, ce que le user a fait.
    bool bombeAVenir() const;
    // Recupere le trace calcule par le thread s'il est pret, et enchaine le
    // pavage -- qui masque les cases du trace et ne peut donc pas le preceder.
    // Ne fait rien tant que le calcul vole, et n'attend jamais.
    void recolterPlan();
    // Le journal de planification, commun aux deux voies : le thread ne peut
    // pas l'ecrire lui-meme, qDebug n'est pas a lui.
    void annoncerPlan() const;
    // Le plateau reduit a ce qui ne bougera pas de la manche : blocs et
    // reservoir. Partage par la planification et par l'etude de bombe, qui
    // doivent voir exactement le meme terrain.
    Game plateauNu() const;
    // Installe le trace qui vient d'etre calcule -- SAUF s'il est plus court
    // que celui qu'il remplace, et que le plateau n'a pas change de manche.
    //
    // Une bombe ne peut pas faire perdre du terrain : elle enleve des blocs,
    // jamais l'inverse, donc le meilleur trace possible ne peut que s'allonger.
    // Ce qui recule, c'est la RECHERCHE -- plus de cases libres, plus d'espace
    // a explorer, un ordre de Warnsdorff different, et le budget s'epuise dans
    // une region moins bonne. Mesure du 2026-09-19 sur `--graine 2998034427
    // --niveau 30`, au niveau 31 : 12 cases, puis 118 apres la premiere bombe,
    // puis 98, puis 96, puis 96, puis 130 -- trois bombes depensees a faire du
    // surplace, et le bot aurait joue 96 si le stock s'etait vide la.
    //
    // Le trace d'avant reste jouable : ses cases sont toujours libres, le
    // souffle n'a pu qu'en ouvrir d'autres. On le garde donc, et la longueur
    // devient monotone au fil des bombes.
    void installerTrace(const Trace &calcule);
    // La case la plus en amont que le trace reclame pour ce type, -1 si aucune.
    //
    // LA CROIX EST SERVIE EN PRIORITE LA OU ELLE EST IRREMPLACABLE. Une case
    // de croisement ne peut etre servie QUE par une croix ; une case de droit,
    // elle, accepte deux types sur sept -- le droit de son axe, et la croix en
    // joker. Depenser la croix en joker pendant qu'un croisement l'attend, ce
    // n'est pas servir l'amont, c'est gacher la seule piece qui puisse faire
    // le travail.
    //
    // Sauf si ca presse : quand le flux talonne, l'amont l'emporte sur tout le
    // reste -- une case non servie devant le flux coute la manche, un
    // croisement non servi ne coute qu'une passe.
    //
    // Regle posee par le user le 2026-09-19, le jour ou le trace s'est mis a
    // reclamer de vraies croix.
    int caseAServir(const ETypePiece &enMain) const;
    // Le corps de caseAServir : la case la plus en amont qui accepte ce type,
    // en se limitant aux croisements quand on le demande.
    int premiereCaseServie(const ETypePiece &enMain, bool seulementCroisements) const;
    // Reste-t-il quoi que ce soit a poser sur le trace ? Faux veut dire que
    // plus aucun geste ne peut allonger le tuyau : on fonce.
    bool resteAServir() const;
    // Le trace planifie est-il trop court pour l'objectif ? C'est le signal que
    // la manche est imprenable en l'etat, et il est disponible des le premier
    // battement -- ce qu'aucun bot d'avant ne savait voir.
    bool traceTropCourt() const;
    // Et imprenable POUR DE BON : plus aucune bombe ne viendra rallonger le
    // trace, soit que le stock soit vide, soit que le flux soit deja parti --
    // on ne mine plus pendant l'ecoulement.
    bool mancheImprenable() const;

    // --- la file du rejeu -------------------------------------------------
    //
    // Le rejeu est PARFAIT : meme plateau, meme file, piece pour piece
    // (Partie::nouvelleManche, graines derivees de (graine, niveau)). Une
    // manche perdue au premier essai rend donc au bot la sequence exacte des
    // tirages a venir -- et la ou le premier essai devait se contenter des cinq
    // pieces visibles, le rejeu peut faire tenir au trace les QUARANTE-QUATRE
    // pieces qui tomberont avant le depart du flux.
    //
    // C'est la reponse du user au probleme du pari : on ne recalcule pas en
    // cours de route, on laisse sa chance au bot une fois, et on rejoue en
    // connaissance de cause.
    void noterFile();
    // Ce qu'on donne au generateur : les cinq pieces visibles au premier essai,
    // la sequence connue a partir du second -- bornee a ce qui tombera avant le
    // depart du flux, puisque au-dela l'ordre compte et que le multiensemble ne
    // le dit plus.
    QVector<ETypePiece> mainConnue() const;

    Trace trace;
    // Le trace en vigueur quand la planification courante a ete lancee, et si
    // elle a le droit de le remplacer par plus court. Voir installerTrace :
    // dans la meme manche -- donc apres une bombe -- elle ne l'a pas.
    Trace tracePrecedente;
    bool seulementSiPlusLong = false;
    // L'etat partage avec le thread de planification. Le thread en tient sa
    // propre reference : le bot peut donc lacher la sienne a tout moment --
    // changement de manche, bot desinstalle -- sans rien attendre et sans rien
    // laisser pendre. Le calcul abandonne finit dans son coin et se libere.
    //
    // C'est ce qui a decide la forme. Un std::future bloque son proprietaire
    // des qu'on le remplace ou qu'on le detruit, et il n'y a aucune raison
    // d'attendre le resultat d'une recherche qu'on vient de perimer : mesure
    // du 2026-09-19, une replanification lancee sur l'ancienne rendait a
    // l'appelant le gel qu'on voulait lui epargner.
    struct Planification {
        Trace trace;
        // Ecrit en dernier par le thread, lu en premier par le bot : c'est lui
        // qui publie `trace`, d'ou le relachement/acquisition explicite.
        std::atomic<bool> prete{false};
    };

    // Non nul tant qu'une planification vole.
    std::shared_ptr<Planification> calcul;

    // --- l'etude de bombe -------------------------------------------------
    //
    // Le ciblage de Bot vise le souffle qui emporte le plus de BLOCS. Pour un
    // bot qui planifie, ce n'est pas ce qu'une bombe doit acheter : ce qu'elle
    // doit acheter, c'est de la LONGUEUR DE TRACE. Les deux ne coincident pas,
    // et le releve du 2026-09-19 sur `--graine 2998034427` est sans appel --
    // pour chaque emplacement possible, le trace qui en resulte :
    //
    //   niveau   sans bombe   critere blocs   meilleur   mediane
    //       31           12             118        130        18
    //       33            1               4        135         1
    //       34           85              75        108        88
    //
    // Au niveau 34 le critere blocs fait PIRE que ne rien faire, et il est
    // sous la mediane des emplacements. Au 33 il rend 4 quand une autre case
    // en rend 135. Ce n'est pas un accident : il optimise autre chose.
    //
    // Alors on essaie. Chaque emplacement est souffle sur une copie du plateau
    // nu, le trace recalcule, et on garde le meilleur -- on connait donc le
    // resultat de la bombe AVANT de la poser. Le balayage entier tient en 2 a
    // 10 s dans le thread (voir BUDGET_ETUDE), ce que le user a explicitement
    // accepte : "perdre une vie pour faire mieux apres, c'est une strategie
    // tres honorable".
    struct Etude {
        // La case retenue, -1 si aucune ne vaut la bombe.
        int caseBombe = -1;
        // Et le trace qu'elle donnera : quand la bombe saute, il n'y a plus
        // rien a chercher.
        Trace trace;
        std::atomic<bool> prete{false};
    };

    // L'etude en vol. Mutable : choisirPaquetDeBlocs est const par contrat de
    // Bot, et c'est elle qui lance et recolte.
    mutable std::shared_ptr<Etude> etude;
    // Le trace calcule d'avance pour la bombe posee, en attente de son souffle.
    mutable Trace traceApresBombe;
    mutable bool traceApresBombePrete = false;
    // L'etude a conclu qu'aucun emplacement ne valait la bombe. Tient jusqu'a
    // ce que le terrain change -- sans quoi elle repart a chaque battement.
    mutable bool etudeFaite = false;

    // Frontieres suivies ici plutot que dans Bot : planifier() est appelee
    // AVANT jouer() au changement de manche, il lui faut donc son propre
    // compte d'essais.
    int niveauSuivi = -1;
    int mancheSuivie = -1;
    int essai = 0;
    // La sequence des types tires, relevee au premier essai du niveau et
    // valable pour tous ses rejeux. Videe quand le niveau change.
    QVector<ETypePiece> sequence;
    // Pieces consommees depuis le debut de la manche : c'est l'index d'ou
    // repart le futur connu.
    int consommees = 0;
    // Taille de la main passee a la derniere planification -- le journal la
    // rend, et en asynchrone il s'ecrit longtemps apres l'appel.
    int piecesPlanifiees = 0;
};

#endif // BOTTRACE_H
