// Sonde de la barre d'espace. Pendant l'ATTENTE, combien de temps la condition
// "objectif atteint ET tete condamnee" tient-elle sans que le bot fonce ? C'est
// la mesure du retard entre l'apparition de l'etat et l'appui. Releve de
// reference : banc/espace-60.csv (60 parties, 2 761 manches).
//
// tete() mute `tas` : la sonde le sauve et le restaure autour de chaque appel,
// elle ne devie donc pas la partie. C'est ce qui vaut le `#define private
// protected` -- une sonde, pas un bot.
//   partie, niveau, objectif, tracee-au-depart, espace(1/0),
//   centiemes ou (objectif atteint && tete condamnee),
//   centiemes ou (objectif atteint && place <= SEUIL),
//   centiemes ou (objectif atteint && aucun pont dans la file),
//   centiemes d'attente au total
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#define private protected
#include "partie.h"
#include "botmemoire.h"
#undef private

#ifndef SEUIL_PLACE
#define SEUIL_PLACE 3
#endif

class Sonde : public BOT_CLASSE {
public:
    using BOT_CLASSE::BOT_CLASSE;

    // Sans effet de bord : snapshot/restore du tas autour de tete().
    void observer(bool &bloquee, int &place, bool &pontDispo) {
        QVector<unsigned char> sauve = tas;

        int col, row;
        ESens entree;

        bloquee = true;
        place = 0;
        pontDispo = false;

        if(tete(col, row, entree)) {
            foreach(ETypePiece t, Ecoulement::piecesCompatibles(entree)) {
                if(meneALaMort(t, col, row, entree)) {
                    continue;
                }

                bloquee = false;

                int e = espaceApres(t, col, row, entree, 0);

                if(e > place) {
                    place = e;
                }
            }

            for(int r = 0; r < p->file()->getTaille(); r++) {
                Piece f = p->file()->getPiece(r);

                if(Ecoulement::piecesCompatibles(entree).contains(f.type)
                   && !meneALaMort(f.type, col, row, entree)) {
                    pontDispo = true;
                    break;
                }
            }
        }

        tas = sauve;
    }

    int restant() const { return objectifRestant(); }
};

int main(int argc, char *argv[]) {
    int nb = argc > 1 ? atoi(argv[1]) : 200;
    quint32 base = argc > 2 ? (quint32)strtoul(argv[2], nullptr, 10) : 1u;

    for(int i = 0; i < nb; i++) {
        quint32 graine = base + (quint32)i * 2654435761u;
        Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
        Sonde bot(&p, 2.0f, p.getGraine());

        float dt = 16.0f / 1000.0f;
        bool fonce = false;
        int niveau = 1;
        int tBloquee = 0, tEtroite = 0, tSansPont = 0, tAttente = 0;
        bool espace = false;

        for(long tick = 0; tick < 40000000L; tick++) {
            EEtatPartie avant = p.etat();

            if(avant == epGameOver || avant == epAbandon) {
                break;
            }

            if(avant != epEcoulement) {
                fonce = false;
            }

            if(avant == epAttente && !fonce) {
                tAttente++;

                if(bot.restant() <= 0) {
                    bool bloquee, pontDispo;
                    int place;

                    bot.observer(bloquee, place, pontDispo);

                    if(bloquee)               tBloquee++;
                    if(bloquee || place <= SEUIL_PLACE) tEtroite++;
                    if(!pontDispo)            tSansPont++;
                }
            }

            int facteur = (avant == epEcoulement && fonce) ? 8 : 1;

            for(int pas = facteur; pas > 0; pas--) {
                if(facteur == 1) {
                    bot.avancer(dt);

                    if(bot.veutFoncer() && !fonce) {
                        espace = espace || (p.etat() == epAttente);
                        p.lancerFluxAnticipe();
                        p.passerLaSuite();
                        fonce = true;
                    }
                }

                p.avancer(dt);
            }

            if(avant == epAttente && p.etat() != epAttente) {
                printf("%d,%d,%d,%d,%d,%d,%d,%d,%d\n", i, niveau, p.longueurMinimale(),
                       p.longueurTracee(), espace ? 1 : 0,
                       tBloquee * 16 / 10, tEtroite * 16 / 10, tSansPont * 16 / 10,
                       tAttente * 16 / 10);
                tBloquee = tEtroite = tSansPont = tAttente = 0;
                espace = false;
            }

            if(p.etat() == epEcoulement || p.etat() == epAttente) {
                niveau = p.niveau();
            }
        }
    }

    return 0;
}
