#ifndef JOURNAL_H
#define JOURNAL_H

#include <QFile>
#include <QTextStream>
#include <QVector>
#include "partie.h"

// Journal de gestes : mesurer un joueur -- humain ou bot -- avec le meme
// instrument et la meme horloge que le banc. Une ligne par geste, aucune
// agregation, comme le CSV du banc : les mille lignes sont la donnee, le resume
// se calcule ailleurs.
//
// Le fichier s'ouvre en ajout, donc plusieurs parties s'y accumulent.
class Journal
{
public:
    explicit Journal(const QString &chemin);
    ~Journal();

    bool ouvert() const;

    // Un geste, qu'il ait abouti ou non : un clic sur une case interdite est un
    // geste depense lui aussi, et il compte dans la cadence.
    //
    // `origine` separe l'investissement du travail utile -- croise avec la
    // colonne `remplie`, c'est ce qui dit si une defausse a fini par servir :
    //
    //   0 = pose ordinaire sur le trace, et tout geste humain
    //   1 = defausse selon le plan de defausse
    //   2 = pre-pose de l'anticipation
    //   3 = defausse posee sur un pari
    //   4 = la pose a ecrase ce qui occupait deja la case
    //
    // Le 4 n'est pas une quatrieme intention mais l'aveu d'une ambiguite : la
    // marque du tas dit ce que la case PORTE, pas qui vient de poser. Quand la
    // case n'etait pas vide, on ne peut plus distinguer le trace qui reprend un
    // rebut de la defausse qui ecrase une piece, sauf quand la marque a change
    // -- auquel cas c'est bien une defausse et elle est comptee comme telle.
    void geste(const Partie *partie, float temps, int col, int row,
               bool accepte, bool parLeBot, unsigned char origine = 0);
    // Fin de manche. C'est seulement ici qu'on sait quelles poses ont fini
    // traversees par le flux, donc lesquelles ont servi : le tampon attend
    // jusque-la.
    void finDeManche(const Partie *partie);

private:
    struct Geste {
        int niveau;
        float temps;
        bool parLeBot;
        int col;
        int row;
        bool accepte;
        EEtatPartie etat;
        int avance;
        int distance;
        unsigned char origine;
    };

    QFile fichier;
    QTextStream sortie;
    QVector<Geste> tampon;
    int manche = 0;
    int dernierCol = -1;
    int dernierRow = -1;
};

#endif // JOURNAL_H
