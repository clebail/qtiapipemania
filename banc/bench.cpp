// Banc : parties completes, niveau atteint a la defaite. Reproduit la boucle de
// MainWindow (dt = 16 ms, cadence 2 gestes/s, acceleration x8 quand le bot fonce).
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

int tailleCase() { return 32; }

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 300;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        BotSpaceAnticp bot(&p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie avant = p.etat();

            // Avec les vies, epPerdue ne finit plus la partie : il coute une
            // vie et rejoue le meme niveau. Seul epGameOver arrete la boucle.
            if(avant == epGameOver) {
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

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }

        printf("%d\n", niveau);
    }

    return 0;
}
