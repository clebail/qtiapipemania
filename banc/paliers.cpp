// Sonde des paliers de points : compare la regle a cliquet (le palier franchi
// est memorise) a la regle naive (on regarde floor(score / 20000) a chaque
// changement de score). L'ecart tient aux ecrasements, qui font redescendre le
// score de 25 points chacun -- donc repasser sous un palier deja paye est
// possible, et le repasser au-dessus redonnerait une vie a la regle naive.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botspaceanticp.h"

#define PALIER 20000

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

        int precedent = 0;        // floor(score / PALIER) au pas d'avant
        int naif = 0;             // vies de la regle naive
        int cliquet = 0;          // vies de la regle a cliquet
        int redescentes = 0;      // fois ou le score repasse sous un palier paye

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

            int courant = p.score() / PALIER;

            if(courant > precedent) {
                naif += courant - precedent;
            }

            if(courant < precedent) {
                redescentes++;
            }

            if(courant > cliquet) {
                cliquet = courant;
            }

            precedent = courant;

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }

        printf("%d,%d,%d,%d,%d,%d\n", i + offset, niveau, p.score(), cliquet, naif, redescentes);
    }

    return 0;
}
