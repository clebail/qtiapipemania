// Sonde : le journal de gestes (journal.h) sans fenetre ni ecran.
//
//   gestes <parties> <graine> <niveau> <bombes> <fichier.csv> [bot]
//
// Meme boucle que banc/bench.cpp -- dt = 16 ms, cadence 2 gestes/s,
// acceleration x8 quand le bot fonce -- pour que les lignes restent
// comparables aux bancs existants ET a ce qu'on voit a l'ecran.
//
// La raison d'etre : une prise a l'ecran coute une heure d'horloge pour
// atteindre le niveau ou le plan de defausse se joue, et il faut cliquer un
// bouton pour installer le bot. Ici la partie se deroule aussi vite que la
// machine calcule, et le niveau de depart est un argument.
//
// Le releve des gestes est celui de MainWindow::battement, recopie : le bot ne
// dit pas ou il pose, on le lit sur le plateau par difference. Et les marques
// du tas sont relevees AVANT le geste, parce que origineTas dit ce que la case
// PORTE et non qui vient de poser -- sans l'avant, le trace qui reprend un
// rebut se compterait comme une defausse. Voir journal.h pour les codes.
//
// Compilation, depuis un build deja fait (les .o y sont) :
//
//   O=build/cli
//   g++ -O2 -std=gnu++1z -fPIC -I. -I/usr/include/x86_64-linux-gnu/qt6
//       -I/usr/include/x86_64-linux-gnu/qt6/QtCore banc/gestes.cpp
//       $O/{journal,partie,game,ecoulement,piecefile,minage}.o
//       $O/{bot,botspace,botspaceanticp,botmemoire}.o -lQt6Core -o $O/gestes
//
// (une seule ligne de commande : les retours a la ligne sont pour la lecture)
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include "partie.h"
#include "journal.h"
#include "botfactory.h"

// Le banc ne dessine rien : la taille de case n'est la que pour satisfaire
// l'edition de liens, comme dans bench.cpp.
int tailleCase() { return 32; }

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 20;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;
    int niveau = argc > 3 ? atoi(argv[3]) : 35;
    int bombes = argc > 4 ? atoi(argv[4]) : BOMBES_MAX;
    const char *chemin = argc > 5 ? argv[5] : "mesure.csv";
    // Le bot est un argument depuis qu'il y en a deux a comparer sur le meme
    // instrument : le taux servi du §2 de TRACE.md se lit a +/- 0,2 point, donc
    // il ne supporte pas qu'on recompile entre les deux bras.
    QString nomBot = argc > 6 ? QString::fromLocal8Bit(argv[6]) : QString("memoire");

    if(!BotFactory::noms().contains(nomBot)) {
        fprintf(stderr, "bot inconnu : %s (connus : %s)\n", qPrintable(nomBot),
                qPrintable(BotFactory::noms().join(", ")));
        return 1;
    }

    // Le journal s'ouvre en AJOUT : deux lancements s'accumulent dans le meme
    // fichier. C'est voulu cote Journal, mais pour une campagne on veut un
    // fichier neuf -- a effacer avant, le banc ne le fait pas a ta place.
    Journal journal(QString::fromLocal8Bit(chemin));

    if(!journal.ouvert()) {
        fprintf(stderr, "journal impossible a ouvrir : %s\n", chemin);
        return 1;
    }

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);

        // Le stock d'abord, le niveau ensuite : setNiveauDepart relance la
        // manche, et c'est elle qu'on veut voir partir avec les bombes.
        p.setBombesDepart(bombes);
        p.setNiveauDepart(niveau);

        // Apres les reglages : le bot lit la graine de la partie a sa naissance.
        Bot *b = BotFactory::createInstance(nomBot, &p, 2.0f, p.getGraine());
        Bot &bot = *b;

        float dt = 16.0f / 1000.0f;
        float temps = 0.0f;
        bool fonce = false;
        int atteint = niveau;

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie etatAvant = p.etat();

            if(etatAvant == epGameOver || etatAvant == epAbandon) {
                break;
            }

            if(etatAvant != epEcoulement) {
                fonce = false;
            }

            int facteur = (etatAvant == epEcoulement && fonce) ? 8 : 1;

            for(int pas = facteur; pas > 0; pas--) {
                temps += dt;

                if(facteur == 1) {
                    Game plateauAvant(*p.plateau());
                    QVector<unsigned char> tasAvant(plateauAvant.getSize());

                    for(int k = 0; k < plateauAvant.getSize(); k++) {
                        tasAvant[k] = bot.origineTas(k % plateauAvant.getLargeur(),
                                                     k / plateauAvant.getLargeur());
                    }

                    Piece sommet = p.file()->getPiece(0);

                    bot.avancer(dt);

                    Piece suivante = p.file()->getPiece(0);

                    // La file a bouge, donc une piece a ete posee.
                    if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
                        for(int k = 0; k < plateauAvant.getSize(); k++) {
                            int col = k % plateauAvant.getLargeur();
                            int row = k / plateauAvant.getLargeur();

                            if(plateauAvant.getTypePiece(col, row) == p.plateau()->getTypePiece(col, row)
                               && plateauAvant.getSens(col, row) == p.plateau()->getSens(col, row)) {
                                continue;
                            }

                            unsigned char apres = bot.origineTas(col, row);
                            unsigned char origine;

                            if(plateauAvant.getTypePiece(col, row) == tpNone) {
                                // Case vierge : la marque ne peut venir que de
                                // la pose qu'on consigne.
                                origine = apres;
                            } else if(apres != 0 && apres != tasAvant.at(k)) {
                                // La marque a change : une defausse a ecrase
                                // une piece, et c'est la defausse qu'on compte.
                                origine = apres;
                            } else {
                                // Marque inchangee sur une case occupee : le
                                // trace reprend un rebut, ou une pose en
                                // remplace une autre.
                                origine = 4;
                            }

                            journal.geste(&p, temps, col, row, true, true, origine);
                            break;
                        }
                    }

                    if(bot.veutFoncer() && !fonce) {
                        p.lancerFluxAnticipe();
                        p.passerLaSuite();
                        fonce = true;
                    }
                }

                p.avancer(dt);
            }

            // Fin de manche : c'est seulement la que le journal sait quels
            // gestes ont servi.
            if(p.etat() != etatAvant
               && (p.etat() == epReussie || p.etat() == epPerdue
                   || p.etat() == epGameOver || p.etat() == epAbandon)) {
                journal.finDeManche(&p);
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                atteint = p.niveau();
            }
        }

        // Sur la sortie standard, une ligne par partie : le niveau atteint,
        // comme bench.cpp. Le CSV, lui, est dans le journal.
        delete b;

        printf("%d\n", atteint);
        fflush(stdout);
    }

    return 0;
}
