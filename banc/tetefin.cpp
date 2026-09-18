// Etat de la tete a l'instant precis ou le flux part au bout du delai.
// Sortie : partie, niveau, objectif, tracee, cas
//   0 = pas de tete, 1 = tete condamnee, 2 = prolongeable mais sans issue utile
//   (ouvertureUtile == 0), 3 = issue utile (le bot attend une bonne piece).
// Les primitives sont protegees : on les expose par heritage, et on ne les
// appelle qu'UNE fois par manche, au depart du flux, pour ne pas devier la
// partie (tete() convertit des cases du tas).
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

class Sonde : public BotSpaceAnticp {
public:
    using BotSpaceAnticp::BotSpaceAnticp;
    using BotSpaceAnticp::tete;
    using BotSpaceAnticp::teteCondamnee;
    using BotSpaceAnticp::ouvertureUtile;
    using BotSpaceAnticp::espaceApres;
};

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 400;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;
    int offset = argc > 3 ? atoi(argv[3]) : 0;

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        Sonde bot(&p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie avant = p.etat();

            // Avec les vies, epPerdue ne finit plus la partie : il coute une
            // vie et rejoue le meme niveau. Seuls epGameOver et epAbandon
            // arretent la boucle.
            if(avant == epGameOver || avant == epAbandon) {
                break;
            }

            if(avant != epEcoulement) {
                fonce = false;
            }

            int facteur = (avant == epEcoulement && fonce) ? 8 : 1;
            bool espace = false;

            for(int pas = facteur; pas > 0; pas--) {
                if(facteur == 1) {
                    bot.avancer(dt);

                    if(bot.veutFoncer() && !fonce) {
                        espace = (p.etat() == epAttente);
                        p.lancerFluxAnticipe();
                        p.passerLaSuite();
                        fonce = true;
                    }
                }

                p.avancer(dt);
            }

            if(avant == epAttente && p.etat() != epAttente && !espace) {
                int col, row;
                ESens entree;
                int cas;

                if(!bot.tete(col, row, entree)) {
                    cas = 0;
                } else if(bot.teteCondamnee(col, row, entree)) {
                    cas = 1;
                } else if(bot.ouvertureUtile(col, row, entree) == 0) {
                    cas = 2;
                } else {
                    cas = 3;
                }

                // Place reellement atteignable devant la tete, non bornee : la
                // meilleure des poses qui ne tuent pas sur le coup. C'est la
                // mesure que culDeSac cesse de faire une fois l'objectif acquis.
                int place = 0;

                if(cas != 0) {
                    foreach(ETypePiece t, Ecoulement::piecesCompatibles(entree)) {
                        if(bot.meneALaMort(t, col, row, entree)) {
                            continue;
                        }

                        int e = bot.espaceApres(t, col, row, entree, 0);

                        if(e > place) {
                            place = e;
                        }
                    }
                }

                printf("%d,%d,%d,%d,%d,%d\n", i + offset, niveau, p.longueurMinimale(),
                       p.longueurTracee(), cas, place);
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }
    }

    return 0;
}
