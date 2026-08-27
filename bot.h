#ifndef BOT_H
#define BOT_H

#include <QVector>
#include "partie.h"

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
    // type suppose. Le plan la reclamait de toute facon, donc un pari perdu
    // reste une defausse ordinaire -- mais on aime les voir.
    bool estPari(int col, int row) const;

    // Vrai si le tas de defausse occupe cette case. N'a d'interet que pour les
    // bots : sert a l'overlay qui montre ou le bot jette ce qu'il ne peut pas
    // poser sur la tete de construction.
    bool estTas(int col, int row) const;

    // Vrai si poser une piece de ce type sur la tete (col, row, entree)
    // condamne la manche : son unique sortie -- la croix va tout droit -- bute
    // sur un mur, une case bloquee, ou une piece deja posee qu'on ne pourra ni
    // traverser (seule la croix se laisse traverser) ni reprendre (seules les
    // pieces du tas se reprennent). Le type est suppose compatible avec
    // `entree`, tel que le renvoie Ecoulement::piecesCompatibles().
    bool meneALaMort(const ETypePiece& type, int col, int row, ESens entree) const;

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
    void construirePlan();
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
    int espaceApres(const ETypePiece& type, int col, int row, ESens entree,
                    int maxi = 0, QVector<unsigned char> *cases = nullptr,
                    const QVector<int> *reservees = nullptr) const;
    // Vrai quand aucune piece ne peut prolonger la tete (ouvertureLocale == 0) :
    // attendre une meilleure pioche ne sert plus a rien, la manche est jouee ici.
    bool teteCondamnee(int col, int row, ESens entree) const;
    // Ce qu'il reste a construire pour boucler l'objectif : la longueur exigee
    // de la manche moins le tuyau deja raccorde au reservoir. Zero une fois
    // l'objectif acquis -- il n'y a alors plus rien a securiser, le bonus se
    // construit sans filet.
    int objectifRestant() const;
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
    // Pose la piece du haut de la file sur cette case precise et l'inscrit au
    // tas. Renvoie false si la pose est refusee. Sert a placer d'avance une
    // piece sur une case que le trace n'a pas encore atteinte.
    // `anticipee` distingue les deux origines dans le tas.
    // `origine` : 1 = defausse selon le plan, 2 = pre-pose de l'anticipation,
    // 3 = defausse posee sur un pari de rang 2.
    bool poserCaseTas(int col, int row, unsigned char origine = 1);
    // Case ou la defausse pose en priorite : le rang 2 du trajet anticipe,
    // quand le bot en connait un. Elle sert deux fois -- comme premier choix,
    // et comme point d'ancrage tant qu'aucun tas n'existe encore, au premier
    // geste d'une manche. -1 quand le bot n'anticipe pas.
    int ancrageDefausse = -1;
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
    quint32 grainePlateauVue;
    // Barre d'espace demandee. Comme pour le joueur, sans retour : ne retombe
    // qu'a la manche suivante, quand la graine de plateau change.
    bool fonce = false;
};

#endif // BOT_H
