#include "journal.h"

Journal::Journal(const QString &chemin) : fichier(chemin) {
    bool neuf = !fichier.exists() || fichier.size() == 0;

    if(!fichier.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    sortie.setDevice(&fichier);

    if(neuf) {
        sortie << "manche,niveau,temps,source,col,row,accepte,etat,avance,distance,remplie\n";
    }
}

Journal::~Journal() {
    sortie.flush();
}

bool Journal::ouvert() const {
    return fichier.isOpen();
}

void Journal::geste(const Partie *partie, float temps, int col, int row,
                    bool accepte, bool parLeBot) {
    Geste g;

    g.niveau = partie->niveau();
    g.temps = temps;
    g.parLeBot = parLeBot;
    g.col = col;
    g.row = row;
    g.accepte = accepte;
    g.etat = partie->etat();
    // Cases construites devant le flux : la marge du joueur. C'est elle qui
    // permet de lire une cadence sous pression au lieu d'une moyenne plate --
    // les moments de panique sont les gestes a faible avance, on n'a pas a les
    // reperer a la main.
    g.avance = partie->longueurTracee() - partie->casesTraversees();
    // Distance de Manhattan au geste precedent, en cases : le cout physique du
    // deplacement, que le bot ne paie jamais et que le modele compte parmi ses
    // sources d'optimisme.
    g.distance = (dernierCol < 0) ? 0 : qAbs(col - dernierCol) + qAbs(row - dernierRow);

    dernierCol = col;
    dernierRow = row;

    tampon.append(g);
}

void Journal::finDeManche(const Partie *partie) {
    if(!fichier.isOpen()) {
        tampon.clear();
        return;
    }

    manche++;

    foreach(const Geste& g, tampon) {
        // Un geste a servi si sa case a fini traversee. C'est le flux qui
        // tranche, pas l'intention -- et on ne le sait qu'a la fin. Une case
        // reecrite compte pour chacune des poses qui l'ont visee, ce qui est
        // volontaire : chaque pose etait bien un geste.
        bool remplie = g.accepte && partie->ecoulement()->estRempli(g.col, g.row);

        sortie << manche << "," << g.niveau << ","
               << QString::number(g.temps, 'f', 3) << ","
               << (g.parLeBot ? "bot" : "humain") << ","
               << g.col << "," << g.row << ","
               << (g.accepte ? 1 : 0) << ","
               << (g.etat == epAttente ? "attente" : "flux") << ","
               << g.avance << "," << g.distance << ","
               << (remplie ? 1 : 0) << "\n";
    }

    sortie.flush();
    tampon.clear();
    dernierCol = -1;
    dernierRow = -1;
}
