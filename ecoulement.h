#ifndef ECOULEMENT_H
#define ECOULEMENT_H

#include <QVector>
#include "game.h"
#include "common.h"

class Ecoulement
{
public:
    Ecoulement(const Game *plateau);
    ~Ecoulement();

    void reinitialiser();
    void demarrer();
    EEtat avancer(float dt);
    void setDureeRemplissage(float secondes);
    bool enCours() const;
    int nbCasesTraversees() const;
    bool estRempli(int col, int row) const;
    float progression(int col, int row) const;
    ESens entree(int col, int row) const;
    // Publique et statique : le rendu en a besoin pour connaitre les sorties
    // d'une case. Si un troisieme utilisateur apparait, l'extraire dans un
    // module de geometrie de tuyaux.
    static QVector<ESens> ouvertures(const ETypePiece& typePiece, const ESens& sens);
private:
    const Game *plateau;
    unsigned char* remplis = nullptr;
    float* progressions = nullptr;
    unsigned char* entrees = nullptr;
    QVector<int> front;
    int casesTraversees = 0;
    // Secondes pour remplir une case. Baisser cette valeur accelere
    // l'ecoulement, sans rien changer d'autre au reste du modele.
    float dureeRemplissage = 0.3f;
};

#endif // ECOULEMENT_H
