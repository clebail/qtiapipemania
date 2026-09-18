// Rejoue une manche et raconte chaque geste du bot dans une fenetre de temps.
//   rejouer <graine> <niveau> <retard> <t0> <t1>
// `retard` est le temps avant l'installation du bot : c'est ce que fait le
// joueur qui lance l'appli avec --graine/--niveau puis clique sur le bouton du
// bot, delai deja entame. Sans lui on ne retombe pas sur la manche vue a
// l'ecran.
//
// Par jeton : tete, ouv (types qui ne tuent pas sur le coup), ouvU (qui
// laissent de quoi finir), place atteignable, objectif restant, acculeParLeFlux,
// et le rang dans la file de la premiere piece qui se raccorderait -- -1 quand
// aucune ne passe.
#include <QtGlobal>
#include <cstdio>
#include <cstdlib>
#define private protected
#include "partie.h"
#include "botmemoire.h"
#undef private

class Sonde : public BotMemoire {
public:
    using BotMemoire::BotMemoire;

    void observer(bool &aTete, int &tc, int &tr, ESens &te, int &place, int &restant,
                  int &ouv, int &ouvU, bool &accule, int &rangPont) {
        QVector<unsigned char> sauve = tas;
        int col, row;
        ESens entree;

        place = 0; ouv = 0; ouvU = 0; rangPont = -1;
        restant = objectifRestant();
        accule = acculeParLeFlux(2.0f);
        aTete = tete(col, row, entree);
        tc = aTete ? col : -1;
        tr = aTete ? row : -1;
        te = entree;

        if(aTete) {
            foreach(ETypePiece t, Ecoulement::piecesCompatibles(entree)) {
                if(!meneALaMort(t, col, row, entree)) {
                    ouv++;
                    int e = espaceApres(t, col, row, entree, 0);
                    if(e > place) place = e;
                }
                if(!culDeSac(t, col, row, entree)) ouvU++;
            }

            for(int r = 0; r < p->file()->getTaille(); r++) {
                Piece f = p->file()->getPiece(r);
                if(Ecoulement::piecesCompatibles(entree).contains(f.type)
                   && !meneALaMort(f.type, col, row, entree)) { rangPont = r; break; }
            }
        }

        tas = sauve;
    }
};

int main(int argc, char *argv[]) {
    quint32 graine = argc > 1 ? (quint32)strtoul(argv[1], nullptr, 10) : 3310453134u;
    int niveau = argc > 2 ? atoi(argv[2]) : 18;
    float retard = argc > 3 ? atof(argv[3]) : 18.0f;
    float t0 = argc > 4 ? atof(argv[4]) : 60.0f;
    float t1 = argc > 5 ? atof(argv[5]) : 80.0f;

    Partie p(COLONNES_PLATEAU, LIGNES_PLATEAU, graine);
    p.setNiveauDepart(niveau);

    float dt = 16.0f / 1000.0f;
    Sonde *bot = nullptr;
    bool fonce = false;
    float t = 0.0f;
    int mancheVue = -1;

    for(long tick = 0; tick < 400000L && t < t1 + 5.0f; tick++, t += dt) {
        EEtatPartie avant = p.etat();

        if(avant == epGameOver || avant == epAbandon) break;
        if(avant != epEcoulement) fonce = false;

        if(bot == nullptr && t >= retard) bot = new Sonde(&p, 2.0f, p.getGraine());

        if(p.numeroManche() != mancheVue) {
            mancheVue = p.numeroManche();
            printf("=== manche %d niveau %d objectif %d vies %d ===\n",
                   mancheVue, p.niveau(), p.longueurMinimale(), p.vies());
        }

        bool aTete = false, accule = false;
        int tc = -1, tr = -1, place = 0, restant = 0, ouv = 0, ouvU = 0, rangPont = -1;
        ESens te = sHaut;
        bool dedans = (t >= t0 && t <= t1);

        if(bot && dedans) bot->observer(aTete, tc, tr, te, place, restant, ouv, ouvU, accule, rangPont);

        int traceeAvant = p.longueurTracee();
        Piece sommet = p.file()->getPiece(0);
        int facteur = (avant == epEcoulement && fonce) ? 8 : 1;

        for(int pas = facteur; pas > 0; pas--) {
            if(facteur == 1 && bot != nullptr) {
                bot->avancer(dt);

                if(bot->veutFoncer() && !fonce) {
                    printf("[%6.2fs] *** ESPACE *** tete=(%d,%d) ouv=%d ouvU=%d place=%d restant=%d accule=%d pont=%d\n",
                           t, tc, tr, ouv, ouvU, place, restant, accule, rangPont);
                    p.lancerFluxAnticipe();
                    p.passerLaSuite();
                    fonce = true;
                }
            }

            p.avancer(dt);
        }

        Piece suivante = p.file()->getPiece(0);

        if(bot && dedans && (suivante.type != sommet.type || suivante.sens != sommet.sens)) {
            printf("[%6.2fs] geste  tete=(%d,%d) ouv=%d ouvU=%d place=%d restant=%d accule=%d pont=%d tracee %d->%d\n",
                   t, tc, tr, ouv, ouvU, place, restant, accule, rangPont,
                   traceeAvant, p.longueurTracee());
        }

        if(avant != p.etat() && (p.etat() == epReussie || p.etat() == epPerdue)) {
            printf("[%6.2fs] %s traversees=%d objectif=%d vies=%d\n", t,
                   p.etat() == epReussie ? "REUSSIE" : "PERDUE",
                   p.casesTraversees(), p.longueurMinimale(), p.vies());
        }
    }

    delete bot;
    return 0;
}
