#include <QGuiApplication>
#include <QScreen>

#include "common.h"

// Ce que la fenetre demande en hauteur en plus de la grille : barre de titre,
// case a cocher et rangee de boutons. Releve sur la fenetre reelle -- 911 px de
// contenu pour 810 px de grille, plus 32 px de cadre -- et arrondi au-dessus :
// une case un peu trop petite ne se voit pas, un bouton hors de l'ecran si.
#define HAUTEUR_HORS_GRILLE         150

// La largeur, elle, suit entierement la taille de case : la grille, le panneau
// (deux cases) et la jauge de depart (une demi-case). Compte en demi-cases pour
// rester en entiers.
#define DEMI_CASES_EN_LARGEUR       (2 * COLONNES_PLATEAU + 4 + 1)

int tailleCase() {
    // Calcule une fois pour toutes : la fenetre est de taille fixe, une taille
    // de case qui changerait en cours de partie n'aurait nulle part ou aller.
    static int retenue = 0;

    if(retenue > 0) {
        return retenue;
    }

    const QScreen *ecran = QGuiApplication::primaryScreen();

    if(ecran == nullptr) {
        return TAILLE_CASE_MAX;
    }

    // availableGeometry et pas geometry : la barre de menus et le Dock prennent
    // leur part, et c'est ce qui reste qui decide.
    QRect dispo = ecran->availableGeometry();

    int parHauteur = (dispo.height() - HAUTEUR_HORS_GRILLE) / LIGNES_PLATEAU;
    int parLargeur = (2 * dispo.width()) / DEMI_CASES_EN_LARGEUR;

    retenue = qBound(TAILLE_CASE_MIN, qMin(parHauteur, parLargeur), TAILLE_CASE_MAX);

    // Paire : la jauge de depart mesure une demi-case, autant qu'elle tombe sur
    // un pixel entier.
    retenue -= retenue % 2;

    return retenue;
}
