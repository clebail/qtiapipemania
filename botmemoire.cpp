#include <QtDebug>
#include <algorithm>

#include "botmemoire.h"

// Seuil de bruit de la liste de blame, en cases de marge brulees d'un carrefour
// au suivant (voir classerBlame). En dessous, le carrefour n'a rien decide qui
// compte, et un veto sur un carrefour sans enjeu est arbitraire (VIES.md §3).
//
// Dix, c'est aussi COUSSIN_CUL_DE_SAC (bot.cpp) : l'unite de "vraie reserve"
// que le reste du bot s'est donnee au banc. La coupure est franche sur les
// manches regardees -- le bruit vit entre 0 et 2, les coupables au-dessus de
// 30 -- donc la valeur exacte importe peu tant qu'elle tombe dans ce fosse.
// Elle n'a PAS ete balayee : c'est le premier reglage a mesurer quand le bras
// v4 aura ses 400 parties.
#ifndef SEUIL_BLAME
#define SEUIL_BLAME 10
#endif

BotMemoire::BotMemoire(Partie *p, float cadence, quint32 seed) : BotSpaceAnticp(p, cadence, seed) {
    graineVue = p->getGraine();
    niveauVu = p->niveau();
    mancheVue = p->numeroManche();
}

void BotMemoire::jouer(float dt) {
    surveillerFrontieres();

    int col, row;
    ESens entree;

    // tete() sort du tas les cases que le trace a rejointes. BotSpaceAnticp
    // l'appelle de toute facon juste apres : l'avancer ici ne change pas son
    // geste, ca ne fait qu'en lire l'etat avant qu'il ne joue.
    if(tete(col, row, entree)) {
        journaliser(col, row, entree);
        desamorcer(col, row, entree);
    }

    BotSpaceAnticp::jouer(dt);
}

// ---------------------------------------------------------------------------
// Frontieres
// ---------------------------------------------------------------------------

void BotMemoire::surveillerFrontieres() {
    quint32 graine = p->getGraine();
    int niveau = p->niveau();
    int manche = p->numeroManche();

    // Partie neuve. C'est le SEUL signal qui la distingue : nouvellePartie()
    // retire une graine au hasard, alors que le niveau retombe au niveau de
    // depart -- indiscernable d'une victoire quand on jouait deja le premier.
    // Se tromper ici ne se verrait pas tout de suite : les vetos d'une partie
    // morte survivraient a la suivante, en la sabotant sur un plateau qui n'a
    // plus rien a voir (VIES.md §B).
    if(graine != graineVue) {
        graineVue = graine;
        niveauVu = niveau;
        mancheVue = manche;
        oublierTout();
        return;
    }

    if(manche == mancheVue) {
        return;
    }

    if(niveau != niveauVu) {
        // Manche reussie : le niveau monte, donc le plateau et la file changent
        // entierement. Les vetos portent sur des cases de l'ancien plateau et
        // n'ont plus d'objet -- les garder poserait un interdit arbitraire sur
        // un terrain qui n'a plus rien a voir.
        //
        // VIES.md §3 ne borne l'oubli qu'au game over ; on l'avance au
        // changement de niveau, qui l'englobe. Le rejeu parfait -- toute la
        // raison d'etre du veto -- ne vaut que DANS un niveau.
        niveauVu = niveau;
        oublierTout();
    } else {
        // Meme niveau, manche suivante : c'est le rejeu. Le journal qu'on vient
        // de remplir est celui de la manche perdue.
        consommerUnBlame();
    }

    mancheVue = manche;
    journal.clear();
    ouvert = false;
}

void BotMemoire::oublierTout() {
    interdits.clear();
    journal.clear();
    reference.clear();
    blame.clear();
    blameConsomme = 0;
    blameFige = false;
    ouvert = false;
}

void BotMemoire::consommerUnBlame() {
    // Le blame se calcule sur l'essai 1 et ne se recalcule plus. Les essais
    // suivants ont diverge : refaire la liste sur la partie qui vient de se
    // jouer donnerait une suite d'interdits sans raison de converger. On la
    // descend dans l'ordre, une entree consommee par mort.
    if(!blameFige) {
        reference = journal;
        noter(reference);
        blame = classerBlame(reference);
        blameFige = true;

        qDebug() << "=== blame ===" << blame.size() << "carrefours retenus sur"
                 << journal.size() << "journalises";
    }

    if(blameConsomme >= blame.size()) {
        // Liste epuisee avant les vies : on rejoue sans veto supplementaire
        // plutot qu'avec un veto au hasard.
        return;
    }

    const Carrefour &c = blame.at(blameConsomme++);

    interdire(c.col, c.row, c.entree, c.choisie);

    qDebug() << "veto" << blameConsomme << "/" << blame.size()
             << "case=(" << c.col << "," << c.row << ") entree=" << c.entree
             << "direction interdite=" << c.choisie
             << "restant=" << c.restant << "prise=" << c.prise
             << "meilleure=" << c.meilleure << "gravite=" << c.gravite;
}

// ---------------------------------------------------------------------------
// Journal des carrefours
// ---------------------------------------------------------------------------

void BotMemoire::journaliser(int col, int row, ESens entree) {
    // La tete ne peut pas avoir bouge sans que le plateau bouge, et mesurer
    // quatre capacites non bornees a chaque battement couterait pour rien. Meme
    // garde que marquerObligations, pour la meme raison.
    //
    // `ouvert` dans la condition : au premier geste d'une manche rejouee, le
    // plateau reinitialise a la meme signature qu'a la manche precedente au
    // meme instant. Sans ca on manquerait son premier carrefour.
    quint32 signature = signaturePlateau();

    if(signature == signatureJournal && ouvert) {
        return;
    }

    signatureJournal = signature;

    if(ouvert && (col != oCol || row != oRow || entree != oEntree)) {
        fermerCarrefour();
    }

    if(!ouvert) {
        ouvrirCarrefour(col, row, entree);
    }
}

void BotMemoire::ouvrirCarrefour(int col, int row, ESens entree) {
    oCol = col;
    oRow = row;
    oEntree = entree;
    oRestant = objectifRestant();

    for(int d = 0; d < 4; d++) {
        oCapacite[d] = 0;
    }

    // Une DIRECTION, jamais un type : la croix traverse tout droit, elle est
    // donc le meme chemin que le tuyau droit de son axe. Deux types qui
    // debouchent du meme cote sont un seul choix -- on garde la plus large des
    // deux mesures, leurs parcours ne different que par la case de pose.
    //
    // maxi = 0, la mesure exacte : le journal compare des capacites entre
    // elles, et une mesure bornee les rendrait toutes egales au plafond.
    foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
        ESens sortie;

        if(!sortieDe(type, entree, sortie)) {
            continue;
        }

        int capacite = espaceApres(type, col, row, entree, 0);

        if(capacite > oCapacite[(int)sortie]) {
            oCapacite[(int)sortie] = capacite;
        }
    }

    ouvert = true;
}

void BotMemoire::fermerCarrefour() {
    ouvert = false;

    Game *plateau = p->plateau();
    ETypePiece type = plateau->getTypePiece(oCol, oRow);
    ESens sens = plateau->getSens(oCol, oRow);

    // La tete peut avoir bouge sans qu'on ait pose ici -- une case du tas que le
    // trace a rejointe plus loin, par exemple. Sans piece raccordee sur la case,
    // il n'y a pas eu de choix a cet endroit.
    if(type == tpNone || !Ecoulement::ouvertures(type, sens).contains(oEntree)) {
        return;
    }

    QVector<ESens> sort = Ecoulement::sorties(type, sens, oEntree);

    if(sort.isEmpty()) {
        return;
    }

    Carrefour c;

    c.col = oCol;
    c.row = oRow;
    c.entree = oEntree;
    c.restant = oRestant;
    c.meilleure = 0;
    c.gravite = 0;

    int vivantes = 0;

    for(int d = 0; d < 4; d++) {
        c.capacite[d] = oCapacite[d];

        if(oCapacite[d] > 0) {
            vivantes++;
        }

        if(oCapacite[d] > c.meilleure) {
            c.meilleure = oCapacite[d];
        }
    }

    // Une seule direction vivante : ce n'etait pas un carrefour mais un passage
    // oblige. Rien a blamer, le bot n'a rien choisi.
    if(vivantes < 2) {
        return;
    }

    c.choisie = sort.first();
    c.prise = oCapacite[(int)c.choisie];

    journal.append(c);
}

// ---------------------------------------------------------------------------
// Blame
// ---------------------------------------------------------------------------

// La MARGE d'un carrefour : ce que la meilleure direction offre au-dela de
// l'objectif restant. La gravite, c'est ce que le pas jusqu'au carrefour SUIVANT
// en a brule.
//
//   marge   = meilleure - restant
//   gravite = marge(i) - marge(i+1)
//
// C'est la table du §3, lue dans le bon sens :
//
//   carrefour 61 : restant=52  meilleure=174  marge=+123
//   carrefour 71 : restant=38  meilleure=118  marge=+81   -> gravite(61) = 42
//   carrefour 81 : restant=31  meilleure= 36  marge=+6    -> gravite(71) = 75
//
// et c'est bien le 71 -- celui qui a pris 41 quand 118 s'offrait -- qui sort.
//
// Ce qu'on ne peut PAS mesurer, et qui etait la premiere version de ce code :
// l'ecart entre les branches d'un meme carrefour. espaceApres inonde en
// 4-connexite, donc depuis n'importe quelle direction on retombe sur la meme
// grande poche : sur une manche de 52 carrefours mesuree (--graine 1910881835
// --niveau 31), meilleure et prise sont egales 48 fois sur 52. Le choix ne se
// voit pas au carrefour, il se voit un carrefour plus loin -- d'ou la
// difference decalee. Les deux coupables de cette manche brulent 33 et 31 cases
// de marge en un pas, quand les cinquante autres oscillent entre 0 et 2.
//
// Le regret du carrefour lui-meme n'a pas besoin d'un terme a part : une
// direction nettement moins large fait tomber `meilleure` au carrefour suivant,
// donc elle est deja dans la chute.
//
// Le DERNIER carrefour n'a pas de successeur et sort de la liste : le blame
// vise les carrefours, jamais le dernier geste (§3). Celui-la est l'endroit ou
// la manche se referme, pas celui ou elle s'est jouee.
void BotMemoire::noter(QVector<Carrefour> &journal) {
    for(int i = 0; i < journal.size(); i++) {
        // Le dernier reste a zero : pas de successeur, donc pas de chute -- et
        // c'est aussi bien, le blame ne vise jamais le dernier geste.
        if(i + 1 >= journal.size()) {
            journal[i].gravite = 0;
            break;
        }

        int marge = journal.at(i).meilleure - journal.at(i).restant;
        int margeApres = journal.at(i + 1).meilleure - journal.at(i + 1).restant;

        journal[i].gravite = marge - margeApres;
    }
}

QVector<BotMemoire::Carrefour> BotMemoire::classerBlame(const QVector<Carrefour> &note) {
    QVector<Carrefour> retenus;

    foreach(const Carrefour &c, note) {
        if(c.gravite >= SEUIL_BLAME) {
            retenus.append(c);
        }
    }

    // Tri stable : a gravite egale, le carrefour le plus AMONT passe d'abord.
    // C'est l'engagement qui coute, pas le geste ou la manche finit par se
    // refermer.
    std::stable_sort(retenus.begin(), retenus.end(),
                     [](const Carrefour &a, const Carrefour &b) {
                         return a.gravite > b.gravite;
                     });

    return retenus;
}

// ---------------------------------------------------------------------------
// Interdits
// ---------------------------------------------------------------------------

int BotMemoire::cle(int col, int row, ESens entree) const {
    return ((row * p->getLargeur() + col) << 2) | (int)entree;
}

bool BotMemoire::sortieDe(const ETypePiece& type, ESens entree, ESens &sortie) {
    if(!Ecoulement::piecesCompatibles(entree).contains(type)) {
        return false;
    }

    // Le sens ne compte que pour le reservoir, qui n'est jamais en jeu ici :
    // meme convention qu'espaceApres et caseAnticipee.
    QVector<ESens> sort = Ecoulement::sorties(type, sHaut, entree);

    if(sort.isEmpty()) {
        return false;
    }

    sortie = sort.first();

    return true;
}

bool BotMemoire::interdite(int col, int row, ESens entree, const ETypePiece& type) const {
    unsigned char masque = interdits.value(cle(col, row, entree), 0);

    if(masque == 0) {
        return false;
    }

    ESens sortie;

    if(!sortieDe(type, entree, sortie)) {
        return false;
    }

    return (masque & (unsigned char)(1 << (int)sortie)) != 0;
}

void BotMemoire::interdire(int col, int row, ESens entree, ESens direction) {
    interdits[cle(col, row, entree)] |= (unsigned char)(1 << (int)direction);
}

void BotMemoire::lever(int col, int row, ESens entree) {
    interdits.remove(cle(col, row, entree));
}

// ---------------------------------------------------------------------------
// Desamorcage
// ---------------------------------------------------------------------------

void BotMemoire::desamorcer(int col, int row, ESens entree) {
    if(!interdits.contains(cle(col, row, entree))) {
        return;
    }

    // Combien de types prolongent cette tete, avec et sans les vetos ?
    int avec = 0;
    int sans = 0;

    foreach(ETypePiece type, Ecoulement::piecesCompatibles(entree)) {
        if(meneALaMort(type, col, row, entree)) {
            continue;
        }

        sans++;

        if(!interdite(col, row, entree, type)) {
            avec++;
        }
    }

    // Le veto ne condamne rien tant qu'un type passe encore, et il n'est pour
    // rien dans une tete deja morte par elle-meme.
    if(sans == 0 || avec > 0) {
        return;
    }

    // Le carrefour n'en etait pas un : la direction interdite est la seule
    // vivante, donc c'est une case OBLIGEE. Le suicide que le veto provoquerait
    // est informatif -- il prouve qu'il n'y avait pas de bifurcation ici -- mais
    // une fois seulement. Laisse en place, il se redeclencherait a chaque vie et
    // le bot se suiciderait en boucle jusqu'au game over.
    lever(col, row, entree);

    qDebug() << "veto empoisonnant leve : case=(" << col << "," << row
             << ") entree=" << entree << "-- passage oblige";

    // Et on le repose en AMONT : si le bot ne devait pas se retrouver ici, c'est
    // plus tot qu'il fallait bifurquer. L'amont se lit dans le journal de
    // l'essai 1, seul endroit ou l'ordre des carrefours soit conserve -- le
    // journal de la manche en cours s'arrete a ce geste-ci.
    int rang = -1;

    for(int i = reference.size() - 1; i >= 0; i--) {
        const Carrefour &c = reference.at(i);

        if(c.col == col && c.row == row && c.entree == entree) {
            rang = i;
            break;
        }
    }

    for(int i = rang - 1; i >= 0; i--) {
        const Carrefour &c = reference.at(i);

        // Un carrefour deja sous veto n'a rien de plus a donner, et un
        // carrefour sans enjeu ne vaut pas mieux que pas de veto du tout : le
        // remplacement passe la meme barre que la liste de blame, sinon le
        // desamorcage se contenterait du premier carrefour venu.
        if(c.gravite < SEUIL_BLAME || interdits.contains(cle(c.col, c.row, c.entree))) {
            continue;
        }

        interdire(c.col, c.row, c.entree, c.choisie);

        qDebug() << "  remplace par un veto amont : case=(" << c.col << "," << c.row
                 << ") direction=" << c.choisie << "gravite=" << c.gravite;

        return;
    }

    // Aucun amont disponible : le veto est simplement abandonne. La mort
    // suivante consommera l'entree suivante de la liste.
}

// ---------------------------------------------------------------------------
// Application
// ---------------------------------------------------------------------------

bool BotMemoire::poseAcceptable(const ETypePiece& type, int col, int row, ESens entree,
                                bool strict) const {
    // Veto DUR tant qu'il reste une vie a depenser ; sur la derniere il ne vaut
    // plus que par la passe preferentielle de choisirPont, qui leve vetoForce
    // apres coup. Le bot joue alors pour survivre, plus pour explorer.
    if((vetoForce || p->vies() > 1) && interdite(col, row, entree, type)) {
        return false;
    }

    return BotSpaceAnticp::poseAcceptable(type, col, row, entree, strict);
}

int BotMemoire::choisirPont(int col, int row, ESens entree, bool strict) const {
    if(p->vies() > 1) {
        // Le veto mord de lui-meme, par poseAcceptable. S'il ne reste rien, on
        // s'y tient : desamorcer() a deja ecarte le cas ou il condamne la tete,
        // donc ce qui reste est une attente, pas un suicide.
        return BotSpaceAnticp::choisirPont(col, row, entree, strict);
    }

    // Derniere vie : le veto redevient une preference. On cherche d'abord hors
    // des directions interdites, puis on accepte tout -- sans quoi le bot se
    // suiciderait sur place au lieu de reprendre la branche connue.
    vetoForce = true;
    int pont = BotSpaceAnticp::choisirPont(col, row, entree, strict);
    vetoForce = false;

    return pont >= 0 ? pont : BotSpaceAnticp::choisirPont(col, row, entree, strict);
}
