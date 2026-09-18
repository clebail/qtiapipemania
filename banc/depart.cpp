// Sonde du depart du flux : la prime de depart anticipe est-elle laissee sur la
// table ? Une ligne par manche.
//   niveau, objectif, tracee, cause (1 = barre d'espace du bot, 0 = delai
//   ecoule), secondes d'attente pendant lesquelles la trace n'a plus grandi.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 400;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;
    int offset = argc > 3 ? atoi(argv[3]) : 0;

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        BotSpaceAnticp bot(&p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;

        int traceeVue = -1;
        float figeeDepuis = 0.0f;   // secondes depuis le dernier allongement

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

            if(avant == epAttente) {
                int t = p.longueurTracee();

                if(t != traceeVue) {
                    traceeVue = t;
                    figeeDepuis = 0.0f;
                } else {
                    figeeDepuis += dt;
                }
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

            if(avant == epAttente && p.etat() != epAttente) {
                printf("%d,%d,%d,%d,%d,%d\n", i + offset, niveau, p.longueurMinimale(),
                       traceeVue, espace ? 1 : 0, (int)(figeeDepuis * 100.0f + 0.5f));
                traceeVue = -1;
                figeeDepuis = 0.0f;
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }
    }

    return 0;
}
