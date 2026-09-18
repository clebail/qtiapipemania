#include <thread>

#include <QElapsedTimer>

#include <QDebug>

#include "bottrace.h"

// Gestes de marge sur l'echeance de chaque rang. Voir planifier().
#define MARGE_ECHEANCE 4

// Noeuds accordes a CHAQUE emplacement essaye par l'etude de bombe. Le
// balayage est complet -- 130 a 150 emplacements sur un plateau charge -- donc
// c'est ce nombre qui fixe le prix de l'etude entiere.
//
// Balaye le 2026-09-19 sur `--graine 2998034427`, niveaux 31, 33 et 34, en
// jugeant le gagnant de chaque budget a plein budget :
//
//   budget    n31    n33    n34    balayage complet
//   400000    130    135    108    24 / 19 / 157 s
//   100000    130    135    108     6 /  7 /  40 s
//    25000    130    135    102     1,7 / 2,4 / 10,6 s   <-
//     6000    128     87    100     0,9 / 1,3 /  3,4 s
//
// A 25 000 le classement tient encore -- l'optimum retrouve deux fois sur
// trois, et 102 contre 75 la ou il le rate -- et le balayage redevient
// payable. A 6 000 il decroche (87 au lieu de 135).
#define BUDGET_ETUDE 25000

// Paliers de visee au plus, quand l'objectif est hors d'atteinte. Six suffisent
// a cerner la longueur atteignable a deux cases pres sur un objectif de 150, et
// chacun ne coute qu'un quart de budget. Voir chercherAuMieux.
#define PALIERS_VISEE 6

// Ce que l'etude de bombe demande a chaque candidat : un progres FRANC sur le
// trace en place, pas l'objectif. Demander l'objectif quand il est hors
// d'atteinte fait rendre UNE case a tous les candidats -- ils deviennent alors
// indiscernables, et le ciblage redevient un tirage au sort. Voir
// chercherAuMieux pour le mecanisme.
#define MARGE_ETUDE 20

// Et l'echeance de l'etude entiere, en millisecondes de montre. Le balayage
// complet coute 1,7 s sur un plateau facile et 10,6 s sur un plateau charge,
// alors que la fenetre n'a que les 22 s du delai de depart -- dont le bot a
// besoin pour poser ses pieces. Passe ce delai, on joue le meilleur
// emplacement vu jusque-la. Le user a tranche le compromis : "si 2 ou 3
// secondes de traitement sont necessaires, ca passera".
//
// Sans echeance, le stock restait intact : l'etude rendait sa reponse apres la
// fin de `epAttente`, et `gererBombes` n'a plus le droit de poser.
#define DELAI_ETUDE_MS 3000

namespace {

// Le souffle sur une copie : 3x3, le reservoir immunise (Minage::exploser).
// Rend le nombre de blocs emportes -- zero veut dire que l'emplacement
// n'ouvre rien, donc qu'il ne change rien.
int souffler(Game &g, int col, int row) {
    int emportes = 0;

    for(int dy = -1; dy <= 1; dy++) {
        for(int dx = -1; dx <= 1; dx++) {
            int nx = col + dx, ny = row + dy;

            if(nx < 0 || nx >= g.getLargeur() || ny < 0 || ny >= g.getHauteur()) {
                continue;
            }

            if(g.getTypePiece(nx, ny) == tpBloque) {
                g.setTypePiece(nx, ny, tpNone);
                emportes++;
            }
        }
    }

    return emportes;
}

// LE PLUS LONG TRACE QUE LA RECHERCHE SACHE RENDRE.
//
// `Trace::calculer` elague chaque candidate par la place restante : depuis
// cette case, le terrain encore atteignable doit valoir au moins ce qu'il
// reste a parcourir. La regle est juste -- ne pas s'engager dans une poche
// trop petite -- mais elle est BINAIRE, et elle se referme d'un coup quand la
// visee approche la taille de la poche : plus une seule candidate ne passe, et
// la recherche rend la case de depart.
//
// Mesure du 2026-09-19, `--graine 2998034427 --niveau 36`, poche de 155 cases,
// objectif 150, meme budget a chaque ligne :
//
//   on demande   20   60   100   110   120   130   140   145   150
//   elle rend    20   60   100   107   117   111    95    49     1
//
// Plus on demande, moins on obtient -- et a l'objectif exact, une case en zero
// milliseconde. Ce n'est pas un probleme de temps de calcul : soixante-quatre
// fois le budget (25 600 000 noeuds) ne change pas un chiffre de cette ligne.
// C'est le mur du niveau 39 de TRACE.md §4, et le palier 3 du §6 ter -- "jouer
// le plus long trace trouve" -- qui ne s'est jamais declenche faute de plus
// long trace.
//
// On demande donc ce qu'on peut obtenir : l'objectif d'abord, puis, s'il
// echoue, une visee descendante par dichotomie, en gardant le MEILLEUR trace
// rencontre. Noter que le meilleur ne vient pas forcement de la plus haute
// visee qui reussit -- dans la table ci-dessus, 117 sort d'une visee de 120
// qui a echoue. D'ou le suivi separe.
Trace chercherAuMieux(const Game &vierge, int objectif, long budget,
                      const QVector<ETypePiece> &enMain,
                      int avantDepart, int parCase) {
    Trace meilleur;

    meilleur.calculer(&vierge, objectif, budget, enMain, avantDepart, parCase);

    if(meilleur.longueur() >= objectif) {
        return meilleur;
    }

    // Les passes de rattrapage ont un budget REDUIT, comme la dichotomie de la
    // garde dans Trace::calculer et pour la meme raison : le pire cas se
    // paierait sinon six recherches pleines.
    int bas = 1;
    int haut = objectif - 1;

    for(int pas = 0; pas < PALIERS_VISEE && bas < haut; pas++) {
        int milieu = (bas + haut + 1) / 2;
        Trace essai;

        essai.calculer(&vierge, milieu, budget / 4, enMain, avantDepart, parCase);

        if(essai.longueur() >= milieu) {
            bas = milieu;
        } else {
            haut = milieu - 1;
        }

        if(essai.longueur() > meilleur.longueur()) {
            meilleur = essai;
        }
    }

    // ET ON REDEMANDE CE QU'ON VIENT DE TROUVER, a plein budget.
    //
    // C'est le geste qui rend la GARDE -- les rangs de tete que le
    // planificateur garantit avec la file connue. Une recherche lancee sur une
    // visee INATTEIGNABLE rend un long trace, mais avec garde 0 : la
    // dichotomie de la garde, dans Trace::calculer, echoue sur chacune de ses
    // passes contraintes puisque la visee ne tombera jamais. Seule la passe
    // libre aboutit, et elle ne garantit rien.
    //
    // La meme longueur, demandee comme visee atteignable, sort avec sa garde.
    // Mesure du 2026-09-19, `--graine 1188181038 --niveau 38` apres la bombe :
    // visee 158 (l'objectif) rend 130 avec garde 0, visee 130 rend 130 avec
    // garde 20. Et la garde decide de tout -- 17 traversees dans le premier
    // cas, 135 dans le second, pour un trace de meme longueur. C'est le debut
    // du trace qui fait le prefixe contigu, donc ce que le flux parcourt.
    Trace finale;

    finale.calculer(&vierge, qMax(bas, meilleur.longueur()), budget, enMain,
                    avantDepart, parCase);

    // A longueur egale, celui qui garantit le plus de rangs.
    if(finale.longueur() > meilleur.longueur()
       || (finale.longueur() == meilleur.longueur()
           && finale.garde() > meilleur.garde())) {
        meilleur = finale;
    }

    // ET LA REECRITURE LOCALE POUR FINIR. Elle ramasse des cases libres que le
    // tracage global avait laissees de cote, et pose des croix la ou le trace
    // peut se recroiser -- une case traversee deux fois compte double. Elle ne
    // touche pas au prefixe garanti (voir Trace::ameliorer), donc la garde
    // reste ce qu'elle etait.
    //
    // Mesure du 2026-09-19, 8 graines par niveau, 36 a 44 : +2,5 passes en
    // moyenne, 1 a 2 croix, moins d'une milliseconde, zero trace illegal sur
    // 40. C'est peu au regard du prototype (+12), et la raison est une bonne
    // nouvelle : le prefixe garanti fait desormais 62 a 86 rangs sur des
    // traces d'une centaine, il ne reste donc pas grand-chose a reecrire. Ce
    // qu'on a gagne en garantie, on ne l'a plus a gagner ici.
    meilleur.ameliorer(&vierge);

    return meilleur;
}

}   // namespace

BotTrace::BotTrace(Partie *p, float cadence, quint32 seed) : BotMemoire(p, cadence, seed) {
    // Le plan est refait ici, une seconde fois. Bot::Bot l'a deja construit,
    // mais depuis un constructeur l'appel virtuel ne descend pas jusqu'a nous :
    // ce plan-la pave donc le plateau entier, trace compris. On le refait avec
    // le masquage, et c'est quelques dizaines de millisecondes payees une fois
    // par installation du bot -- pas une fois par manche.
    construirePlan();
}

const Trace *BotTrace::tracePlanifie() const {
    return &trace;
}

// Le plateau est STATIQUE pendant la manche : blocs tires a nouvelleManche,
// reservoir fixe, et le trace ne depend d'aucune information que la file
// apporterait -- une pose n'a lieu que quand la piece en main est exactement le
// type que la case reclame. Il n'y a donc aucune raison de replanifier.
//
// Sauf une, et c'est le seul evenement qui perime un trace : l'EXPLOSION. Elle
// ouvre du terrain, donc elle change le plateau sur lequel le chemin a ete
// cherche. Les deux points d'appel de construirePlan sont exactement ceux-la --
// changement de manche, et bombe qui vient de sauter -- d'ou la planification
// ici plutot que dans un declencheur de plus a tenir en phase. Voir TRACE.md, §5.
void BotTrace::construirePlan() {
    planifier();

    // En asynchrone, le trace n'est pas encore la : le pavage doit attendre,
    // c'est recolterPlan qui l'enchainera. Le plan en place reste celui de la
    // manche d'avant pendant ce temps, et personne ne s'en sert -- le bot ne
    // joue pas tant que la planification vole.
    if(!planEnVol()) {
        Bot::construirePlan();
    }
}

bool BotTrace::traceTropCourt() const {
    return trace.longueur() < p->longueurMinimale();
}

bool BotTrace::mancheImprenable() const {
    // Pas de verdict sur un trace perime : tant que la planification vole, le
    // trace en place est celui d'avant l'explosion, et c'est justement pour le
    // rallonger qu'on a bombe. Declarer la manche perdue la-dessus ferait
    // basculer le bot au v4 -- sans filet, depuis rienAPerdre -- une demi-
    // seconde avant d'apprendre qu'elle ne l'est plus.
    return traceTropCourt() && !bombeEnAttente() && !planEnVol()
           && (p->bombes() <= 0 || p->etat() != epAttente);
}

bool BotTrace::rienAPerdre() const {
    return mancheImprenable();
}

bool BotTrace::niveauSansEspoir() const {
    return traceTropCourt() && p->bombes() <= 0 && !bombeAVenir();
}

bool BotTrace::bomberMaintenant() const {
    // Une seule en vol a la fois : on ne sait pas, avant de replanifier, si la
    // suivante sera utile -- et deux bombes voisines s'annulent.
    //
    // Et rien tant que la planification vole : `traceTropCourt` lirait le
    // trace d'AVANT l'explosion, donc le bot poserait une bombe de plus pour
    // un terrain que la precedente vient peut-etre de degager.
    if(bombeEnAttente() || planEnVol()) {
        return false;
    }

    // TRACE TROP COURT : on rallonge. Autant de bombes qu'il en faut, une a
    // la fois, la boucle s'arretant d'elle-meme des que l'objectif redevient
    // atteignable.
    if(traceTropCourt()) {
        return true;
    }

    // TRACE VALIDE ET MANCHE PERDUE QUAND MEME : la bombe de perturbation.
    //
    // Ce bot avait desactive la regle de base -- une bombe par essai des le
    // premier rejeu -- au motif qu'il savait, lui, quand une manche etait
    // perdue d'avance, et qu'il valait donc mieux garder le stock pour
    // rallonger un trace trop court. Le raisonnement laissait un trou, et le
    // user l'a vu a l'ecran (`--graine 2998034427 --niveau 30 --vies 8
    // --bombes 10`) : trace 126/126, donc aucune bombe posee, et le bot meurt
    // a 83 traversees, huit fois de suite, avec dix bombes en stock.
    //
    // Pourquoi c'est sans appel. Quand le trace est valide, le bot ne decide
    // plus rien : il sert la case la plus en amont et defausse le reste. Ni
    // veto ni pont n'interviennent -- BotTrace::jouer ne passe pas par la. Le
    // rejeu etant parfait, plateau et file compris, la manche rejouee est la
    // COPIE EXACTE de celle qu'on vient de perdre. Attendre un meilleur
    // tirage n'a aucun sens : il n'y a pas de tirage, il y a le meme.
    //
    // Le seul levier qui reste est le TERRAIN, et la bombe est le seul outil
    // qui y touche. Elle ouvre des cases, la replanification refait le chemin
    // dessus, et le rejeu cesse d'etre une copie. C'est le "redemarrage
    // perturbe" de TRACE.md §7 -- et la perturbation est deterministe, donc
    // compatible avec le rejeu.
    //
    // UN REJEU, ET RIEN D'AUTRE. Pas au premier essai : on laisse d'abord sa
    // chance au trace, c'est tout l'objet de la regle -- on ne bombe que
    // quand il a echoue pour de bon. C'est aussi pourquoi on ne reprend pas
    // Bot::bomberMaintenant telle quelle : elle bombe des le premier essai
    // quand le stock est plein (la onzieme bombe serait perdue), et ce motif
    // de comptable ferait sauter une bombe sur un trace qui allait peut-etre
    // au bout.
    //
    // Une seule par essai : elle change le terrain, donc le trace, donc la
    // manche. Si ca ne suffit pas, le rejeu suivant en remettra une -- et le
    // terrain qu'elle a ouvert est acquis, les rejeux cumulent.
    return essaiDuNiveau() >= 2 && !bombeDejaPosee();
}

// Releve de la file, un cran a la fois. On ne peut pas simplement lire le haut
// de pile a chaque geste : deux pieces identiques d'affilee seraient
// indiscernables. On regarde donc la FENETRE des cinq visibles et on detecte
// qu'elle a glisse d'un cran, ce qui est la seule chose qu'un geste puisse lui
// faire faire.
void BotTrace::noterFile() {
    QVector<ETypePiece> vue;

    for(int i = 0; i < p->file()->getTaille(); i++) {
        vue << p->file()->getPiece(i).type;
    }

    if(vue.isEmpty()) {
        return;
    }

    if(sequence.isEmpty()) {
        sequence = vue;
        consommees = 0;
        return;
    }

    // La fenetre du battement precedent occupe la fin de `sequence` des lors
    // qu'on a tout note. Elle a glisse si les quatre premieres cases d'
    // aujourd'hui sont les quatre dernieres d'hier.
    int fin = consommees + vue.size();

    if(fin > sequence.size()) {
        return;
    }

    bool glisse = true;

    for(int i = 0; i + 1 < vue.size() && glisse; i++) {
        glisse = vue.at(i) == sequence.at(consommees + i + 1);
    }

    if(!glisse) {
        return;
    }

    consommees++;

    // Au premier essai on decouvre la suite ; aux suivants on la connait deja,
    // et la reecrire n'apporterait rien -- le rejeu est parfait.
    if(consommees + vue.size() > sequence.size()) {
        sequence << vue.last();
    }
}

QVector<ETypePiece> BotTrace::mainConnue() const {
    QVector<ETypePiece> visible;

    for(int i = 0; i < p->file()->getTaille(); i++) {
        visible << p->file()->getPiece(i).type;
    }

    if(essai < 2 || consommees >= sequence.size()) {
        return visible;
    }

    // TOUTE la suite connue, et plus seulement les pieces qui tombent avant le
    // depart du flux. C'est Trace qui sait desormais qu'une piece arrivee au
    // 190e tirage ne peut pas servir le rang 3 : chaque rang porte son echeance
    // (voir Trace::calculer). Tronquer ici reviendrait a jeter les pieces qui
    // servent justement les rangs lointains -- et c'est exactement la ou le bot
    // mourait.
    return sequence.mid(consommees);
}

void BotTrace::planifier() {
    // Frontieres. planifier() passe une fois par manche -- et une fois de plus
    // quand une bombe saute, sans que le numero de manche bouge, d'ou le test
    // sur lui et pas sur autre chose.
    int manche = p->numeroManche();

    // Meme manche : c'est une bombe qui vient de sauter, donc le terrain n'a
    // fait que s'ouvrir et le trace en place reste jouable. Voir
    // installerTrace.
    seulementSiPlusLong = manche == mancheSuivie;
    tracePrecedente = seulementSiPlusLong ? trace : Trace();

    if(manche != mancheSuivie) {
        mancheSuivie = manche;
        consommees = 0;

        if(p->niveau() != niveauSuivi) {
            niveauSuivi = p->niveau();
            essai = 0;
            sequence.clear();
        }

        // L'etude et son trace valaient pour la manche qui vient de finir.
        etude.reset();
        traceApresBombePrete = false;
        etudeFaite = false;

        essai++;
    }

    // Terrain nu : on ne garde que ce qui ne bougera pas de la manche. Trace
    // traite toute piece posee comme un obstacle, au meme titre qu'un bloc --
    // ce qui est juste quand on planifie au premier battement, et faux apres
    // une explosion, ou le tuyau deja pose est du trace SERVI et non un mur.
    // Le meme calcul que WGame::rafraichirTrace, donc ce que la case "Afficher
    // le trace" montre est bien le chemin que le bot suit.
    Game vierge = plateauNu();

    // LA BOMBE A DEJA REPONDU. Le trace qui suit son souffle a ete calcule
    // pendant l'etude, sur ce plateau-ci exactement : le souffle simule enleve
    // les memes blocs que le vrai, et le plateau nu ignore tout le reste. Il
    // n'y a donc rien a rechercher.
    if(traceApresBombePrete) {
        traceApresBombePrete = false;
        // Le terrain vient de changer : une etude qui avait conclu "rien ne
        // vaut le coup" ne vaut plus.
        etudeFaite = false;

        // Et on ne s'en contente QUE s'il vaut mieux que ce qu'on a. Le trace
        // de l'etude a ete calcule a budget reduit (BUDGET_ETUDE) : il sert a
        // classer les emplacements, pas forcement a jouer. S'il n'ameliore
        // pas, le terrain a quand meme change et merite une vraie recherche.
        if(traceApresBombe.longueur() > trace.longueur()) {
            // Meme traitement que les traces de chercherAuMieux : celui-ci
            // vient de l'etude, il n'est pas passe par la.
            traceApresBombe.ameliorer(&vierge);
            installerTrace(traceApresBombe);
            annoncerPlan();
            return;
        }
    }

    // Ce qu'on sait de la file : cinq pieces au premier essai, la sequence
    // entiere des le second. Le generateur en fait le debut du trace, et ces
    // cases-la sont servies sans rien attendre du tirage -- c'est la que le bot
    // mourait, les dix-huit trous fatals mesures etant tous dans les vingt
    // premiers rangs et dix-sept sur dix-huit des coudes.
    QVector<ETypePiece> enMain = mainConnue();

    // On vise l'objectif et rien de plus : la recherche s'arrete des qu'elle
    // l'atteint. Chercher le plus long chemin possible est un probleme bien
    // plus dur, le jeu ne demande que le second, et les succes deviennent
    // instantanes -- 13 ms en moyenne au niveau 35, 39 plateaux sur 40
    // (TRACE.md, §6 bis).
    // L'echeance des rangs. Le flux part dans `secondesAvantDepart` et avance
    // ensuite d'au plus une case par seconde (DUREE_MIN), donc le bot aura tire
    // `cadence x secondes` pieces au depart et `cadence` de plus par case
    // traversee. Minorer la duree de remplissage est du bon cote : on suppose
    // le flux au plus rapide, donc moins de pieces disponibles qu'en realite.
    //
    // MARGE_ECHEANCE, en revanche, corrige un optimisme qui a coute une manche.
    // La piece d'indice i n'est pas disponible a i/cadence mais a (i+1)/cadence
    // -- il faut un geste pour la consommer, et elle n'est posable qu'une fois
    // en tete de file. Et le flux ENTRE dans la case avant de l'avoir remplie.
    // Mesure du 18 septembre, graine 831056493 niveau 12 : le planificateur
    // affectait au rang 45 la piece 133 pour une echeance de 134, la garantie
    // tenait a un geste pres, et la manche etait perdue. Deux secondes de marge
    // ne coutent que quelques rangs de garde.
    int gestesAvantDepart = (int)(p->secondesAvantDepart() * cadence) - MARGE_ECHEANCE;
    int gestesParCase = (int)cadence;

    if(gestesAvantDepart < 1) {
        gestesAvantDepart = 1;
    }

    // Surtout PAS de plancher sur la taille de la main : un plancher a
    // `enMain.size()` rendrait toutes les pieces disponibles des le rang 0,
    // c'est-a-dire supprimerait l'echeance qu'on vient d'introduire. La garde
    // annoncait alors le trace entier et n'en garantissait rien -- mesure du
    // 18 septembre : trace 54/54 "garde 54", et le bot mourait au rang 44.
    int objectif = p->longueurMinimale();
    int avantDepart = essai >= 2 ? gestesAvantDepart : 0;
    int parCase = essai >= 2 ? gestesParCase : 0;

    piecesPlanifiees = enMain.size();

    if(!planificationAsynchrone()) {
        installerTrace(chercherAuMieux(vierge, objectif, 400000, enMain,
                                       avantDepart, parCase));
        annoncerPlan();
        return;
    }

    // Tout par COPIE, et rien du bot dans la capture : le thread ne voit ni le
    // plateau de la partie, ni la file, ni ce Bot -- seulement un Game nu et
    // une liste de types. C'est ce qui rend le calcul sur : il n'y a rien a
    // verrouiller parce qu'il n'y a rien de partage.
    //
    // Une planification chasse l'autre, et sans attendre : la manche peut
    // changer pendant qu'une recherche vole, et son resultat ne vaut alors
    // plus rien -- il a ete calcule sur le plateau d'avant. On lache la
    // reference, le thread garde la sienne, il finira pour personne.
    auto etat = std::make_shared<Planification>();

    calcul = etat;

    std::thread([etat, vierge, enMain, objectif, avantDepart, parCase]() {
        etat->trace = chercherAuMieux(vierge, objectif, 400000, enMain,
                                      avantDepart, parCase);
        etat->prete.store(true, std::memory_order_release);
    }).detach();
}

Game BotTrace::plateauNu() const {
    Game vierge(*p->plateau());

    for(int i = 0; i < vierge.getSize(); i++) {
        int col = i % vierge.getLargeur();
        int row = i / vierge.getLargeur();
        ETypePiece t = vierge.getTypePiece(col, row);

        if(t != tpBloque && t != tpReservoir) {
            vierge.setTypePiece(col, row, tpNone);
        }
    }

    return vierge;
}

// OU BOMBER. On essaie, au lieu de viser un proxy : chaque emplacement est
// souffle sur une copie, le trace recalcule, et on garde celui qui rend le
// plus long. Le resultat de la bombe est donc connu avant qu'elle soit posee.
//
// Deux conditions pour qu'un emplacement soit retenu, et elles different selon
// ce qu'on attend de la bombe :
//
//   - TRACE TROP COURT : il faut qu'elle ALLONGE. Un souffle qui rend moins
//     que ce qu'on a deja est une bombe jetee -- et c'etait le cas au niveau
//     34 de la graine du user, ou le critere blocs faisait passer de 85 a 75.
//   - PERTURBATION : le trace vaut deja l'objectif et on cherche a le CHANGER,
//     pas a l'allonger (la recherche s'arrete a l'objectif, il ne peut plus
//     grandir). La condition devient : que le trace d'apres vaille encore
//     l'objectif. Sans elle, la perturbation pourrait casser un plan qui
//     marchait.
//
// Aucun emplacement ne passe ? On ne bombe pas, et la bombe reste en stock
// pour le niveau suivant. C'est la monnaie du bot a ces niveaux-la.
bool BotTrace::choisirPaquetDeBlocs(int &col, int &row) const {
    int largeur = p->getLargeur();

    // L'etude est-elle rentree ?
    if(etude != nullptr && etude->prete.load(std::memory_order_acquire)) {
        int retenue = etude->caseBombe;

        if(retenue >= 0) {
            col = retenue % largeur;
            row = retenue / largeur;

            // LE TERRAIN A PU BOUGER pendant que l'etude tournait -- le bot a
            // continue de poser, et la case retenue n'est peut-etre plus
            // minable. On la jette et on recommence sur l'etat courant, plutot
            // que de la proposer a un poserBombe qui la refusera sans que
            // personne ne s'en apercoive.
            if(!p->peutMiner(col, row)) {
                etude.reset();
                qDebug() << "=== bombe === (" << col << "," << row
                         << ") n'est plus minable, on refait l'etude";
                lancerEtude(plateauNu(), trace.longueur(), traceTropCourt(),
                            traceTropCourt() && essai >= 2);

                return etude != nullptr
                       && etude->prete.load(std::memory_order_acquire)
                       ? choisirPaquetDeBlocs(col, row) : false;
            }

            traceApresBombe = etude->trace;
            traceApresBombePrete = true;
        }

        etude.reset();

        if(retenue < 0) {
            etudeFaite = true;
            qDebug() << "=== bombe === aucun emplacement ne vaut la bombe,"
                     << "on la garde pour le niveau suivant";
            return false;
        }

        // Le chiffre annonce est celui de l'ETUDE, calcule a budget reduit :
        // il sert a classer les emplacements. Quand il est inferieur au trace
        // en place, ca ne veut pas dire qu'on recule -- installerTrace refuse
        // tout trace plus court, et une vraie recherche suit le souffle. On
        // depense alors pour changer le terrain, pas pour ce chiffre-la.
        if(traceApresBombe.longueur() > trace.longueur()) {
            qDebug() << "=== bombe === etude : (" << col << "," << row
                     << ") fait passer le trace de" << trace.longueur()
                     << "a" << traceApresBombe.longueur();
        } else {
            qDebug() << "=== bombe === etude : (" << col << "," << row
                     << ") -- on depense pour ouvrir du terrain, le trace"
                     << trace.longueur() << "est garde";
        }

        // ET ON ENCHAINE, sur le terrain tel qu'il sera apres ce souffle-ci.
        // La meche brule 2,5 s : autant les passer a chercher la bombe
        // suivante plutot qu'a attendre. Si celle-ci suffit, l'etude sera
        // jetee -- un thread de plus, rien de casse.
        //
        // Sauf si c'est la DERNIERE : le stock sera vide, plus personne ne
        // viendra chercher le resultat, et ce serait du calcul pour rien.
        if(p->bombes() <= 1) {
            return true;
        }

        Game apres = plateauNu();

        souffler(apres, col, row);

        int apresLong = traceApresBombe.longueur();

        lancerEtude(apres, apresLong, apresLong < p->longueurMinimale(),
                    apresLong < p->longueurMinimale() && essai >= 2);

        return true;
    }

    // En vol : on attend. Le bot continue de jouer pendant ce temps -- ce
    // qu'il pose l'est sur le trace courant, et une pose devenue fausse se
    // reprend (voir caseAServir). Seule la planification, elle, impose le
    // silence.
    if(etude != nullptr) {
        return false;
    }

    // DEJA REPONDU : "aucun emplacement ne vaut la bombe" vaut jusqu'a ce que
    // le terrain change. Sans cette memoire l'etude repartait au battement
    // suivant, et au suivant -- un thread par battement dans la fenetre.
    if(etudeFaite) {
        return false;
    }

    // Sinon on la lance. Tout par copie : le thread ne voit qu'un Game nu.
    lancerEtude(plateauNu(), trace.longueur(), traceTropCourt(),
                traceTropCourt() && essai >= 2);

    // Le resultat ne sera pas la avant plusieurs battements -- ou tout de
    // suite, en synchrone.
    return etude != nullptr && etude->prete.load(std::memory_order_acquire)
           ? choisirPaquetDeBlocs(col, row) : false;
}

void BotTrace::lancerEtude(const Game &vierge, int courant, bool allonger,
                           bool depenser) const {
    int largeur = vierge.getLargeur();
    int objectif = p->longueurMinimale();
    QVector<int> candidats;

    for(int i = 0; i < vierge.getSize(); i++) {
        // Il faut un souffle qui ouvre quelque chose -- et une case qu'on ait
        // le droit de miner, ce qui se demande au PLATEAU REEL et non au
        // plateau nu.
        //
        // La nuance a coute une soiree. `peutMiner` exige une case
        // TOTALEMENT vide ; le plateau nu, lui, a efface toutes les pieces
        // posees. Une etude qui ne regarde que lui finit donc par designer une
        // case ou le bot vient de poser, `poserBombe` refuse, et rien ne le
        // rattrape : l'etude repart, redonne la meme case, et la bombe n'est
        // jamais posee. C'est exactement ce que le user voyait -- deux bombes
        // parties, les deux dernieres bloquees en stock.
        if(!p->peutMiner(i % largeur, i / largeur)) {
            continue;
        }

        Game essaiSouffle(vierge);

        if(souffler(essaiSouffle, i % largeur, i / largeur) > 0) {
            candidats << i;
        }
    }

    if(candidats.isEmpty()) {
        etudeFaite = true;
        return;
    }

    auto etat = std::make_shared<Etude>();
    // Ce qu'on demande a chaque candidat : un progres franc, pas l'objectif.
    // Voir MARGE_ETUDE.
    int visee = qMin(objectif, courant + MARGE_ETUDE);

    auto travail = [etat, vierge, candidats, visee, courant, allonger,
                    depenser, largeur]() {
        // LA REFERENCE, ET C'EST TOUT LE SUJET. On ne peut pas comparer un
        // candidat evalue a BUDGET_ETUDE au trace en place, qui a ete trouve
        // avec seize fois plus de noeuds : le budget reduit sous-estime
        // systematiquement, aucun candidat ne "bat" le trace courant, et
        // l'etude conclut eternellement que rien ne vaut la bombe.
        //
        // C'est ce que le user a vu : mort au niveau 36 a 143/150, avec deux
        // bombes en reserve jamais posees. On mesure donc le plateau ACTUEL
        // avec le meme budget que les candidats, et on compare a ca.
        Trace reference;

        reference.calculer(&vierge, visee, BUDGET_ETUDE);

        int repere = qMin(courant, reference.longueur());

        // L'ECHEANCE. Le balayage complet coute jusqu'a 10 s ; on rend le
        // meilleur trouve a l'heure dite, quitte a n'avoir pas tout vu.
        QElapsedTimer horloge;

        horloge.start();

        int meilleure = -1;
        Trace meilleurTrace;

        foreach(int idx, candidats) {
            if(horloge.elapsed() > DELAI_ETUDE_MS) {
                break;
            }

            Game apres(vierge);

            souffler(apres, idx % largeur, idx / largeur);

            Trace essaiTrace;

            essaiTrace.calculer(&apres, visee, BUDGET_ETUDE);

            if(essaiTrace.longueur() <= meilleure) {
                continue;
            }

            meilleure = essaiTrace.longueur();
            meilleurTrace = essaiTrace;
            etat->caseBombe = idx;
        }

        // La bombe doit payer, et le juge est le repere -- jamais le trace en
        // place, qui a ete trouve avec un autre budget.
        //
        // AU REJEU, ON DEPENSE SANS CONDITION. Garder une comparaison ici
        // revenait a ne jamais depenser : sur un plateau modifie, une
        // recherche a budget reduit fait souvent moins bien que sur le plateau
        // d'origine, donc aucun candidat ne passe la barre. Mesure sur
        // `--graine 2998034427` au niveau 33 : le bot mourait a 136/138 --
        // DEUX cases -- avec neuf bombes en stock, l'etude repondant "aucun
        // emplacement ne vaut la bombe" a chaque rejeu.
        //
        // Rien ne se perd a depenser : installerTrace refuse tout trace plus
        // court que celui en place, le terrain ouvert reste ouvert d'un rejeu
        // a l'autre, et une bombe gardee pour un niveau qu'on n'atteindra
        // jamais ne vaut rien. La seule chose qui arrete la depense est le
        // trace qui rejoint l'objectif : `allonger` tombe, et avec lui
        // `depenser`.
        bool vaut = depenser ? meilleure >= 0
                  : allonger ? meilleure > repere
                             : meilleure >= repere;

        if(!vaut) {
            etat->caseBombe = -1;
        } else {
            etat->trace = meilleurTrace;
        }

        etat->prete.store(true, std::memory_order_release);
    };

    etude = etat;

    // SYNCHRONE HORS DE LA FENETRE, comme la planification et pour une raison
    // de plus. Le determinisme du banc d'abord ; mais surtout, le banc deroule
    // une manche de soixante secondes en quelques millisecondes de montre. Un
    // calcul confie a un thread n'y rentre JAMAIS a temps : le bot ne bomberait
    // plus du tout, et la mesure porterait sur un bot qu'on n'a pas ecrit.
    if(!planificationAsynchrone()) {
        travail();
        return;
    }

    std::thread(travail).detach();
}

void BotTrace::installerTrace(const Trace &calcule) {
    if(seulementSiPlusLong && calcule.longueur() < tracePrecedente.longueur()) {
        qDebug() << "=== trace === la recherche recule apres la bombe ("
                 << calcule.longueur() << "<" << tracePrecedente.longueur()
                 << ") : on garde le precedent, ses cases sont toujours libres";
        trace = tracePrecedente;
        return;
    }

    trace = calcule;
}

void BotTrace::annoncerPlan() const {
    // Les croix se comptent sur la suite des rangs : une case qui y figure
    // deux fois est un croisement.
    QVector<char> vues(p->getLargeur() * p->getHauteur(), 0);
    int croix = 0;

    for(int r = 0; r < trace.longueur(); r++) {
        int idx = trace.caseAuRang(r);

        if(vues.at(idx)) {
            croix++;
        } else {
            vues[idx] = 1;
        }
    }

    qDebug() << "=== trace === niveau" << p->niveau() << "essai" << essai
             << ": " << trace.longueur() << "/" << p->longueurMinimale()
             << ", garde" << trace.garde() << "sur" << piecesPlanifiees
             << "," << croix << "croix"
             << (essai >= 2 ? "(file connue)" : "(file visible)");
}

bool BotTrace::bombeAVenir() const {
    if(bombeEnAttente()) {
        return true;
    }

    // Une etude ne compte que si quelqu'un viendra encore en chercher le
    // resultat. `gererBombes` sort des que le stock est vide ou que l'attente
    // est finie : une etude lancee juste avant que la derniere bombe parte
    // resterait alors en vol pour toujours, et cette garde figerait le bot
    // jusqu'au bout de la manche.
    //
    // C'est exactement ce que le user a vu sur `--graine 1188181038 --niveau 38
    // --vies 10 --bombes 1` : zero traversee, une case posee, "il meurt sans
    // rien faire". Et la manche muette ne consommait aucune piece, donc le
    // rejeu replanifiait avec cinq pieces connues au lieu de quatre-vingts.
    return etude != nullptr && p->bombes() > 0 && p->etat() == epAttente;
}

bool BotTrace::planEnVol() const {
    return calcul != nullptr;
}

void BotTrace::recolterPlan() {
    // Sans jamais attendre : on repasse a chaque battement, et geler ici
    // serait reintroduire ce qu'on vient d'enlever.
    if(calcul == nullptr || !calcul->prete.load(std::memory_order_acquire)) {
        return;
    }

    installerTrace(calcul->trace);
    calcul.reset();
    annoncerPlan();

    // Le pavage ne peut pas preceder le trace : il masque ses cases. C'est la
    // moitie de construirePlan qui avait ete mise en attente.
    Bot::construirePlan();
}

bool BotTrace::reserveeAuTrace(int col, int row) const {
    return trace.rang(col, row) >= 0;
}

// La case la plus en amont que le trace reclame pour cette piece.
//
// "Deja servie" se juge avec Trace::convient et non par egalite : une croix
// posee la ou un droit est demande traverse tout droit, le flux passe
// exactement pareil, il n'y a rien a corriger. C'est aussi ce qui donne un
// emploi au septieme type.
//
// Une piece POSEE mais fausse, en revanche, reste a reprendre -- et c'est bien
// la case la plus en amont. Le cas ne se produit qu'apres une replanification :
// la defausse, elle, n'a pas le droit de toucher au trace.
int BotTrace::caseAServir(const ETypePiece &enMain) const {
    // La croix d'abord la ou elle est irremplacable -- voir bottrace.h.
    if(enMain == tpCroix && !acculeParLeFlux(2.0f)) {
        int croisement = premiereCaseServie(enMain, true);

        if(croisement >= 0) {
            return croisement;
        }
    }

    return premiereCaseServie(enMain, false);
}

int BotTrace::premiereCaseServie(const ETypePiece &enMain,
                                 bool seulementCroisements) const {
    int largeur = p->getLargeur();

    for(int rang = 0; rang < trace.longueur(); rang++) {
        int idx = trace.caseAuRang(rang);
        int col = idx % largeur;
        int row = idx / largeur;
        ETypePiece voulu = trace.type(col, row);

        if(seulementCroisements && voulu != tpCroix) {
            continue;
        }

        if(Trace::convient(p->plateau()->getTypePiece(col, row), voulu)) {
            continue;
        }

        // Figee par le flux, ou hors manche : plus rien a y faire.
        if(!p->peutPoser(col, row)) {
            continue;
        }

        if(Trace::convient(enMain, voulu)) {
            return idx;
        }
    }

    return -1;
}

bool BotTrace::resteAServir() const {
    int largeur = p->getLargeur();

    for(int rang = 0; rang < trace.longueur(); rang++) {
        int idx = trace.caseAuRang(rang);
        int col = idx % largeur;
        int row = idx / largeur;

        if(Trace::convient(p->plateau()->getTypePiece(col, row), trace.type(col, row))) {
            continue;
        }

        if(p->peutPoser(col, row)) {
            return true;
        }
    }

    return false;
}

void BotTrace::jouer(float dt) {
    noterFile();

    // Le thread a-t-il fini ? Si oui le trace et le pavage sont a jour des ce
    // battement ; sinon on ne joue pas. Poser une piece sur un plan perime,
    // c'est exactement ce que le silence pendant la meche evite deja -- ici
    // ca dure quelques dixiemes de seconde de plus, le temps de la recherche.
    recolterPlan();

    if(planEnVol()) {
        return;
    }

    // OBJECTIF ACQUIS : le v3 prend le relais. Le trace s'arrete a
    // `longueurMinimale` -- au-dela il n'a rien a proposer, et ce qui reste a
    // gagner n'est plus une manche mais des points, 50 par case traversee. Le
    // v3 est fait pour ca : il prolonge la tete de proche en proche, sans plan.
    //
    // La barre d'espace ne se demande donc plus des que le trace est servi : on
    // la laisse a `abandonner()`, c'est-a-dire au moment ou plus aucune piece
    // ne peut prolonger le tuyau. La prime de depart anticipe (200 points) reste
    // prise par la meme voie qu'avant.
    //
    // objectifAcquis et non objectifRestant : depuis rienAPerdre, le second
    // rend zero dans DEUX cas -- l'objectif dans la poche et la manche perdue
    // d'avance -- et ils n'appellent pas le meme bot. Le test brut les separe,
    // et il garde sa priorite : une manche declaree imprenable que le v4 finit
    // par boucler quand meme est une manche GAGNEE, elle repasse au v3.
    if(objectifAcquis()) {
        BotSpaceAnticp::jouer(dt);
        return;
    }

    // MANCHE IMPRENABLE : le trace ne vaut pas l'objectif et plus aucune bombe
    // ne viendra le rallonger. La manche est perdue quoi qu'on fasse -- autant
    // remplir le plateau et encaisser les traversees au passage, ce que le v4
    // fait de son mieux. Sans ca le bot pose deux pieces et appuie sur la barre
    // d'espace, une fois par vie, devant un plateau vide.
    //
    // ET SANS FILET : rienAPerdre est vrai des cet instant, donc le v4 qui
    // prend la main ne verifie plus la place et ne s'interdit plus rien (voir
    // rienAPerdre, et BotMemoire::poseAcceptable pour le veto). Mesure du
    // 2026-09-19, 15 graines par niveau, 3 vies, sans bombe : 12 650 -> 58 650
    // points au niveau 40, 10 275 -> 48 800 au niveau 45, 9 400 -> 42 150 au
    // niveau 50. Sur `--graine 1 --niveau 45`, le flux traversait 3 cases ; il
    // en traverse 57.
    //
    // MAIS LE TRACE D'ABORD, et c'est une correction du 2026-09-19 signalee
    // par le user : "le trace a l'air ok, mais il decroche et part en bot
    // memoire". Il avait raison de trouver ca etrange. Le relais avait ete
    // ecrit pour le cas DEGENERE -- un trace d'UNE case pour un objectif de
    // 210 -- et il s'appliquait des qu'il manquait une seule case. Sur
    // `--graine 2998034427 --niveau 36`, le planificateur rendait 139 cases
    // pour un objectif de 150 : le bot jetait ces 139 cases, improvisait, et
    // la manche rendait VINGT traversees. Le trace en valait 139, soit 6 950
    // points contre 1 000.
    //
    // Un trace trop court reste donc un trace, et on le sert jusqu'au bout.
    // Le v4 ne prend la main qu'apres, pour prolonger le tuyau la ou le plan
    // s'arrete -- ce qu'il sait faire, et qui vaut mieux que la barre
    // d'espace.
    if(mancheImprenable() && !resteAServir() && !bombeAVenir()) {
        BotMemoire::jouer(dt);
        return;
    }

    if(!prendreJeton(dt)) {
        return;
    }

    // UNE BOMBE EST EN CHEMIN : on ne joue pas. Tout ce qu'on poserait serait
    // perime par la replanification qui suit le souffle, et surtout foncer
    // jetterait la manche avant que le terrain ait bouge -- c'est exactement
    // ce qui arrivait sur un reservoir enferme dans une poche : le trace
    // faisait deux cases, le bot n'avait rien a servir, il appuyait sur la
    // barre d'espace et mourait avec sa bombe encore en l'air.
    //
    // "En chemin" couvre les deux temps : la meche qui brule, et l'ETUDE qui
    // cherche encore ou poser. Le second manquait, et il ne se voyait que dans
    // la fenetre -- voir bombeAVenir.
    if(bombeAVenir() && traceTropCourt()) {
        return;
    }

    Piece piece = p->file()->getPiece(0);
    int idx = caseAServir(piece.type);

    if(idx >= 0) {
        // Pose sur le trace. Pas de marque de tas : ce n'est pas du reseau
        // parallele, c'est le tuyau lui-meme -- et le journal la compte a
        // l'origine 0.
        p->poserPiece(idx % p->getLargeur(), idx / p->getLargeur());
        return;
    }

    // Plus rien a poser nulle part sur le trace : soit il est complet, soit le
    // flux a fige ce qui restait. Dans les deux cas plus aucun geste ne peut
    // allonger le tuyau, donc on appuie sur la barre d'espace -- ce qui vaut la
    // prime de depart anticipe quand le trace est fini avant la fin du delai,
    // et deroule la fin sans attendre sinon.
    //
    // Le cas "trace plus court que l'objectif" passe par la lui aussi : la
    // manche est perdue depuis le premier battement, autant encaisser tout de
    // suite les traversees acquises. C'est le palier 3 de TRACE.md, §6 ter.
    if(!resteAServir()) {
        demanderFoncer();
        return;
    }

    // La piece n'a d'emploi sur aucune case encore libre du trace : elle part
    // au reseau de defausse, qui ne pave plus que le hors trace.
    defausser();
}
