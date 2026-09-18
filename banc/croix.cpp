// PROTOTYPE -- l'auto-croisement par reecriture locale. TRACE.md, §7.
//
//   croix <parties> <niveau min> <niveau max> [fenetre] [schemas]
//
// La question posee par le user : existe-t-il des schemas ou poser une croix
// ici ou la conserve l'encombrement du schema d'origine, avec les memes
// entree/sortie, et gagne autant de passes que de croix ?
//
// Plutot que d'ecrire le catalogue a la main, on le fait DECOUVRIR : on prend
// le trace tel que le planificateur le rend, on glisse une fenetre le long de
// ses rangs, et on re-resout chaque fenetre par une recherche exhaustive qui,
// elle, a le droit de se croiser. Entree et sortie de la fenetre sont figees,
// le vivier de cases est borne, et on ne remplace que si la fenetre rend
// STRICTEMENT plus de passes. Ce que la recherche trouve est le catalogue --
// et l'option `schemas` en imprime les premiers exemplaires.
//
// Ce qu'il faut savoir avant de lire les chiffres, et c'est une loi, pas une
// heuristique. Colorie le plateau en damier : un trace alterne les couleurs a
// chaque pas, donc une marche de L visites a ses deux bouts de meme couleur si
// et seulement si L est impair. Une croix ajoute une visite DE SA PROPRE
// COULEUR. Consequence : a encombrement et extremites figes, on ne peut ajouter
// que des croix PAR PAIRES -- une seule retournerait la parite et il n'existe
// alors aucune marche. Une croix seule exige donc soit une case de plus (le
// terrain hors trace), soit de deplacer une extremite.
//
// D'ou les deux colonnes du releve : les croix trouvees a encombrement egal
// (par paires) et celles qui consomment du terrain libre (une case pour une
// croix, +2 passes).
#include <QtGlobal>
#include <QElapsedTimer>
#include <QVector>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "trace.h"

int tailleCase() { return 32; }

namespace {

// Cases d'un vivier, au plus : la recherche locale est exhaustive, c'est elle
// qui fixe le prix. 22 tient largement sous la milliseconde.
#ifndef VIVIER_MAX
#define VIVIER_MAX              22
#endif
// Noeuds par fenetre. Une fenetre qui ne se resout pas dans ce budget n'avait
// rien d'evident a rendre.
#define BUDGET_FENETRE          200000
// Passes de reecriture au plus : on s'arrete des qu'une passe entiere ne
// rapporte plus rien, ce qui arrive bien avant.
#ifndef PASSES_MAX
#define PASSES_MAX              12
#endif

ESens oppose(ESens s) {
    switch(s) {
    case sHaut:   return sBas;
    case sBas:    return sHaut;
    case sGauche: return sDroite;
    default:      return sGauche;
    }
}

bool estAxeVertical(ESens s) {
    return s == sHaut || s == sBas;
}

// --- la recherche locale ----------------------------------------------------
//
// Une marche dans le vivier, de la case d'entree a la case de sortie, qui
// maximise les VISITES. Une case vaut une visite ; une croix en vaut deux, et
// c'est tout le sujet.
struct Local {
    const Game *plateau = nullptr;
    int largeur = 0;
    int hauteur = 0;

    // Vivier : 1 si la case est utilisable par la reecriture.
    QVector<char> dansVivier;
    // Passes deja faites sur la case : 0, 1, ou 2 (croix).
    QVector<char> passes;
    // La premiere passe, pour savoir si la case peut en accueillir une seconde
    // -- il faut qu'elle soit DROITE, et la seconde perpendiculaire.
    QVector<ESens> entree1;
    QVector<ESens> sortie1;

    // Ou la marche doit finir, et par ou elle doit en sortir. arrivee < 0 :
    // c'est la queue du trace, elle finit ou elle veut.
    int arrivee = -1;
    ESens sortieFinale = sHaut;

    long budget = 0;
    int visitesMax = 0;
    int croixMax = 0;

    // La marche courante et la meilleure : une case par visite, donc une case
    // croisee y figure deux fois.
    QVector<int> chemin;
    QVector<ESens> entrees;
    QVector<ESens> sorties;
    QVector<int> meilleurChemin;
    QVector<ESens> meilleuresEntrees;
    QVector<ESens> meilleuresSorties;
};

// La case peut-elle etre TRAVERSEE une fois de plus par cette entree ?
bool accessible(const Local &l, int idx, ESens entree) {
    if(idx < 0 || !l.dansVivier.at(idx)) {
        return false;
    }

    if(l.passes.at(idx) == 0) {
        return true;
    }

    if(l.passes.at(idx) >= 2) {
        return false;
    }

    // Seconde passe : la premiere doit etre droite, et celle-ci lui etre
    // perpendiculaire. La croix ne tourne pas -- c'est sa definition meme,
    // Ecoulement::sorties lui rend le sens reciproque de l'entree.
    if(l.sortie1.at(idx) != oppose(l.entree1.at(idx))) {
        return false;
    }

    return estAxeVertical(entree) != estAxeVertical(l.entree1.at(idx));
}

// La sortie `s` est-elle jouable depuis cette entree, sur cette case ?
bool sortieJouable(const Local &l, int idx, ESens entree, ESens s) {
    if(Trace::typePour(entree, s) == tpNone) {
        return false;
    }

    // Seconde passe : elle doit etre droite.
    return l.passes.at(idx) == 0 || s == oppose(entree);
}

void garder(Local &l, int visites, int croix) {
    if(visites < l.visitesMax) {
        return;
    }

    // A visites egales, le moins de croix : une croix se paie en joker (elle
    // ne remplace plus un droit), donc a gain egal on n'en pose pas une de
    // plus. C'est la regle posee par le user.
    if(visites == l.visitesMax && croix >= l.croixMax) {
        return;
    }

    l.visitesMax = visites;
    l.croixMax = croix;
    l.meilleurChemin = l.chemin;
    l.meilleuresEntrees = l.entrees;
    l.meilleuresSorties = l.sorties;
}

void marcher(Local &l, int idx, ESens entree, int visites, int croix) {
    if(l.budget-- <= 0) {
        return;
    }

    // Peut-on s'arreter ici ? A la queue du trace, oui, partout. Ailleurs, il
    // faut etre sur la case de sortie ET pouvoir en sortir du bon cote, sans
    // quoi la piece suivante ne se raccorde plus.
    if(l.arrivee < 0) {
        l.passes[idx]++;
        l.chemin << idx;
        l.entrees << entree;
        l.sorties << entree;   // sans suite : la valeur ne sert pas
        garder(l, visites + 1, croix + (l.passes.at(idx) == 2 ? 1 : 0));
        l.passes[idx]--;
        l.chemin.removeLast();
        l.entrees.removeLast();
        l.sorties.removeLast();
    } else if(idx == l.arrivee && sortieJouable(l, idx, entree, l.sortieFinale)) {
        char avant = l.passes.at(idx);
        ESens e1 = l.entree1.at(idx), s1 = l.sortie1.at(idx);

        l.passes[idx]++;

        if(avant == 0) {
            l.entree1[idx] = entree;
            l.sortie1[idx] = l.sortieFinale;
        }

        l.chemin << idx;
        l.entrees << entree;
        l.sorties << l.sortieFinale;
        garder(l, visites + 1, croix + (avant == 1 ? 1 : 0));
        l.chemin.removeLast();
        l.entrees.removeLast();
        l.sorties.removeLast();
        l.passes[idx] = avant;
        l.entree1[idx] = e1;
        l.sortie1[idx] = s1;
    }

    // Puis on continue.
    for(int s = 0; s < 4; s++) {
        if(!sortieJouable(l, idx, entree, (ESens)s)) {
            continue;
        }

        int vCol, vRow;
        ESens vEntree;

        Ecoulement::voisine(idx % l.largeur, idx / l.largeur, (ESens)s, vCol, vRow, vEntree);

        if(vCol < 0 || vCol >= l.largeur || vRow < 0 || vRow >= l.hauteur) {
            continue;
        }

        int vIdx = vRow * l.largeur + vCol;
        char avant = l.passes.at(idx);
        ESens e1 = l.entree1.at(idx), s1 = l.sortie1.at(idx);

        // On pose la passe AVANT de tester la voisine : une case qu'on vient
        // de croiser n'est plus disponible pour la suite du meme pas.
        l.passes[idx]++;

        if(avant == 0) {
            l.entree1[idx] = entree;
            l.sortie1[idx] = (ESens)s;
        }

        if(accessible(l, vIdx, vEntree)) {
            l.chemin << idx;
            l.entrees << entree;
            l.sorties << (ESens)s;
            marcher(l, vIdx, vEntree, visites + 1, croix + (avant == 1 ? 1 : 0));
            l.chemin.removeLast();
            l.entrees.removeLast();
            l.sorties.removeLast();
        }

        l.passes[idx] = avant;
        l.entree1[idx] = e1;
        l.sortie1[idx] = s1;
    }
}

// --- le plateau -------------------------------------------------------------

// Copie NUE : blocs et reservoir, rien d'autre. Le meme calcul que
// BotTrace::planifier.
Game denuder(const Game &plateau) {
    Game vierge(plateau);

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

// La poche du reservoir : ce que le terrain permet vraiment. Mesure du
// 2026-09-19 : 64 blocs au plafond, et la poche tombe a 130-155 cases, bien en
// dessous de l'objectif des le niveau 39.
int poche(const Game &g, QVector<char> &dedans) {
    int largeur = g.getLargeur(), hauteur = g.getHauteur();
    int depart = g.getIdxDepart();
    int col, row;
    ESens e;

    dedans.fill(0, g.getSize());
    Ecoulement::voisine(depart % largeur, depart / largeur,
                        g.getSens(depart % largeur, depart / largeur), col, row, e);

    if(col < 0 || col >= largeur || row < 0 || row >= hauteur
       || g.getTypePiece(col, row) == tpBloque) {
        return 0;
    }

    QVector<int> pile;
    int n = 0;

    pile << row * largeur + col;
    dedans[row * largeur + col] = 1;

    while(!pile.isEmpty()) {
        int idx = pile.takeLast();
        n++;

        for(int s = 0; s < 4; s++) {
            int vc, vr;
            ESens ve;

            Ecoulement::voisine(idx % largeur, idx / largeur, (ESens)s, vc, vr, ve);

            if(vc < 0 || vc >= largeur || vr < 0 || vr >= hauteur) {
                continue;
            }

            int v = vr * largeur + vc;
            ETypePiece t = g.getTypePiece(vc, vr);

            if(dedans.at(v) || t == tpBloque || t == tpReservoir) {
                continue;
            }

            dedans[v] = 1;
            pile << v;
        }
    }

    return n;
}

// Le plus long trace que la recherche sache rendre. On vise l'objectif ; s'il
// depasse ce que la poche permet -- des le niveau 39 -- l'elagage refuse tout
// et la recherche rend UNE case. On redescend alors la visee par dichotomie.
//
// C'est le point 1 du plan, fait ici en dehors du code de production : sans
// lui le prototype n'aurait rien a reecrire au-dela du niveau 39.
int meilleurTrace(Trace &trace, const Game &vierge, int objectif, int plafond) {
    trace.calculer(&vierge, objectif, 400000);

    if(trace.longueur() >= objectif) {
        return trace.longueur();
    }

    int bas = 1, haut = qMin(objectif, plafond);
    Trace garde;
    int meilleure = trace.longueur();

    garde = trace;

    while(bas < haut) {
        int milieu = (bas + haut + 1) / 2;
        Trace essai;

        essai.calculer(&vierge, milieu, 400000);

        if(essai.longueur() >= milieu) {
            bas = milieu;
        } else {
            haut = milieu - 1;
        }

        if(essai.longueur() > meilleure) {
            meilleure = essai.longueur();
            garde = essai;
        }
    }

    trace = garde;

    return meilleure;
}

// --- l'impression d'un schema ----------------------------------------------

void dessiner(const Game &g, const QVector<int> &avant, const QVector<int> &apres,
              const QVector<char> &vivier) {
    int largeur = g.getLargeur(), hauteur = g.getHauteur();
    int cMin = largeur, cMax = -1, rMin = hauteur, rMax = -1;

    for(int i = 0; i < vivier.size(); i++) {
        if(!vivier.at(i)) {
            continue;
        }

        cMin = qMin(cMin, i % largeur); cMax = qMax(cMax, i % largeur);
        rMin = qMin(rMin, i / largeur); rMax = qMax(rMax, i / largeur);
    }

    QVector<int> ordreAvant(g.getSize(), -1), passesApres(g.getSize(), 0);

    for(int k = 0; k < avant.size(); k++) {
        ordreAvant[avant.at(k)] = k;
    }

    for(int k = 0; k < apres.size(); k++) {
        passesApres[apres.at(k)]++;
    }

    printf("      avant (%d passes)            apres (%d passes)\n",
           avant.size(), apres.size());

    for(int row = rMin; row <= rMax; row++) {
        printf("      ");

        for(int col = cMin; col <= cMax; col++) {
            int idx = row * largeur + col;
            char c = '.';

            if(g.getTypePiece(col, row) == tpBloque) {
                c = '#';
            } else if(ordreAvant.at(idx) >= 0) {
                c = 'o';
            } else if(vivier.at(idx)) {
                c = ' ';
            }

            printf("%c ", c);
        }

        printf("     ");

        for(int col = cMin; col <= cMax; col++) {
            int idx = row * largeur + col;
            char c = '.';

            if(g.getTypePiece(col, row) == tpBloque) {
                c = '#';
            } else if(passesApres.at(idx) == 2) {
                c = 'X';
            } else if(passesApres.at(idx) == 1) {
                c = 'o';
            } else if(vivier.at(idx)) {
                c = ' ';
            }

            printf("%c ", c);
        }

        printf("\n");
    }

    printf("\n");
}

// La marche rendue est-elle un TRACE LEGAL ? On ne conclut pas d'un gain sans
// avoir verifie qu'il est jouable : cases voisines deux a deux, jamais plus de
// deux passes sur une case, et sur une case croisee les deux passes droites et
// perpendiculaires -- la croix ne tourne pas.
int verifier(const Game &g, const QVector<int> &chemin, ESens entreeInitiale) {
    int largeur = g.getLargeur();
    int fautes = 0;
    QVector<int> nb(g.getSize(), 0);
    QVector<ESens> e1(g.getSize(), sHaut), s1(g.getSize(), sHaut);

    for(int k = 0; k < chemin.size(); k++) {
        int idx = chemin.at(k);

        if(g.getTypePiece(idx % largeur, idx / largeur) != tpNone) {
            fprintf(stderr, "faute: bloc sous le trace rang %d case (%d,%d)\n",
                    k, idx % largeur, idx / largeur);
            fautes++;
        }

        // Le rang 0 est entre par le RESERVOIR : son entree ne se lit pas sur
        // la case precedente, il n'y en a pas. L'ignorer laissait sa passe
        // inconnue, et une croix posee plus tard sur cette case-la passait
        // pour illegale alors qu'elle ne l'etait pas.
        ESens entree = entreeInitiale, sortie = sHaut;
        bool aEntree = k == 0, aSortie = false;

        if(k > 0) {
            int prec = chemin.at(k - 1);

            for(int s = 0; s < 4; s++) {
                int vc, vr;
                ESens ve;

                Ecoulement::voisine(prec % largeur, prec / largeur, (ESens)s, vc, vr, ve);

                if(vc >= 0 && vc < largeur && vr * largeur + vc == idx) {
                    entree = ve;
                    aEntree = true;
                    break;
                }
            }

            if(!aEntree) {
                fprintf(stderr, "faute: rangs %d et %d ne se touchent pas\n", k - 1, k);
                fautes++;
                continue;
            }
        }

        if(k + 1 < chemin.size()) {
            int suiv = chemin.at(k + 1);

            for(int s = 0; s < 4; s++) {
                int vc, vr;
                ESens ve;

                Ecoulement::voisine(idx % largeur, idx / largeur, (ESens)s, vc, vr, ve);

                if(vc >= 0 && vc < largeur && vr * largeur + vc == suiv) {
                    sortie = (ESens)s;
                    aSortie = true;
                    break;
                }
            }

            if(!aSortie) {
                fautes++;
                continue;
            }
        }

        if(!aEntree || !aSortie) {
            nb[idx]++;
            continue;
        }

        if(Trace::typePour(entree, sortie) == tpNone) {
            fprintf(stderr, "faute: demi-tour rang %d case (%d,%d)\n",
                    k, idx % largeur, idx / largeur);
            fautes++;
        }

        nb[idx]++;

        if(nb.at(idx) == 1) {
            e1[idx] = entree;
            s1[idx] = sortie;
        } else if(nb.at(idx) == 2) {
            // Croix : les deux passes droites, et perpendiculaires.
            if(s1.at(idx) != oppose(e1.at(idx)) || sortie != oppose(entree)
               || estAxeVertical(entree) == estAxeVertical(e1.at(idx))) {
                fprintf(stderr, "faute: croix illegale rang %d case (%d,%d)"
                        " passe1 %d->%d passe2 %d->%d\n", k, idx % largeur, idx / largeur,
                        (int)e1.at(idx), (int)s1.at(idx), (int)entree, (int)sortie);
                fautes++;
            }
        } else {
            fprintf(stderr, "faute: %d passes rang %d case (%d,%d)\n",
                    nb.at(idx), k, idx % largeur, idx / largeur);
            fautes++;
        }
    }

    return fautes;
}

}   // namespace

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 10;
    int niveauMin = argc > 2 ? atoi(argv[2]) : 35;
    int niveauMax = argc > 3 ? atoi(argv[3]) : 38;
    int fenetre = argc > 4 ? atoi(argv[4]) : 10;
    int schemas = argc > 5 ? atoi(argv[5]) : 0;

    printf("fenetre de %d rangs, vivier <= %d cases\n\n", fenetre, VIVIER_MAX);
    printf("niveau  objectif  poche  trace  ->  reecrit   croix  gain  cases+  ms  fautes\n");

    for(int niveau = niveauMin; niveau <= niveauMax; niveau++) {
        double sTrace = 0, sReecrit = 0, sCroix = 0, sCases = 0, sMs = 0, sPoche = 0;
        int objectif = 0;
        int fautes = 0;

        for(int i = 0; i < nb; i++) {
            quint32 graine = 1u + (quint32)i * 2654435761u;
            Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
            p.setNiveauDepart(niveau);
            objectif = p.longueurMinimale();

            Game vierge = denuder(*p.plateau());
            int largeur = vierge.getLargeur();
            QVector<char> dansPoche;
            int taillePoche = poche(vierge, dansPoche);

            Trace trace;
            int longueur = meilleurTrace(trace, vierge, objectif, taillePoche);

            if(longueur < 2) {
                sPoche += taillePoche;
                continue;
            }

            // Le trace en cases, dans l'ordre des rangs.
            QVector<int> chemin;

            for(int r = 0; r < trace.longueur(); r++) {
                chemin << trace.caseAuRang(r);
            }

            QVector<char> surTrace(vierge.getSize(), 0);

            foreach(int idx, chemin) {
                surTrace[idx] = 1;
            }

            QElapsedTimer chrono;
            chrono.start();

            int croixPosees = 0, casesGagnees = 0, montres = 0;

            // JUSQU'AU POINT FIXE. Une reecriture deplace le trace, donc elle
            // ouvre des fenetres que la passe precedente ne pouvait pas voir.
            // On repasse tant que ca rapporte -- ca ne coute rien, la passe
            // entiere se mesure en fractions de milliseconde.
            for(int passe = 0; passe < PASSES_MAX; passe++) {
            int gain = 0;

            // La fenetre glisse rang par rang. On repart du trace COURANT a
            // chaque pas : une reecriture change les rangs suivants.
            for(int debut = 0; debut + 1 < chemin.size(); debut++) {
                int fin = qMin(debut + fenetre - 1, chemin.size() - 1);

                if(fin <= debut) {
                    break;
                }

                // UNE CROIX NE SE COUPE PAS EN DEUX. Des la seconde passe le
                // trace en contient, et une case croisee apparait a DEUX rangs.
                // Si la fenetre n'en attrape qu'un, la recherche locale croit
                // la case libre et peut la reutiliser -- trois passes sur une
                // case, et le trace ne se joue plus. On saute donc toute
                // fenetre qui coupe une croix en deux.
                bool coupee = false;

                for(int r = debut; r <= fin && !coupee; r++) {
                    int idx = chemin.at(r);
                    int dedans = 0, total = 0;

                    for(int k = 0; k < chemin.size(); k++) {
                        if(chemin.at(k) != idx) {
                            continue;
                        }

                        total++;

                        if(k >= debut && k <= fin) {
                            dedans++;
                        }
                    }

                    coupee = dedans != total;
                }

                if(coupee) {
                    continue;
                }

                // Vivier : les cases de la fenetre, plus le terrain libre qui
                // les touche -- c'est la que se logent les croix qui coutent
                // une case.
                Local l;
                l.plateau = &vierge;
                l.largeur = largeur;
                l.hauteur = vierge.getHauteur();
                l.dansVivier.fill(0, vierge.getSize());
                l.passes.fill(0, vierge.getSize());
                l.entree1.fill(sHaut, vierge.getSize());
                l.sortie1.fill(sHaut, vierge.getSize());

                int taille = 0;

                for(int r = debut; r <= fin; r++) {
                    if(!l.dansVivier.at(chemin.at(r))) {
                        l.dansVivier[chemin.at(r)] = 1;
                        taille++;
                    }
                }

                for(int r = debut; r <= fin && taille < VIVIER_MAX; r++) {
                    int idx = chemin.at(r);

                    for(int s = 0; s < 4 && taille < VIVIER_MAX; s++) {
                        int vc, vr;
                        ESens ve;

                        Ecoulement::voisine(idx % largeur, idx / largeur, (ESens)s, vc, vr, ve);

                        if(vc < 0 || vc >= largeur || vr < 0 || vr >= vierge.getHauteur()) {
                            continue;
                        }

                        int v = vr * largeur + vc;

                        if(l.dansVivier.at(v) || surTrace.at(v)
                           || vierge.getTypePiece(vc, vr) != tpNone) {
                            continue;
                        }

                        l.dansVivier[v] = 1;
                        taille++;
                    }
                }

                // Entree dans la fenetre : celle du trace, figee.
                ESens entree = trace.entree(chemin.at(debut) % largeur,
                                            chemin.at(debut) / largeur);

                if(debut > 0) {
                    int prec = chemin.at(debut - 1);
                    int vc, vr;
                    ESens ve;

                    for(int s = 0; s < 4; s++) {
                        Ecoulement::voisine(prec % largeur, prec / largeur, (ESens)s, vc, vr, ve);

                        if(vr * largeur + vc == chemin.at(debut)) {
                            entree = ve;
                            break;
                        }
                    }
                }

                // Sortie : vers le rang suivant, figee elle aussi. En queue de
                // trace, pas de contrainte.
                l.arrivee = -1;

                if(fin + 1 < chemin.size()) {
                    l.arrivee = chemin.at(fin);

                    int suivant = chemin.at(fin + 1);
                    int vc, vr;
                    ESens ve;

                    for(int s = 0; s < 4; s++) {
                        Ecoulement::voisine(l.arrivee % largeur, l.arrivee / largeur,
                                            (ESens)s, vc, vr, ve);

                        if(vr * largeur + vc == suivant) {
                            l.sortieFinale = (ESens)s;
                            break;
                        }
                    }
                }

                l.budget = BUDGET_FENETRE;
                l.visitesMax = fin - debut + 1;   // ce que la fenetre rend deja
                l.croixMax = 0;

                marcher(l, chemin.at(debut), entree, 0, 0);

                if(l.meilleurChemin.isEmpty()) {
                    continue;
                }

                // Combien de croix, et combien de cases neuves ?
                QVector<char> vues(vierge.getSize(), 0);
                int croix = 0, neuves = 0;

                foreach(int idx, l.meilleurChemin) {
                    if(vues.at(idx)) {
                        croix++;
                    } else {
                        vues[idx] = 1;

                        if(!surTrace.at(idx)) {
                            neuves++;
                        }
                    }
                }

                if(schemas > montres) {
                    QVector<int> avant;

                    for(int r = debut; r <= fin; r++) {
                        avant << chemin.at(r);
                    }

                    printf("\n  --- niveau %d, graine %u, rangs %d a %d : +%d passes,"
                           " %d croix, %d case(s) de terrain\n",
                           niveau, graine, debut, fin,
                           (int)l.meilleurChemin.size() - (fin - debut + 1), croix, neuves);
                    dessiner(vierge, avant, l.meilleurChemin, l.dansVivier);
                    montres++;
                }

                gain += (int)l.meilleurChemin.size() - (fin - debut + 1);
                croixPosees += croix;
                casesGagnees += neuves;

                // On recoud : prefixe + fenetre reecrite + suffixe.
                QVector<int> neuf;

                for(int r = 0; r < debut; r++) {
                    neuf << chemin.at(r);
                }

                neuf += l.meilleurChemin;

                for(int r = fin + 1; r < chemin.size(); r++) {
                    neuf << chemin.at(r);
                }

                chemin = neuf;

                foreach(int idx, chemin) {
                    surTrace[idx] = 1;
                }

                // La fenetre a grandi : on reprend apres elle.
                debut += l.meilleurChemin.size() - 1;
            }

            if(gain == 0) {
                break;
            }
            }

            fautes += verifier(vierge, chemin,
                               trace.entree(trace.caseAuRang(0) % largeur,
                                            trace.caseAuRang(0) / largeur));

            sMs += chrono.elapsed();
            sTrace += longueur;
            sReecrit += chemin.size();
            sCroix += croixPosees;
            sCases += casesGagnees;
            sPoche += taillePoche;
        }

        printf("%6d %9d %6.0f %6.1f  -> %8.1f %7.1f %5.1f %7.1f %5.0f %7d\n",
               niveau, objectif, sPoche / nb, sTrace / nb, sReecrit / nb,
               sCroix / nb, (sReecrit - sTrace) / nb, sCases / nb, sMs / nb, fautes);
        fflush(stdout);
    }

    return 0;
}
