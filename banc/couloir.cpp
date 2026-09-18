// Couloir oblige, version DIRECTIONS.
//
// L'obligation ne porte pas sur le type mais sur le routage : la croix traverse
// tout droit (Ecoulement::sorties), exactement comme le tuyau droit de son axe.
// Deux types, un seul chemin. Compter les types faisait donc voir un choix la
// ou il n'y en a pas -- et un desaccord plan/couloir la ou les deux envoient le
// flux au meme endroit.
//
// Un releve par GESTE, jamais par tick : sinon la meme position est comptee une
// trentaine de fois.
#include <QtGlobal>
#include <QVector>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

int tailleCase() { return 32; }

struct Maillon {
    int col;
    int row;
    ESens entree;
    ESens sortie;       // la seule direction possible
};

// Les sorties distinctes qu'offre cette case, parmi les types qui ne la
// condamnent pas. Une seule = le chemin est subi, pas choisi.
static QVector<ESens> sortiesVivantes(const BotSpaceAnticp &bot, int col, int row,
                                      ESens entree, bool strict) {
    QVector<ESens> distinctes;

    foreach(ETypePiece t, Ecoulement::piecesCompatibles(entree)) {
        if(bot.meneALaMort(t, col, row, entree)) {
            continue;
        }

        if(strict && bot.culDeSac(t, col, row, entree)) {
            continue;
        }

        QVector<ESens> so = Ecoulement::sorties(t, sHaut, entree);

        if(so.isEmpty()) {
            continue;
        }

        if(!distinctes.contains(so.first())) {
            distinctes << so.first();
        }
    }

    return distinctes;
}

static QVector<Maillon> couloir(const BotSpaceAnticp &bot, const Partie &p,
                                int col, int row, ESens entree, bool strict) {
    QVector<Maillon> chaine;
    Game *plateau = p.plateau();

    for(int garde = 0; garde < 64; garde++) {
        if(col < 0 || col >= p.getLargeur() || row < 0 || row >= p.getHauteur()
           || plateau->getTypePiece(col, row) != tpNone) {
            break;
        }

        QVector<ESens> so = sortiesVivantes(bot, col, row, entree, strict);

        if(so.size() != 1) {
            break;
        }

        chaine << Maillon{col, row, entree, so.first()};
        Ecoulement::voisine(col, row, so.first(), col, row, entree);
    }

    return chaine;
}

// Le type route-t-il le flux vers `sortie` depuis `entree` ?
static bool route(ETypePiece t, ESens entree, ESens sortie) {
    if(t == tpNone || !Ecoulement::piecesCompatibles(entree).contains(t)) {
        return false;
    }

    QVector<ESens> so = Ecoulement::sorties(t, sHaut, entree);

    return !so.isEmpty() && so.first() == sortie;
}

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 120;
    int niveau = argc > 2 ? atoi(argv[2]) : 26;
    bool strict = argc > 3;

    long gestes = 0, histo[12] = {0}, sommeLongueur = 0, avecCouloir = 0;
    long mTete = 0, mTeteAccord = 0, mTeteMuet = 0;
    long mSuite = 0, mSuiteAccord = 0, mSuiteMuet = 0;
    long piegeOuvert = 0, piegeArme = 0;
    long placable = 0, placableHorsTete = 0;
    // Ce que le bot a REELLEMENT fait : le geste est-il tombe sur une case
    // obligee hors tete, et l'a-t-il routee comme il fallait ?
    long poseSurCouloir = 0, poseScellante = 0, poseConforme = 0;

    for(int i = 0; i < nb; i++) {
        quint32 graine = 533828732u + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        p.setNiveauDepart(niveau);

        BotSpaceAnticp bot(&p, 2.0f, p.getGraine());
        float dt = 16.0f / 1000.0f;
        bool fonce = false;

        for(long tick = 0; tick < 400000; tick++) {
            if(p.etat() != epAttente && p.etat() != epEcoulement) {
                break;
            }

            int col, row;
            ESens entree;
            Piece avant = p.file()->getPiece(0);
            bool aTete = bot.tete(col, row, entree);

            QVector<Maillon> ch;

            if(aTete) {
                ch = couloir(bot, p, col, row, entree, strict);
            }

            Game copie(*p.plateau());

            int facteur = (p.etat() == epEcoulement && fonce) ? 8 : 1;

            for(int pas = facteur; pas > 0; pas--) {
                if(facteur == 1) {
                    bot.avancer(dt);

                    if(bot.veutFoncer() && !fonce) {
                        p.lancerFluxAnticipe();
                        fonce = true;
                    }
                }

                p.avancer(dt);
            }

            Piece apres = p.file()->getPiece(0);

            // Rien n'a bouge : ce n'etait pas un geste, on ne releve pas.
            if(!aTete || (apres.type == avant.type && apres.sens == avant.sens)) {
                continue;
            }

            gestes++;
            histo[qMin(11, ch.size())]++;
            sommeLongueur += ch.size();

            if(ch.isEmpty()) {
                continue;
            }

            avecCouloir++;

            bool ok = false, okHorsTete = false;

            for(int k = 0; k < ch.size(); k++) {
                const Maillon &m = ch.at(k);
                ETypePiece pt = bot.planType(m.col, m.row);
                bool accord = route(pt, m.entree, m.sortie);

                if(route(avant.type, m.entree, m.sortie)) {
                    ok = true;

                    if(k > 0) {
                        okHorsTete = true;
                    }
                }

                if(k == 0) {
                    mTete++;
                    if(pt == tpNone) mTeteMuet++;
                    else if(accord)  mTeteAccord++;
                    continue;
                }

                mSuite++;

                if(pt == tpNone) {
                    mSuiteMuet++;
                } else if(accord) {
                    mSuiteAccord++;
                } else {
                    // Le plan reclame ici un type qui envoie le flux ailleurs :
                    // l'y defausser scellerait le seul passage vivant.
                    piegeOuvert++;

                    if(pt == avant.type) {
                        piegeArme++;
                    }
                }
            }

            if(ok)         placable++;
            if(okHorsTete) placableHorsTete++;

            // Ou le geste est-il tombe ? On le lit par difference, comme le fait
            // MainWindow pour le journal.
            for(int idx = 0; idx < copie.getSize(); idx++) {
                int cx = idx % copie.getLargeur();
                int cy = idx / copie.getLargeur();

                if(copie.getTypePiece(cx, cy) == p.plateau()->getTypePiece(cx, cy)
                   && copie.getSens(cx, cy) == p.plateau()->getSens(cx, cy)) {
                    continue;
                }

                for(int k = 1; k < ch.size(); k++) {
                    if(ch.at(k).col != cx || ch.at(k).row != cy) {
                        continue;
                    }

                    poseSurCouloir++;

                    if(route(p.plateau()->getTypePiece(cx, cy),
                             ch.at(k).entree, ch.at(k).sortie)) {
                        poseConforme++;
                    } else {
                        poseScellante++;
                    }
                }

                break;
            }
        }
    }

    printf("\n=== couloir oblige, par DIRECTION (%s) -- niveau %d, %d manches ===\n\n",
           strict ? "strict" : "lache", niveau, nb);
    printf("gestes observes            : %ld\n", gestes);
    printf("  avec couloir (>= 1)      : %ld  (%.1f %%)\n",
           avecCouloir, 100.0 * avecCouloir / (gestes ? gestes : 1));
    printf("  longueur moyenne         : %.2f cases\n\n",
           (double)sommeLongueur / (gestes ? gestes : 1));

    for(int k = 0; k <= 11; k++) {
        if(histo[k] == 0) {
            continue;
        }

        printf("  longueur %2d%s : %6ld  (%4.1f %%)  ", k, k == 11 ? "+" : " ",
               histo[k], 100.0 * histo[k] / (gestes ? gestes : 1));

        for(int b = 0; b < (int)(50.0 * histo[k] / (gestes ? gestes : 1)); b++) {
            printf("#");
        }

        printf("\n");
    }

    printf("\n--- le plan route-t-il comme le couloir l'exige ? ---\n");
    printf("  sur la tete : %7ld cases   accord %5.1f %%   muet %4.1f %%   DESACCORD %5.1f %%\n",
           mTete, 100.0 * mTeteAccord / (mTete ? mTete : 1),
           100.0 * mTeteMuet / (mTete ? mTete : 1),
           100.0 * (mTete - mTeteAccord - mTeteMuet) / (mTete ? mTete : 1));
    printf("  au-dela     : %7ld cases   accord %5.1f %%   muet %4.1f %%   DESACCORD %5.1f %%\n",
           mSuite, 100.0 * mSuiteAccord / (mSuite ? mSuite : 1),
           100.0 * mSuiteMuet / (mSuite ? mSuite : 1),
           100.0 * (mSuite - mSuiteAccord - mSuiteMuet) / (mSuite ? mSuite : 1));

    printf("\n  piege ouvert (plan route ailleurs, hors tete) : %ld\n", piegeOuvert);
    printf("  piege arme   (et c'est le haut de file)       : %ld sur %ld gestes  (%.2f %%)\n",
           piegeArme, gestes, 100.0 * piegeArme / (gestes ? gestes : 1));

    printf("\n  --- ce que le bot a REELLEMENT pose ---\n");
    printf("    gestes tombes sur une case obligee hors tete : %ld  (%.2f %%)\n",
           poseSurCouloir, 100.0 * poseSurCouloir / (gestes ? gestes : 1));
    printf("      routee comme exige (construction gratuite) : %ld\n", poseConforme);
    printf("      SCELLE le passage (manche condamnee)       : %ld  (%.3f %% des gestes)\n",
           poseScellante, 100.0 * poseScellante / (gestes ? gestes : 1));

    printf("\n  haut de file routant comme exige :\n");
    printf("    n'importe ou             : %ld  (%.1f %%)\n",
           placable, 100.0 * placable / (gestes ? gestes : 1));
    printf("    ailleurs que sur la tete : %ld  (%.1f %%)\n",
           placableHorsTete, 100.0 * placableHorsTete / (gestes ? gestes : 1));

    return 0;
}
