// Sonde : ce que le gabarit de Trace donne sur de vrais plateaux.
//   tracer <parties> <graine> <niveau>
// Une ligne par plateau : objectif, longueur atteignable, couverture du terrain
// libre. Puis l'histogramme des types sur l'ensemble -- c'est lui qui dit si le
// trace gaspillera des pieces (TRACE.md, §3).
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#include <QElapsedTimer>
#include "partie.h"
#include "trace.h"

int tailleCase() { return 32; }

static const char *NOM[] = {
    "none", "reservoir", "horizontal", "vertical", "coudeHG", "coudeHD",
    "coudeBG", "coudeBD", "croix", "bombe", "bloque"
};

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 20;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;
    int niveau = argc > 3 ? atoi(argv[3]) : 35;
    long budget = argc > 4 ? atol(argv[4]) : 400000;

    QVector<int> total(tpBloque + 1, 0);
    int atteints = 0;
    double sommeCouverture = 0.0;
    qint64 sommeMs = 0;

    printf("objectif  atteint  libres  couverture      ms\n");

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        p.setNiveauDepart(niveau);

        int libres = 0;

        for(int k = 0; k < p.plateau()->getSize(); k++) {
            if(p.plateau()->getTypePiece(k % p.getLargeur(), k / p.getLargeur()) == tpNone) {
                libres++;
            }
        }

        int objectif = p.longueurMinimale();

        // On vise l'objectif et rien de plus : la recherche s'arrete des
        // qu'elle l'atteint. Demander la couverture maximale est un tout autre
        // probleme, bien plus dur, et le jeu ne la demande pas.
        QElapsedTimer horloge;
        horloge.start();

        Trace t;
        t.calculer(p.plateau(), objectif, budget);

        qint64 ms = horloge.elapsed();
        sommeMs += ms;
        double couverture = libres > 0 ? 100.0 * t.longueur() / libres : 0.0;

        sommeCouverture += couverture;

        if(t.longueur() >= objectif) {
            atteints++;
        }

        if(t.longueur() < objectif) {
            printf("  ^ graine %u\n", (unsigned)graine);
        }

        printf("%8d %8d %7d %9.1f %% %7lld%s\n", objectif, t.longueur(), libres,
               couverture, (long long)ms,
               t.longueur() >= objectif ? "" : "   <-- trop court");

        QVector<int> h = t.histogramme();

        for(int k = 0; k <= tpBloque; k++) {
            total[k] += h.at(k);
        }
    }

    printf("\nobjectif atteint : %d / %d\n", atteints, nb);
    printf("couverture moyenne du terrain libre : %.1f %%\n", sommeCouverture / nb);
    printf("temps moyen par plateau : %lld ms\n\n", (long long)(sommeMs / nb));

    int somme = 0;

    for(int k = 0; k <= tpBloque; k++) {
        somme += total[k];
    }

    // L'ideal n'est PAS plat : la croix traverse tout droit, donc elle se pose
    // partout ou un droit est demande. Les sept types tires se repartissent sur
    // six reclames -- 3/7 pour les deux droits ensemble, 1/7 par coude.
    printf("histogramme des types reclames (ideal : droits 21,4 %%, coudes 14,3 %%)\n");

    for(int k = 0; k <= tpBloque; k++) {
        if(total[k] > 0) {
            printf("  %-12s %7d  %5.1f %%\n", NOM[k], total[k],
                   somme > 0 ? 100.0 * total[k] / somme : 0.0);
        }
    }

    return 0;
}
