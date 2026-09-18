// Sonde : une ligne CSV par manche jouee (partie, niveau, traversees, score).
// Meme boucle que banc/bench.cpp -- dt = 16 ms, acceleration x8 quand le bot
// fonce -- pour que les chiffres restent comparables aux bancs existants.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"



int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 400;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;

    printf("partie,niveau,traversees,score\n");

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

            EEtatPartie apres = p.etat();

            if(avant != apres && (apres == epReussie || apres == epPerdue || apres == epGameOver)) {
                printf("%d,%d,%d,%d\n", i, niveau, p.casesTraversees(), p.score());
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }
    }

    return 0;
}
