#include "botspaceanticp.h"

BotSpaceAnticp::BotSpaceAnticp(Partie *p, float cadence, quint32 seed) : Bot(p, cadence, seed) {
}

int BotSpaceAnticp::choisirPont(int col, int row, ESens entree, bool strict) const {
    PieceFile *file = p->file();

    // La premiere piece de la file qui se raccorde a la tete et passe le filtre.
    for(int c = 0; c < file->getTaille(); c++) {
        Piece pc = file->getPiece(c);

        if(Ecoulement::piecesCompatibles(entree).contains(pc.type)
           && poseAcceptable(pc.type, col, row, entree, strict)) {
            return c;
        }
    }

    return -1;
}

bool BotSpaceAnticp::poseAcceptable(const ETypePiece& type, int col, int row, ESens entree,
                                    bool strict) const {
    // culDeSac englobe meneALaMort, mais le tester d'abord evite le parcours
    // dans le cas lache -- et dit en une ligne ce que les deux etages veulent.
    if(meneALaMort(type, col, row, entree)) {
        return false;
    }

    return !strict || !culDeSac(type, col, row, entree);
}

bool BotSpaceAnticp::caseAnticipee(int tCol, int tRow, ESens tEntree, int pont,
                                   int &fCol, int &fRow, ESens &fEntree,
                                   QVector<int> *chaine) const {
    Game *plateau = p->plateau();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    // Depart de la chaine : la case ou debouche file[pont] pose sur la tete.
    QVector<ESens> sortiePont = Ecoulement::sorties(p->file()->getPiece(pont).type, sHaut, tEntree);
    if(sortiePont.isEmpty()) {
        return false;
    }

    int cx, cy;
    ESens ce;
    Ecoulement::voisine(tCol, tRow, sortiePont.first(), cx, cy, ce);

    if(chaine != nullptr) {
        chaine->clear();
        *chaine << tRow * largeur + tCol;   // la tete, prise par le pont
    }

    // Borne de securite : une croix peut refermer la chaine sur elle-meme.
    // Deux passages par case, comme dans Bot::tete() -- la croix se traverse
    // une fois par axe.
    for(int garde = 2 * plateau->getSize(); garde > 0; garde--) {
        if(cx < 0 || cx >= largeur || cy < 0 || cy >= hauteur) {
            return false;
        }

        ETypePiece t = plateau->getTypePiece(cx, cy);

        if(chaine != nullptr) {
            *chaine << cy * largeur + cx;
        }

        if(t == tpNone) {
            fCol = cx;
            fRow = cy;
            fEntree = ce;
            return true;
        }

        // Case deja posee : on la traverse si elle raccorde (case pre-posee a un
        // tour precedent), sinon la chaine est coupee.
        ESens sens = plateau->getSens(cx, cy);
        QVector<ESens> st = Ecoulement::sorties(t, sens, ce);

        if(!Ecoulement::ouvertures(t, sens).contains(ce) || st.isEmpty()) {
            return false;
        }

        Ecoulement::voisine(cx, cy, st.first(), cx, cy, ce);
    }

    return false;
}

// La piece qu'on s'apprete a pre-poser sera un maillon d'une chaine, pas une
// tete isolee. La juger seule -- ce que fait poseAcceptable -- revient a
// demander "y a-t-il de la place autour de cette case ?", et la reponse est oui
// tant que le plateau n'est pas plein. La vraie question est : une fois cette
// piece en place, le PONT pose sur la tete mene-t-il encore quelque part ?
//
// On la pose donc pour de faux et on remesure la chaine entiere depuis la tete.
// C'est le seul moment ou le futur flux est pris en compte : la marche
// d'espaceApres traverse alors tous les maillons deja poses, celui-ci compris,
// et debouche la ou le trace debouchera vraiment.
//
// Sans ce test, le v3 batissait des chaines qui ne menaient nulle part, chaque
// maillon ayant l'air excellent isolement -- 193 cases libres autour -- et
// l'ensemble finissant dans un mur trois cases plus loin.
bool BotSpaceAnticp::chaineViable(int col, int row, ESens entree, int pont,
                                  int fCol, int fRow, const ETypePiece &type,
                                  const QVector<int> &chaine) const {
    Game *plateau = p->plateau();
    ETypePiece avant = plateau->getTypePiece(fCol, fRow);
    ESens sensAvant = plateau->getSens(fCol, fRow);

    plateau->setTypePiece(fCol, fRow, type);

    // Le besoin, calcule comme dans culDeSac mais en tenant la chaine pour
    // deja prise : ses cases -- posees ou encore vides -- seront traversees par
    // le flux, elles ne comptent pas comme place disponible.
    int restant = objectifRestant();
    bool bon = true;

    if(restant > 0) {
        int besoin = qMax(1, restant - 1);
        bon = espaceApres(p->file()->getPiece(pont).type, col, row, entree,
                          besoin, nullptr, &chaine) >= besoin;
    }

    plateau->setTypePiece(fCol, fRow, avant);
    plateau->setSens(fCol, fRow, sensAvant);

    return bon;
}

void BotSpaceAnticp::ancrerDefausse(int col, int row, ESens entree) {
    Game *plateau = p->plateau();

    ancrageDefausse = -1;

    // Un pont, meme lache : il suffit a projeter un trajet plausible.
    int pont = choisirPont(col, row, entree, false);

    if(pont < 0) {
        return;
    }

    int fc, fr;
    ESens fe;

    if(!caseAnticipee(col, row, entree, pont, fc, fr, fe)) {
        return;
    }

    // On suppose dans la premiere case libre le premier type qui s'y raccorde,
    // et on regarde ou le trajet debouche ensuite. C'est le rang 2.
    QVector<ETypePiece> possibles = Ecoulement::piecesCompatibles(fe);

    if(possibles.isEmpty()) {
        return;
    }

    ETypePiece avant = plateau->getTypePiece(fc, fr);
    plateau->setTypePiece(fc, fr, possibles.first());

    int gc, gr;
    ESens ge;

    if(caseAnticipee(col, row, entree, pont, gc, gr, ge) && (gc != fc || gr != fr)) {
        ancrageDefausse = gr * p->getLargeur() + gc;
    }

    plateau->setTypePiece(fc, fr, avant);
}

void BotSpaceAnticp::jouer(float dt) {
    if(!prendreJeton(dt)) {
        return;
    }

    int col, row;
    ESens entree;

    // tete() convertit au passage les cases du tas raccordees au trace -- dont
    // celles que le v3 avait pre-posees et que le pont vient de relier.
    if(!tete(col, row, entree)) {
        demanderFoncer();
        return;
    }

    // Aucune piece ne peut prolonger la tete : inutile d'anticiper ou d'attendre.
    if(teteCondamnee(col, row, entree)) {
        abandonner(col, row, entree);
        return;
    }

    // On cherche d'abord un pont qui laisse de quoi boucler l'objectif.
    int pont = choisirPont(col, row, entree, true);
    bool strict = pont >= 0;

    if(!strict) {
        // Aucune piece de la file ne convient a ce compte-la. Deux raisons
        // possibles, et elles n'appellent pas la meme reponse :
        //
        //   - la tete pourrait etre sauvee, mais par un type que la file ne
        //     contient pas (ouvertureUtile > 0) : on defausse et on attend.
        //     C'est le cas qui compte. S'en passer, c'est poser une piece dont
        //     on voit qu'elle s'enferme -- une poche d'une case quand la moitie
        //     du plateau est libre -- alors que le flux n'a pas encore bouge ;
        //   - aucun type ne la sauverait : attendre ne ferait que depenser la
        //     manche en defausses. On se rabat alors sur "ne pas mourir sur le
        //     coup", faute de mieux.
        if(!acculeParLeFlux(2.0f) && ouvertureUtile(col, row, entree) > 0) {
            ancrerDefausse(col, row, entree);
            defausser();
            return;
        }

        pont = choisirPont(col, row, entree, false);
    }

    if(pont < 0) {
        // Aucune piece de la file ne prolonge la tete : on attend une meilleure
        // pioche tant que le flux ne talonne pas.
        if(!acculeParLeFlux(2.0f)) {
            ancrerDefausse(col, row, entree);
            defausser();
            return;
        }

        abandonner(col, row, entree);
        return;
    }

    // Le pont retenu est au sommet : on le pose sur la tete.
    if(pont == 0) {
        p->poserPiece(col, row);
        return;
    }

    // Pont plus loin : on pre-pose le haut de file sur la premiere case libre
    // de la chaine qui suivra ce pont, s'il y est acceptable -- au meme degre
    // d'exigence que le pont lui-meme. Avoir renonce pour le pont et rester
    // exigeant pour la chaine ne protegerait rien et couterait des defausses.
    // Geste apres geste, les pieces d'avant le pont s'y enchainent.
    int fc, fr;
    ESens fe;
    Piece haut = p->file()->getPiece(0);

    QVector<int> chaine;

    if(caseAnticipee(col, row, entree, pont, fc, fr, fe, &chaine)
       && Ecoulement::piecesCompatibles(fe).contains(haut.type)
       && poseAcceptable(haut.type, fc, fr, fe, strict)
       && chaineViable(col, row, entree, pont, fc, fr, haut.type, chaine)) {
        poserCaseTas(fc, fr, 2);
        return;
    }

    // Rien d'utile a faire du haut de file maintenant.
    if(!acculeParLeFlux(2.0f)) {
        ancrerDefausse(col, row, entree);
        defausser();
        return;
    }

    abandonner(col, row, entree);
}
