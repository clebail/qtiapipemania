// Verification du moteur de vies : une ligne par manche.
// niveau, objectif, traversees, etat (R/P/G), vies restantes, score, graine du
// plateau -- la graine dit si le rejeu redonne bien la meme manche.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 1;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        BotSpaceAnticp bot(&p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;

        printf("=== partie %d (graine %u)\n", i, graine);

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie avant = p.etat();

            if(avant == epGameOver || avant == epAbandon) {
                break;
            }

            if(avant != epEcoulement) {
                fonce = false;
            }

            int facteur = (avant == epEcoulement && fonce) ? 8 : 1;

            for(int pas = facteur; pas > 0; pas--) {
                if(facteur == 1) {
                    bot.avancer(dt);

                    if(bot.veutFoncer() && !fonce) {
                        p.lancerFluxAnticipe();
                        p.passerLaSuite();
                        fonce = true;
                    }
                }

                p.avancer(dt);
            }

            EEtatPartie apres = p.etat();

            if(avant != apres && (apres == epReussie || apres == epPerdue
                                  || apres == epGameOver || apres == epAbandon)) {
                printf("niveau %2d  objectif %3d  traversees %3d  %s  vies %d  score %6d  plateau %u\n",
                       niveau, p.longueurMinimale(), p.casesTraversees(),
                       apres == epReussie ? "REUSSIE " : (apres == epPerdue ? "perdue  " : "GAMEOVER"),
                       p.vies(), p.score(), p.plateau()->getGraine());
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }
    }

    return 0;
}
