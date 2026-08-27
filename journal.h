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
    void geste(const Partie *partie, float temps, int col, int row,
               bool accepte, bool parLeBot);
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
    };

    QFile fichier;
    QTextStream sortie;
    QVector<Geste> tampon;
    int manche = 0;
    int dernierCol = -1;
    int dernierRow = -1;
};

#endif // JOURNAL_H
