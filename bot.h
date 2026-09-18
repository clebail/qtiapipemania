#ifndef BOT_H
#define BOT_H

#include <QVector>
#include <QPair>
#include "partie.h"

class Trace;

class Bot
{
public:
    Bot(Partie *p, float cadence, quint32 seed);
    virtual ~Bot() = default;

    // Tenue de comptes commune a tous les bots -- remise a zero du tas quand la
    // manche change -- puis appel de jouer(). C'est ce point d'entree que la
    // fenetre et le banc appellent ; les sous-classes n'ecrivent que jouer().
    void avancer(float dt);

    // Piece que le plan de defausse reclame sur cette case, tpNone la ou il ne
    // dit rien. Le plan est recalcule a chaque manche sur le plateau de cette
    // manche-la (voir construirePlan) : il est clos -- aucune de ses ouvertures
    // ne bute sur un bloc ni sur le bord -- au prix de quelques cases qu'il
    // renonce a couvrir. Publique parce que la grille l'affiche en overlay.
    ETypePiece planType(int col, int row) const;

    // Vrai quand le plan ne fait qu'enteriner une OBLIGATION : pour une entree
    // au moins, un seul routage survit sur cette case, et c'est celui que le
    // plan y reclame. Le flux qui entre par la en sortira par la, ou la manche
    // est perdue -- il n'y a pas de choix a faire, seulement un constat.
    //
    // L'obligation porte sur la DIRECTION, jamais sur le type : la croix
    // traverse tout droit (Ecoulement::sorties), donc elle et le tuyau droit de
    // son axe sont un seul et meme chemin. Compter les types verrait un choix
    // la ou il n'y en a pas.
    //
    // Simple lecture : la marque est posee par marquerObligations(), refaite
    // des que le plateau bouge. Elle ne se fige PAS au debut de la manche --
    // chaque piece posee ferme des issues autour d'elle, et une case qui
    // offrait encore un choix devient forcee. Le plateau vide n'en montre
    // qu'une vingtaine ; l'essentiel se fabrique en cours de manche.
    //
    // La grille s'en sert pour epaissir le trait du plan, la defausse pour y
    // aller en premier.
    bool planObligatoire(int col, int row) const;
    // Le flux qui entre ici par ce cote a-t-il une suite ? Simple lecture du
    // point fixe pose par marquerMortes().
    bool etatMort(int col, int row, ESens entree) const;
    // Morte par tous les cotes : aucun trace ne la traversera jamais, quelle
    // que soit la piece et quel que soit le chemin. Ce n'est plus une
    // contrainte de routage, c'est de la surface perdue. (La grille l'affiche.)
    bool caseMorte(int col, int row) const;
    // Le point fixe des morts ne sert a AUCUNE decision : le brancher sur le
    // marquage coutait -0,380 niveau (voir le commentaire dans
    // marquerObligations). Il ne reste utile qu'a l'oeil, et il coute 2,5x en
    // simulation -- on ne le calcule donc que quand la grille l'affiche.
    void setCalculerMortes(bool calculer);

    // Vrai si cette case du tas a ete posee par l'ANTICIPATION -- une chaine
    // projetee pour un pont qui n'est peut-etre jamais venu -- et non par la
    // defausse selon le plan. Les deux etaient indistinguables jusqu'ici, ce
    // qui les faisait traiter avec le meme respect : une case speculative
    // valait une case du plan.
    bool estAnticipee(int col, int row) const;

    // Vrai si cette case du tas a ete posee sur un PARI : la case de rang 2 du
    // trajet anticipe, qui ne sera sur le chemin que si le rang 1 recoit le
    // type suppose. Le plan de defausse n'a pas son mot a dire sur cette
    // pose-la : un pari perdu est une piece depensee, et on aime les voir.
    bool estPari(int col, int row) const;

    // Vrai si le tas de defausse occupe cette case. N'a d'interet que pour les
    // bots : sert a l'overlay qui montre ou le bot jette ce qu'il ne peut pas
    // poser sur la tete de construction.
    bool estTas(int col, int row) const;

    // Le code d'origine que le tas porte sur cette case, 0 quand elle n'est pas
    // au tas : 1 = defausse selon le plan, 2 = pre-pose de l'anticipation,
    // 3 = defausse sur un pari. C'est estTas/estAnticipee/estPari en un seul
    // appel, pour le journal qui veut la valeur et non trois questions.
    //
    // ATTENTION, c'est l'etat de la CASE et non l'auteur du dernier geste : une
    // pose du trace sur un rebut laisse la marque en place jusqu'a ce que
    // Bot::tete() la convertisse, au geste suivant. Qui veut savoir d'ou vient
    // une pose doit comparer l'avant et l'apres (voir MainWindow::battement).
    unsigned char origineTas(int col, int row) const;

    // Vrai si poser une piece de ce type sur la tete (col, row, entree)
    // condamne la manche : son unique sortie -- la croix va tout droit -- bute
    // sur un mur, une case bloquee, ou une piece deja posee qu'on ne pourra ni
    // traverser (seule la croix se laisse traverser) ni reprendre (seules les
    // pieces du tas se reprennent). Le type est suppose compatible avec
    // `entree`, tel que le renvoie Ecoulement::piecesCompatibles().
    bool meneALaMort(const ETypePiece& type, int col, int row, ESens entree) const;

    // Le trace planifie que ce bot suit, nul pour ceux qui n'en ont pas -- donc
    // pour tous sauf BotTrace. La grille s'en sert pour dessiner la ligne verte
    // : sans ca elle refait le calcul dans son coin, et les deux divergent des
    // qu'une bombe a saute, puisque le bot replanifie et pas elle.
    virtual const Trace *tracePlanifie() const;

    // Le bot a-t-il le droit de calculer hors du thread qui l'appelle ?
    //
    // Non par defaut, et ce defaut-la est le bon : le banc doit rendre deux
    // fois le meme chiffre pour la meme graine, et un bot dont les gestes
    // dependent de l'instant ou un thread rend la main ne le peut pas. Toute
    // la methode de mesure du projet -- comparaison appariee, quelques
    // centiemes de niveau moyen -- repose la-dessus.
    //
    // La fenetre, elle, l'allume : c'est la seule qui ait un affichage a ne
    // pas geler, et la seule ou le determinisme ne se mesure pas. Voir
    // BotTrace::planifier, le seul calcul assez long pour se voir.
    void setPlanificationAsynchrone(bool oui);

    // Combien de rejeux RIGOUREUSEMENT identiques d'affilee font jeter
    // l'eponge, zero pour ne jamais abandonner. Deux par defaut.
    //
    // Pourquoi deux et pas un : le premier rejeu identique est une
    // information -- il dit que le bot a rejoue son coup sans rien changer --
    // le second dit que la boucle est fermee, et il n'y a plus de raison
    // qu'elle s'ouvre. Meme plateau, meme file, memes gestes : la troisieme
    // manche sera la copie des deux premieres, et les six vies qui restent
    // ne feront que la repeter.
    void setRejeuxAvantAbandon(int rejeux);

    // Vrai quand le bot reclame la barre d'espace : lancer le flux tout de
    // suite et derouler la fin sans attendre. La fenetre la lui accorde
    // exactement comme au joueur. Retombe seule au changement de manche.
    bool veutFoncer() const;

    // Marques rendues par regionApresPoseSurTete(), une par case.
    enum EMarqueRegion {
        mrRien = 0,
        mrAtteignable = 1,  // le trace peut encore passer par la
        mrPose = 2          // la tete : la ou la piece du haut de file irait
    };

    // --- Pilotage manuel (mode pas a pas de la fenetre) ---
    //
    // Le bot ne choisit plus rien : la fenetre lui dicte le geste, et il
    // l'execute avec ses propres primitives -- meme tete de construction, meme
    // plan de defausse que lorsqu'il joue seul. Ce qu'on voit en pas a pas est
    // donc bien ce que le bot ferait, geste par geste, sans le chronometre.

    // Un geste, et un seul, joue par le bot lui-meme : c'est sa strategie qui
    // choisit, pas la fenetre. Le temps de jeu ne bouge pas pour autant -- on
    // se contente de lui verser de quoi acheter un jeton, la cadence n'entre
    // donc pas en compte. C'est le bouton qui manquait au pas a pas : sans lui
    // on ne pouvait que jouer a la place du bot, jamais le regarder decider.
    void unGeste();

    // Pose la piece du haut de file sur la tete de construction, quelle qu'elle
    // soit : c'est la fenetre qui decide, y compris de se condamner. False s'il
    // n'y a pas de tete ou si la pose est refusee.
    bool poserSurTete();
    // Ce que la pose de la piece du haut de file sur la tete laisserait devant
    // elle : un octet par case (index row * largeur + col), valeurs de
    // EMarqueRegion. Vide s'il n'y a pas de tete. Une piece qui ne se raccorde
    // pas a la tete, ou qui bute sur un mur, ne marque que la case de pose --
    // et c'est la bonne reponse : cette pose ne laisse aucune suite.
    //
    // Pas une simple lecture : elle passe par Bot::tete(), qui sort du tas les
    // cases que le trace a rejointes. C'est exactement ce que le prochain geste
    // du bot ferait de toute facon, mais ca explique le non-const.
    QVector<unsigned char> regionApresPoseSurTete();

    // Pose la piece du haut de la file et l'inscrit au tas. Sans effet si le
    // plateau est plein : la piece reste en file.
    //
    // Le tas n'est pas un depotoir mais un reseau parallele, et il n'est plus
    // improvise : la case est celle que reclame le plan de defausse, un pavage
    // du plateau calcule hors ligne (voir motifPlan dans bot.cpp). Y poser une
    // piece a sa place, c'est poser une piece qui ne peut pas etre mort-nee --
    // le plan ne laisse aucune ouverture buter sur un flanc plein -- et que
    // Bot::tete() absorbera gratuitement, sans geste ni remplacement, quand le
    // trace finira par l'atteindre.
    //
    // Parmi les cases que le plan reclame pour ce type, la plus eloignee du
    // reservoir : le flux met plus longtemps a y arriver, donc la piece a plus
    // de temps pour voir ses voisines la rejoindre. Les cases vides passent
    // d'abord ; si le plan n'en offre plus, une piece deja posee est ecrasee --
    // une case du tas de preference, une case du trace principal en dernier
    // recours seulement.
    void defausser();
protected:
    virtual void jouer(float dt) = 0;

    bool prendreJeton(float dt);
    void coinLePlusEloigne(int &x, int &y) const;
    // Case ou debouche le reservoir : la premiere que le flux traversera.
    // game.cpp la garantit libre de tout obstacle.
    void sortieReservoir(int &col, int &row) const;
    // Recalcule le plan de defausse sur le plateau courant. Appele a la
    // creation et a chaque changement de manche, pas plus : c'est une recherche
    // de quelques dizaines de millisecondes, gratuite une fois par manche mais
    // hors de question a chaque geste.
    //
    // Virtuelle pour le seul bot qui ait quelque chose a faire AVANT le pavage :
    // celui qui planifie un trace doit l'avoir calcule pour pouvoir le masquer
    // (voir reserveeAuTrace). Les deux points d'appel -- changement de manche et
    // explosion de la bombe -- sont exactement ceux ou le trace se refait, ce
    // qui evite un second declencheur a tenir en phase avec celui-ci.
    virtual void construirePlan();
    // Cette case est-elle reservee, donc interdite au reseau de defausse ?
    // Faux partout par defaut : sans trace planifie, le bot ne sait pas ou il
    // ira, et c'est justement ce qui oblige a paver le plateau entier.
    //
    // Un trace, lui, supprime l'ignorance la ou elle coutait : ses cases ont un
    // type connu, pas parie. Elles sortent donc du pavage -- passees a
    // construirePlan comme des pseudo-blocs -- et la defausse n'a plus le droit
    // d'y tomber, filet de securite compris. Pas pour les points : une case du
    // tas qui ne raccorde pas est precisement la ou Bot::tete() fait
    // reconstruire, et le bot brulerait un geste a reecrire ce qu'il vient de
    // poser. Voir TRACE.md, §6.
    virtual bool reserveeAuTrace(int col, int row) const;
    // Le drapeau pose par setPlanificationAsynchrone.
    bool planificationAsynchrone() const;
    // LE REJEU QUI TOURNE EN ROND. Appelee a chaque battement : a la fin de
    // chaque manche perdue, regarde si le rejeu a fait MIEUX que les
    // precedents du meme niveau, et jette l'eponge (Partie::abandonner) quand
    // plusieurs de suite n'apportent rien.
    //
    // Le juge est le PLAN, pas la manche. Deux criteres ont ete essayes et
    // jetes, et ils se trompaient de grandeur :
    //
    //   - l'empreinte du plateau final, "deux manches identiques case par
    //     case" : trop strict. La file connue grandit d'un rejeu a l'autre,
    //     donc le trace change un peu, donc la manche n'est jamais exactement
    //     la meme -- le bot rejouait indefiniment sans jamais abandonner ;
    //   - les traversees du rejeu : trop instable. Sur `--graine 1188181038
    //     --niveau 38`, elles oscillent entre 20 et 133 d'un rejeu a l'autre
    //     pour un MEME plan. Juger la-dessus, c'est abandonner sur deux
    //     malchances ou jamais sur un coup de bol.
    //
    // Ce qui est stable, et connu avant meme de jouer, c'est ce que le plan
    // permet : un trace de 130 pour un objectif de 158 ne gagnera pas, quelle
    // que soit la chance du rejeu. Le user : "un coup il va jusqu'a 130 et
    // quelques, un coup jusque vers 20, mais le trace reste a 130, c'est ca
    // qui compte."
    void surveillerRejeu();
    // Ce niveau est-il sans espoir ? Faux partout par defaut : un bot qui ne
    // planifie pas ne peut pas le savoir, et ne doit donc jamais abandonner.
    // Celui qui planifie, si -- son trace est trop court et plus une bombe ne
    // viendra le rallonger.
    virtual bool niveauSansEspoir() const;
    // Pose la marque des cases obligees, a la fin de construirePlan et donc sur
    // un plateau encore vide. Voir planObligatoire pour ce que "oblige" veut
    // dire, et le commentaire d'ISSUE_MINIMALE dans bot.cpp pour le seuil qui
    // distingue une vraie issue d'un cul-de-sac.
    void marquerObligations();
    // Les entrees DEJA DECIDEES sur une case : les cotes ou une piece est
    // posee et s'ouvre sur nous. Le trace est une ligne -- s'il arrive ici, il
    // arrive par la, et les cotes encore libres ne sont plus des hypotheses
    // recevables. Vide quand rien n'est pose autour : l'entree est alors
    // reellement ouverte, et marquerObligations retombe sur le type du plan.
    QVector<ESens> entreesDecidees(int col, int row) const;
    // Propage l'entree en avant depuis la tete du trace. Tant qu'une case n'a
    // qu'une issue vivante, la case suivante est atteinte par un cote connu :
    // ce n'est plus une hypothese, c'est le seul chemin qui reste. Remplit
    // `entreeTete`, refait a chaque marquage.
    //
    // L'ancrage vient d'Ecoulement::tete() et non de Bot::tete() : celle-ci
    // vide `tas` au passage (les rebuts traverses sont absorbes), et le
    // marquage ne doit rien decider. La version d'Ecoulement est un constat.
    void propagerDepuisTete(int besoin);
    // Le point fixe des ETATS MORTS. Un etat, c'est un couple (case, cote par
    // lequel le flux y entre) -- 225 x 4 pour le plateau. Il est mort quand
    // aucune piece posable ne debouche sur un etat vivant.
    //
    // Le calcul est arriere et iteratif : une case au fond d'une impasse meurt
    // d'abord parce qu'elle n'a aucune sortie, puis sa voisine perd une issue,
    // et ainsi de suite jusqu'a stabilite. C'est le pendant exact de
    // l'obligation -- meme decompte d'issues, cas zero au lieu du cas un.
    //
    // Rien a voir avec le seuil de place : celui-ci demande "y en a-t-il
    // assez ?", celui-la constate "il n'y a rien du tout". Une impasse de
    // cinquante cases est vivante ici et morte pour le seuil ; une case cernee
    // de blocs est morte ici quel que soit l'objectif.
    void marquerMortes();
    // Un octet par case, un bit par ESens : 1 = le flux qui entre par ce cote
    // n'a aucune suite. Rempli par marquerMortes a chaque marquage.
    QVector<unsigned char> mortes;
    bool calculerMortes = false;
    // Un cote peut-il alimenter cette case ? Faux hors grille, sur un bloc ou
    // sur une bombe : aucun flux n'en sortira jamais.
    bool coteAlimentable(int col, int row, ESens cote) const;
    // `pose` route-t-il EXACTEMENT comme `voulu`, par toutes les entrees que
    // `voulu` accepte ? La croix traverse tout droit, elle est donc le meme
    // chemin que le tuyau droit de son axe -- deux types, un seul routage. La
    // regle est derivee, pas ecrite en dur : elle vaut pour tout type qui
    // couvrirait les entrees d'un autre sans en devier le flux.
    static bool memeRoutage(const ETypePiece& pose, const ETypePiece& voulu);
    // Empreinte des types poses sur le plateau : sert a ne refaire le marquage
    // que quand quelque chose a bouge.
    // Retire du tas les cases qu'une explosion a videes : la marque ne doit pas
    // survivre a la piece. Appelee des que le plateau bouge.
    void oublierTasDetruit();
    quint32 signaturePlateau() const;
    // Recolle les circuits courts du plan en circuits longs. Voir le
    // commentaire dans construirePlan : c'est la longueur, pas la cloture, qui
    // empeche le plan de devenir un piege pour le trace.
    void fusionnerCircuits(QVector<unsigned char> &ouvertures,
                           const QVector<unsigned char> &mort);
    // Cases bloquees du plateau courant. Sert au compte-rendu du plan.
    int nbBloquees() const;
    // A appeler quand il n'y a plus de coup utile : leve veutFoncer().
    void demanderFoncer();
    // Nombre de types compatibles avec `entree` qui, poses sur cette tete, ne
    // la condamnent pas (0 a 4). Le tirage etant uniforme sur 7 types,
    // ouvertureLocale/7 est la probabilite que la prochaine piece piochee soit
    // posable directement -- une mesure de "a quel point cette tete est ouverte".
    int ouvertureLocale(int col, int row, ESens entree) const;
    // Reprendre cette case CHANGERAIT-IL quelque chose ? Vrai s'il existe un
    // type compatible avec `entree`, qui ne condamne pas, et qui envoie le flux
    // AILLEURS que la piece deja posee. Reserve aux cases du tas qu'on envisage
    // de remplacer : la question n'y est pas "reste-t-il une issue" -- la piece
    // en place en est une -- mais "une AUTRE issue". Voir reculerSurUnRebut.
    bool issueNouvelle(int col, int row, ESens entree) const;
    // Croiser son PROPRE tuyau. Vrai quand cette case porte un horizontal ou un
    // vertical du TRACE (pas du tas), que le fluide n'a pas encore rempli, et
    // que le trace l'aborde par un cote perpendiculaire a son axe : une croix y
    // passe sans toucher a l'axe deja pose, donc sans defaire ce qui est
    // construit. C'est le seul remplacement de ce genre qui soit sur.
    //
    // Pure geometrie : ne dit pas si le coup est OPPORTUN. Voir tete() pour la
    // condition de la croix en main, et espaceApres pour ce qu'on n'en fait
    // surtout pas.
    bool croisable(int col, int row, ESens entree) const;
    // Une croix quelque part dans la file ? Un croisement ne s'ouvre qu'a cette
    // condition : c'est le seul type qui s'y pose, et l'attendre sans l'avoir
    // coute la manche.
    bool croixEnFile() const;
    // Nombre de cases vides (tpNone) -- et, si `inclureTas` est vrai, du tas
    // aussi -- atteignables en 4-connexite depuis cette case, elle comprise si
    // elle compte. inclureTas vaut vrai par defaut : une case du tas n'est pas
    // un mur pour cette mesure, reprenable ou deja raccordee elle n'est pas de
    // la place perdue -- meme convention que meneALaMort et Bot::tete(), qui
    // traverseront vraiment ces cases le moment venu.
    //
    // A false, la mesure devient celle du vrai vide restant : c'est ce qu'il
    // faut pour verifier qu'un reseau parallele en construction (Bot::tas) a
    // encore ou grandir. Compter le tas comme franchissable masquerait le
    // retrecissement -- chaque case qu'on lui ajoute compterait encore comme
    // de la place, et le reseau ne verrait le mur qu'au coup qui s'y cogne.
    //
    // Proxy grossier de la place disponible dans les deux cas -- ne verifie
    // pas qu'un trace la remplit vraiment, juste qu'il y a assez de matiere.
    // S'arrete des que le compte atteint `maxi` (0 ou moins = sans borne) :
    // sert a demander seulement "au moins N cases ?" sans explorer toute la
    // poche.
    int tailleRegionVide(int col, int row, int maxi = 0, bool inclureTas = true) const;
    // Ce que `type` pose sur cette tete laisse comme suite : on part de la case
    // ou debouche son unique sortie et on avance de case en case. Une case vide
    // ou du tas se propage aux quatre cotes -- on y mettra la piece qu'on veut ;
    // une croix deja posee ne se traverse que tout droit, chacun de ses deux
    // axes comptant pour une traversee ; tout le reste (mur, bloc, piece
    // etrangere) arrete le parcours. Meme convention que meneALaMort, dont
    // c'est le prolongement au-dela de la premiere case -- a ceci pres qu'un
    // type qui ne se raccorde pas a `entree` est rejete ici au lieu d'etre
    // suppose absent : renvoie 0, comme une pose qui bute sur un mur.
    //
    // Renvoie le nombre de traversees ainsi atteignables -- un majorant de ce
    // que le trace peut encore parcourir, la 4-connexite ne garantissant pas
    // qu'un vrai tuyau remplisse la poche. Un compte inferieur a l'objectif
    // restant condamne donc la manche a coup sur, l'inverse ne la sauve pas.
    //
    // S'arrete des que le compte atteint `maxi` (0 ou moins = sans borne) : on
    // demande le plus souvent "au moins N ?", pas la mesure exacte. Remplit
    // `cases` si non nul (un octet par case, 1 = atteignable) -- l'overlay du
    // mode pas a pas veut la forme de la region, pas seulement sa taille ; la
    // borne `maxi` la tronque alors comme elle tronque le compte.
    // `reservees` : des cases que le comptage doit tenir pour deja prises,
    // meme vides. Ce sont les trous de la chaine projetee -- le flux y passera,
    // elles ne sont donc pas de la place disponible. Sans elles, la meme place
    // etait comptee deux fois : une fois comme chemin a venir, une fois comme
    // reserve pour la suite.
    // `sansRepli` coupe le rattrapage : quand la marche bute, elle ne revient
    // pas reprendre un rebut deja traverse pour repartir de la. C'est ce qu'il
    // faut pour juger une pose ANTICIPEE -- le repli y signifie "ce coup est
    // bon puisque je pourrai revenir defaire ce que je viens de poser", ce qui
    // n'est pas un sauvetage mais un aveu, et se paie 25 points pour revenir au
    // point de depart. La tete, elle, garde son repli : une fois engage, s'en
    // sortir en reprenant un rebut est legitime.
    int espaceApres(const ETypePiece& type, int col, int row, ESens entree,
                    int maxi = 0, QVector<unsigned char> *cases = nullptr,
                    const QVector<int> *reservees = nullptr,
                    bool sansRepli = false) const;
    // Vrai quand aucune piece ne peut prolonger la tete (ouvertureLocale == 0) :
    // attendre une meilleure pioche ne sert plus a rien, la manche est jouee ici.
    bool teteCondamnee(int col, int row, ESens entree) const;
    // Ce qu'il reste a construire pour boucler l'objectif : la longueur exigee
    // de la manche moins le tuyau deja raccorde au reservoir. Zero une fois
    // l'objectif acquis -- il n'y a alors plus rien a securiser, le bonus se
    // construit sans filet.
    //
    // Zero AUSSI quand la manche est perdue d'avance (rienAPerdre) : l'objectif
    // n'est pas une contrainte a respecter mais une contrainte a satisfaire, et
    // une contrainte insatisfiable ne se respecte pas -- elle paralyse. Voir
    // rienAPerdre pour ce que ca change en aval, qui est beaucoup.
    int objectifRestant() const;
    // La manche est-elle perdue d'avance ? Faux partout par defaut : aucun bot
    // ne sait le dire avant de l'avoir jouee. Celui qui planifie son trace, si
    // -- des le premier battement, et sans appel (voir BotTrace).
    //
    // Ce que ca change : TOUTE la securite de placement passe par
    // objectifRestant, qui la rend a zero. culDeSac retombe sur le seul "ne pas
    // mourir sur le coup", placeExigee n'est plus consultee, chaineViable ne
    // demande plus qu'une case, marquerObligations revient a ISSUE_MINIMALE. Le
    // bot cesse de se reserver de la place pour un objectif qu'il n'atteindra
    // pas, et va chercher des traversees -- 50 points chacune -- avec
    // l'anticipation pour seule boussole.
    //
    // La raison est du user, et elle est courte : "il n'a plus rien a perdre,
    // soit il score, soit il a deja perdu et il joue pour la gloire". Le
    // contraire etait le pire des deux mondes -- un bot qui garde une reserve
    // pour une victoire impossible defausse au lieu de poser, et finit la
    // manche perdue ET sans points.
    //
    // "Deja perdu" est mesure, et sans nuance : sur 25 parties entieres
    // (2026-09-19), une manche dont le trace planifie est plus court que
    // l'objectif au depart du flux est perdue 228 fois sur 228. Quand le
    // trace suffit, la meme manche est gagnee 829 fois sur 865. Il n'y a
    // donc rien a sacrifier en levant le filet : il n'y a plus de manche a
    // sauver, seulement des traversees a encaisser.
    virtual bool rienAPerdre() const;
    // L'objectif est-il DANS LA POCHE ? La question brute, celle que
    // objectifRestant ne repond plus depuis qu'il se tait aussi pour les
    // manches perdues : les deux cas valent zero et n'appellent pas le meme
    // bot (voir BotTrace::jouer).
    bool objectifAcquis() const;
    // Vrai si poser `type` sur cette tete engage le trace dans un cul-de-sac :
    // la place encore atteignable derriere la pose ne suffit plus a boucler
    // l'objectif. C'est meneALaMort prolonge au-dela de la premiere case, et il
    // l'englobe -- une pose mortelle ne laisse rien derriere elle.
    //
    // Le refus est sur : espaceApres majore toujours ce qu'un vrai tuyau peut
    // parcourir, donc une region trop petite condamne la manche pour de bon, et
    // ce filtre ne rejette jamais une pose gagnante. La reciproque est fausse --
    // de la place ne fait pas un trace -- donc il laisse passer des poses qui se
    // condamneront plus loin. Il elimine les impasses franches, rien de plus.
    bool culDeSac(const ETypePiece& type, int col, int row, ESens entree) const;
    // La place qu'un coup doit laisser derriere lui : le reste de l'objectif,
    // borne par ce que le plateau offre encore. Le corps de culDeSac, extrait
    // pour que le marquage des obligations exige la meme chose que la pose.
    // Suppose objectifRestant() > 0.
    int placeExigee() const;
    // Le pendant strict d'ouvertureLocale : combien de types compatibles avec
    // `entree` passent le filtre complet. Zero veut dire qu'aucune pioche ne
    // sauvera cette tete -- attendre mieux ne ferait alors que depenser la
    // manche en defausses, et il faut se rabattre sur le critere lache.
    int ouvertureUtile(int col, int row, ESens entree) const;
    // Plus rien a esperer sur cette tete : pose le haut de file s'il peut s'y
    // raccorder (une case traversee de plus vaut mieux que rien), sinon le
    // defausse -- et fonce dans les deux cas.
    void abandonner(int col, int row, ESens entree);
    // Vrai quand le flux talonne la tete de construction : il ne reste plus que
    // `margeGestes` gestes environ avant qu'il ne l'atteigne, l'avance de tuyau
    // deja pose (casesEnAval) convertie en gestes par la duree de remplissage
    // et la cadence. Toujours faux tant que le flux n'est pas parti -- rien ne
    // presse encore. Un bot s'en sert pour savoir s'il a encore le luxe de
    // defausser une piece plutot que de la poser.
    bool acculeParLeFlux(float margeGestes) const;
    // Faut-il poser une bombe maintenant ? Appelee a chaque battement pendant
    // l'attente, stock non vide. Voir bot.cpp pour la regle de base, et
    // BotTrace pour celle qu'un trace planifie rend calculable.
    virtual bool bomberMaintenant() const;
    // La bombe de CET essai est-elle deja posee ? Une par essai, c'est la
    // regle de BOMBES.md -- et la perturbation d'un rejeu n'en demande pas
    // deux non plus.
    bool bombeDejaPosee() const;
    // Une bombe posee par le bot n'a pas encore saute. Tant qu'elle brule, le
    // plateau va changer : ce qu'on poserait maintenant sera peut-etre perime,
    // et surtout il ne faut pas finir la manche avant l'explosion.
    bool bombeEnAttente() const;
    // Essais du niveau courant, celui-ci compris : 1 au premier passage, 2 au
    // premier rejeu. C'est lui qui dit au bot qu'il rejoue -- et, depuis que le
    // rejeu est parfait, qu'il connait deja la file.
    int essaiDuNiveau() const;
    // La case a bomber : celle dont le souffle emporte le plus de blocs. Seul
    // critere de ciblage depuis la mesure du 2026-09-17 -- voir le commentaire
    // dans bot.cpp pour ce qu'il a remplace et ce que ca vaut.
    //
    // Virtuelle pour le bot qui planifie : lui sait ce qu'une bombe doit
    // acheter -- de la longueur de trace -- et peut donc l'essayer avant de la
    // poser, au lieu de viser un proxy. Voir BotTrace.
    virtual bool choisirPaquetDeBlocs(int &col, int &row) const;
    // Pose la bombe quand bomberMaintenant() le dit, et refait le plan quand
    // elle a saute. Appelee a chaque battement, avant jouer().
    void gererBombes();
    // Pose une bombe sur cette case et la retire du stock. Elle est perdue
    // pour de bon : ni rendue a l'explosion, ni au rejeu, ni si la manche finit
    // avant qu'elle ait saute. False si le stock est vide, si la manche est
    // finie, ou si la case n'est pas TOTALEMENT vide. Voir BOMBES.md.
    bool poserBombe(int col, int row);
    // Pose la piece du haut de la file sur cette case precise et l'inscrit au
    // tas. Renvoie false si la pose est refusee. Sert a placer d'avance une
    // piece sur une case que le trace n'a pas encore atteinte.
    // `anticipee` distingue les deux origines dans le tas.
    // `origine` : 1 = defausse selon le plan, 2 = pre-pose de l'anticipation,
    // 3 = defausse posee sur un pari de rang 2.
    bool poserCaseTas(int col, int row, unsigned char origine = 1);
    // Case ou la defausse pose AVANT TOUT : le rang 2 du trajet anticipe -- la
    // tete, puis le pont, puis le rang 1 qu'on suppose, puis elle. -1 quand le
    // bot n'anticipe pas. CONSOMMEE par defausser() : un ancrage ne vaut que
    // pour le geste qui l'a calcule, la projection etant refaite a chaque fois.
    int ancrageDefausse = -1;
    // Sens par lequel le flux entrerait dans `ancrageDefausse`. C'est lui qui
    // decide si la piece a defausser y a sa place : elle doit s'y raccorder.
    ESens ancrageEntree = sHaut;
    // Les pieces que la projection a supposees EN AMONT du pari et que le
    // plateau n'a pas encore : le pont pose sur la tete -- pris dans la file,
    // ou impose quand la tete est obligee -- et le type qu'on imagine matche
    // sur le rang 1, celui-la meme dont le pari depend. Chaque entree est
    // (index de case, type suppose). Consommees avec l'ancrage.
    QVector<QPair<int, ETypePiece>> ancrageAmont;
    // Tout le trajet de la tete au pari, cases deja posees comprises. Le flux
    // y sera passe avant d'atteindre le pari : ce n'est pas de la place que le
    // pari puisse compter derriere lui. Consomme avec l'ancrage.
    QVector<int> ancrageChaine;
    // "Mene a la mort" pour le pari, mesure sur le plateau que le pari aura
    // DEVANT LUI et non sur celui d'aujourd'hui : l'amont suppose est pose le
    // temps du test, et le trajet tenu pour pris. Voir bot.cpp.
    // Non-const : elle pose l'amont sur le plateau et lui retire sa marque de
    // tas le temps de la mesure, puis remet tout en place.
    bool pariCondamne(const ETypePiece& type, int col, int row, ESens entree,
                      const QVector<QPair<int, ETypePiece>> &amont,
                      const QVector<int> &chaine);
    // Remonte le trace depuis le reservoir et renvoie la case ou construire :
    // la premiere case vide, ou une case du tas qui ne raccorde pas (le bot la
    // reprendra). false si le trace bute sur un vrai obstacle (mur, bloc, piece
    // etrangere) ou se referme en boucle. Convertit au passage toute case du
    // tas qui raccorde : raccordee au reservoir, elle sert le flux, ce n'est
    // plus un rebut. A preferer a Ecoulement::tete() pour un bot, qui lui
    // s'arrete sur toute case du tas comme sur un obstacle.
    bool tete(int& col, int& row, ESens& entree);
    // Repli de tete() quand le trace bute : rend le rebut traverse le plus
    // profond qui ait encore une issue, pour repartir de la plutot que de
    // declarer la manche perdue. Voir le commentaire dans bot.cpp.
    bool reculerSurUnRebut(const QVector<int> &reprises,
                           const QVector<unsigned char> &entrees,
                           int &col, int &row, ESens &entree);

    Partie *p;
    float cadence;
    quint32 seed;
    float jetons = 0.0f;
private:
    // Une case par octet, indexee row * largeur + col : 0 = rien, 1 = defausse
    // selon le plan, 2 = pre-pose de l'anticipation, 3 = defausse sur un pari. Remise a zero des que la graine de plateau change,
    // c.-a-d. a chaque nouvelle manche (niveau suivant comme partie perdue).
    QVector<unsigned char> tas;
    // Une case par octet : le type (ETypePiece) que le plan de defausse veut
    // la, tpNone la ou il ne dit rien. Refait a chaque manche.
    QVector<unsigned char> plan;
    // Une case par octet : 1 la ou le plan ne fait qu'enteriner une obligation.
    // Entree imposee par la propagation avant, quand elle est connue : 0 si la
    // case n'est pas sur le couloir issu de la tete, sinon 1 + ESens. Refait a
    // chaque marquage, comme `oblige`.
    QVector<unsigned char> entreeTete;
    // Refait des que le plateau bouge -- voir planObligatoire.
    QVector<unsigned char> oblige;

    // Etat du plateau au dernier marquage, pour ne pas le refaire pour rien.
    quint32 signatureVue = 0;
    int mancheVue;
    int niveauVu;
    // Essais du niveau courant, celui-ci compris : 1 au premier passage, 2 au
    // premier rejeu. C'est lui qui dit au bot qu'il rejoue.
    int essaisNiveau = 1;
    // La bombe de cet essai est posee. Une par essai (BOMBES.md §C) : le rejeu
    // suivant en reposera une, sur le terrain que celle-ci aura deja ouvert.
    bool bombeCetEssai = false;
    // Case de la bombe tant qu'elle n'a pas saute, -1 sinon. Des qu'elle saute,
    // le plan de defausse est perime : il a ete calcule sur des blocs qui
    // n'existent plus.
    int bombeEnVol = -1;
    // Barre d'espace demandee. Comme pour le joueur, sans retour : ne retombe
    // qu'a la manche suivante, quand la graine de plateau change.
    bool fonce = false;
    // Calcul hors thread autorise. Voir setPlanificationAsynchrone.
    bool planAsynchrone = false;
    // --- le rejeu qui tourne en rond (surveillerRejeu) --------------------
    //
    // Rejeux consecutifs joues sans espoir, et le drapeau qui garantit qu'une
    // manche n'est jugee qu'une fois -- la fin de manche dure plusieurs
    // battements.
    int rejeuxSansEspoir = 0;
    bool mancheJugee = false;
    int rejeuxAvantAbandon = 2;
};

#endif // BOT_H
