// Banc : parties completes, niveau atteint a la defaite. Reproduit la boucle de
// MainWindow (dt = 16 ms, cadence 2 gestes/s, acceleration x8 quand le bot fonce).
//
//   bench <parties> <graine> [bot]
//
// Le bot est celui de BotFactory, "spaceAnticp" par defaut : c'est lui qui a
// produit banc/reference-400.txt, et une valeur par defaut qui change rendrait
// les bras d'une comparaison appariee incomparables sans qu'on le voie.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "botfactory.h"

int tailleCase() { return 32; }

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 300;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;
    QString nomBot = argc > 3 ? QString::fromLocal8Bit(argv[3]) : QString("spaceAnticp");

    if(!BotFactory::noms().contains(nomBot)) {
        fprintf(stderr, "bot inconnu : %s (connus : %s)\n", qPrintable(nomBot),
                qPrintable(BotFactory::noms().join(", ")));
        return 1;
    }

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        Bot *bot = BotFactory::createInstance(nomBot, &p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;
        int score = 0;

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie avant = p.etat();

            // Avec les vies, epPerdue ne finit plus la partie : il coute une
            // vie et rejoue le meme niveau. Seuls epGameOver et epAbandon --
            // le bot qui jette l'eponge sur un rejeu en boucle -- arretent tout.
            if(avant == epGameOver || avant == epAbandon) {
                break;
            }

            if(avant != epEcoulement) {
                fonce = false;
            }

            int facteur = (avant == epEcoulement && fonce) ? 8 : 1;

            for(int pas = facteur; pas > 0; pas--) {
                if(facteur == 1) {
                    bot->avancer(dt);

                    if(bot->veutFoncer() && !fonce) {
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

            // Le score ne redescend qu'a la partie neuve : on garde le dernier
            // vu, celui de la fin de partie.
            score = p.score();
        }

        delete bot;
        // Deux colonnes : le niveau atteint, et le score final. analyse.py ne
        // lit que la premiere, les fichiers a une colonne restent comparables.
        printf("%d %d\n", niveau, score);
        fflush(stdout);
    }

    return 0;
}
