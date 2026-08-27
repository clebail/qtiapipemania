#include "mainwindow.h"
#include <QKeyEvent>

#include "botfactory.h"
#include "journal.h"

// Cadence des bots joues a l'ecran. Deux poses par seconde, c'est le milieu de
// la fourchette que BOT.md prete a un humain en reflexion continue -- et c'est
// surtout assez lent pour qu'on voie le bot penser, ce qui est tout l'interet de
// le regarder jouer avant de croire ses chiffres.
#define CADENCE_BOT             2.0f
// Une fois l'objectif securise, la manche est jouee : plus aucune decision, plus
// rien a regarder. On deroule la fin a cette vitesse plutot que de la subir. Ne
// s'applique qu'a un bot : un joueur humain, lui, construit encore son bonus.
#define ACCELERATION_ACQUISE    8

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), horloge() {
    setupUi(this);

    p = new Partie(COLONNES_PLATEAU, LIGNES_PLATEAU);

    game->setPartie(p);
    panneau->setPartie(p);

    // Les boutons de bot s'excluent, et leur etat enfonce dit lequel joue.
    pbGlouton->setCheckable(true);
    pbSpace->setCheckable(true);
    pbSpaceAnticp->setCheckable(true);

    // Sans ca, un bouton garde le focus apres le clic et avale la barre
    // d'espace : elle rejouerait le bouton au lieu de lancer le flux.
    pbGlouton->setFocusPolicy(Qt::NoFocus);
    pbSpace->setFocusPolicy(Qt::NoFocus);
    pbSpaceAnticp->setFocusPolicy(Qt::NoFocus);
    pbPause->setFocusPolicy(Qt::NoFocus);
    pbGeste->setFocusPolicy(Qt::NoFocus);
    pbPose->setFocusPolicy(Qt::NoFocus);
    pbDefausse->setFocusPolicy(Qt::NoFocus);

    connect(game, &WGame::pieceDeposee, this, &MainWindow::pieceDeposee);
    connect(cbMEETas, &QCheckBox::toggled, game, &WGame::setAfficherTas);
    game->setAfficherTas(cbMEETas->isChecked());
    connect(cbPlan, &QCheckBox::toggled, game, &WGame::setAfficherPlan);
    game->setAfficherPlan(cbPlan->isChecked());
    majPasAPas();

    game->setFixedSize(p->plateau()->getLargeur() * tailleCase(),
                       p->plateau()->getHauteur() * tailleCase());

    // La fenetre se dimensionne d'elle-meme autour de la grille : inutile de
    // recalculer sa taille a la main a chaque widget ajoute (barre de boutons...).
    centralWidget()->layout()->setSizeConstraint(QLayout::SetFixedSize);

    // Une seule horloge pour tout le jeu : c'est Partie qui sait, selon son
    // etat, s'il faut decompter avant le depart ou faire avancer le flux.
    horloge.setInterval(16);
    connect(&horloge, &QTimer::timeout, this, &MainWindow::battement);
    horloge.start();

    rafraichir();
}

MainWindow::~MainWindow() {
    // Le bot d'abord : il tient un pointeur sur la partie.
    game->setBot(nullptr);
    delete bot;
    delete journal;
    delete p;
}

// Le bot est reinstalle apres coup : il tient un tas et un plan de defausse
// calcules sur le plateau precedent, qui vient d'etre remplace.
void MainWindow::setNiveauDepart(int niveau) {
    p->setNiveauDepart(niveau);

    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    rafraichir();
}

// Le bot est reinstalle pour la meme raison qu'au changement de niveau : son
// tas et son plan de defausse valaient pour le plateau precedent.
void MainWindow::setGraine(quint32 graine) {
    p->nouvellePartie(graine);

    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    rafraichir();
}

quint32 MainWindow::graine() const {
    return p->getGraine();
}

void MainWindow::ouvrirJournal(const QString &chemin) {
    delete journal;
    journal = new Journal(chemin);
}

// Un depot du joueur. Le coup refuse compte aussi : c'est un geste, il occupe la
// main et il pese dans la cadence.
void MainWindow::pieceDeposee(int col, int row, bool accepte) {
    if(accepte) {
        panneau->animerDepilage();
    }

    if(journal != nullptr) {
        journal->geste(p, tempsSimule, col, row, accepte, false);
    }

    rafraichir();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        // Espace veut dire "je n'attends pas", et c'est au joueur d'en decider.
        // Aucun test d'etat ici : la touche fait tout ce qu'elle peut faire, et
        // chaque action se refuse d'elle-meme la ou elle n'a pas de sens. Avant
        // le depart on lance le flux contre la prime, pendant la pause de fin de
        // manche on enchaine -- et dans tous les cas on fonce.
        //
        // Ca ne se reprend pas. Acceleree, la manche ne laisse plus le temps de
        // poser : le jeu avance huit fois plus vite, la main du joueur non, et
        // il perd donc les cases qu'il aurait construites devant le flux. C'est
        // le meme marche que la prime de depart -- du temps de construction
        // echange contre autre chose -- donc le meme engagement, sans retour.
        foncer();
        rafraichir();

        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::foncer() {
    p->lancerFluxAnticipe();
    p->passerLaSuite();
    accelereManuel = true;
}

void MainWindow::annoncerManche() {
    if(p->getGraine() == graineAnnoncee && p->niveau() == niveauAnnonce) {
        return;
    }

    graineAnnoncee = p->getGraine();
    niveauAnnonce = p->niveau();

    qInfo("--- manche : niveau %d, objectif %d   |   rejouer avec :  --graine %u --niveau %d",
          p->niveau(), p->longueurMinimale(), p->getGraine(), p->niveau());
}

void MainWindow::rafraichir() {
    depart->setFraction(p->fractionAvantDepart());
    panneau->update();
    // La grille aussi : un changement d'etat peut signifier un plateau neuf.
    game->update();
}

// Accelerer, c'est jouer PLUSIEURS battements de 16 ms, jamais un battement plus
// gros : Ecoulement quantifie le remplissage a chaque appel, donc un dt multiplie
// ne donnerait pas la meme partie. La simulation reste celle du jeu, tick pour
// tick, on la deroule seulement plus vite que le temps reel.
int MainWindow::facteurTemps() const {
    if(p->etat() != epEcoulement) {
        return 1;
    }

    // Barre d'espace, ou bot qui n'a plus rien a construire et l'a demandee :
    // on deroule la fin a vitesse acceleree, il n'y a plus de decision a voir.
    if(accelereManuel) {
        return ACCELERATION_ACQUISE;
    }

    // Sinon le flux avance au temps reel -- que le joueur construise encore
    // devant lui ou qu'on regarde un bot le faire. Jamais zero : ca figerait la
    // manche tant que personne ne demande a accelerer, et le flux parti de
    // lui-meme a l'expiration du delai n'aurait alors plus rien pour repartir.
    return 1;
}

void MainWindow::battement() {
    annoncerManche();

    // Le pas a pas fige la partie comme la pause : plus de delai qui s'ecoule,
    // plus de flux qui avance, plus de bot qui joue de lui-meme. C'est tout
    // l'interet du mode -- deplier une manche geste par geste, sans le
    // chronometre qui decide a votre place.
    if(enPause || cbStep->isChecked()) {
        return;
    }

    EEtatPartie avant = p->etat();
    float dt = horloge.interval() / 1000.0f;

    // L'acceleration demandee ne vaut que pour la manche en cours : elle tombe
    // des que le flux s'arrete, sans quoi la suivante demarrerait lancee.
    if(avant != epEcoulement) {
        accelereManuel = false;
    }

    // Le facteur se calcule une fois pour toute la rafale : il decide aussi si
    // le bot joue, et l'interroger dans la boucle le ferait changer d'avis en
    // cours de route.
    int facteur = facteurTemps();

    for(int pas=facteur; pas>0; pas--) {
        // Accelerer, c'est n'avoir plus rien a regarder : le bot pose les mains
        // pour de bon. Sans ca il joue huit fois plus vite a l'ecran, et la fin
        // de manche qu'on voulait rendre lisible devient illisible.
        //
        // Le prix est celui que paie deja un joueur humain -- le jeu avance huit
        // fois plus vite, pas sa main, donc il perd les cases qu'il aurait
        // construites devant le flux. Le marche est ainsi le meme pour les deux :
        // on fonce, on ne construit plus.
        battementUnitaire(dt, facteur == 1);
    }

    // Fin de manche : c'est la que le journal peut dire quels gestes ont servi.
    if(journal != nullptr && p->etat() != avant
       && (p->etat() == epReussie || p->etat() == epPerdue)) {
        journal->finDeManche(p);
    }

    if(p->etat() == epEcoulement) {
        game->repaint();
    }

    // Le statut et la jauge ne bougent qu'en attente ou sur changement d'etat :
    // inutile de les reecrire soixante fois par seconde une fois la manche finie.
    // Avec un bot en revanche, le plateau et la file changent a n'importe quel
    // battement sans qu'aucun signal ne l'annonce : poserPiece() n'est pas
    // WGame::pieceDeposee.
    if(p->etat() != avant || p->etat() == epAttente || bot != nullptr) {
        rafraichir();
    }
}

void MainWindow::battementUnitaire(float dt, bool joueLeBot) {
    tempsSimule += dt;

    if(bot != nullptr && joueLeBot) {
        Piece sommet = p->file()->getPiece(0);
        // Le bot ne dit pas ou il pose : on le lit sur le plateau, par
        // difference. Une copie de 225 octets par battement, et seulement quand
        // le journal est ouvert -- plutot que d'imposer a l'API des bots une
        // obligation de compte-rendu qui ne sert qu'a la mesure.
        Game avant(*p->plateau());

        // Le bot joue avant Partie::avancer(), comme le clic du joueur precede
        // le battement suivant : c'est l'ordre du banc d'essai, et les deux
        // doivent derouler exactement la meme partie.
        bot->avancer(dt);

        Piece suivante = p->file()->getPiece(0);

        // La file a bouge, donc une piece a ete posee. Le panneau se repeint de
        // toute facon sur l'etat courant : l'animation n'est que du confort, et
        // la manquer quand deux pieces identiques se suivent ne se voit pas.
        if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
            panneau->animerDepilage();

            if(journal != nullptr) {
                for(int i=0; i<avant.getSize(); i++) {
                    int col = i % avant.getLargeur();
                    int row = i / avant.getLargeur();

                    if(avant.getTypePiece(col, row) != p->plateau()->getTypePiece(col, row)
                       || avant.getSens(col, row) != p->plateau()->getSens(col, row)) {
                        journal->geste(p, tempsSimule, col, row, true, true);
                        break;
                    }
                }
            }
        }

        // Le bot n'a plus de coup utile : on lui accorde la barre d'espace,
        // exactement comme au joueur qui appuierait dessus.
        if(bot->veutFoncer()) {
            foncer();
        }
    }

    p->avancer(dt);
}

// Nom du bot enfonce, vide si c'est le joueur qui tient la souris.
QString MainWindow::nomBotCourant() const {
    if(pbGlouton->isChecked())     return "glouton";
    if(pbSpace->isChecked())       return "space";
    if(pbSpaceAnticp->isChecked()) return "spaceAnticp";

    return QString();
}

void MainWindow::installerBot(const QString &nom) {
    delete bot;

    bot = nom.isEmpty() ? nullptr
                        : BotFactory::createInstance(nom, p, CADENCE_BOT, p->getGraine());

    // La grille n'affiche l'overlay du tas que tant qu'un bot joue.
    game->setBot(bot);

    pbGlouton->setChecked(nom == "glouton");
    pbSpace->setChecked(nom == "space");
    pbSpaceAnticp->setChecked(nom == "spaceAnticp");

    majPasAPas();
}

// Le bouton est bascule : un deuxieme clic rend la main au joueur, et cliquer
// l'autre bot remplace celui qui jouait.
void MainWindow::on_pbGlouton_clicked() {
    installerBot(pbGlouton->isChecked() ? "glouton" : QString());
}

void MainWindow::on_pbSpace_clicked() {
    installerBot(pbSpace->isChecked() ? "space" : QString());
}

void MainWindow::on_pbSpaceAnticp_clicked() {
    installerBot(pbSpaceAnticp->isChecked() ? "spaceAnticp" : QString());
}

// Simple bascule : un clic fige la partie (bot compris, battement() ne fait
// plus rien), le suivant la relache. Sert a immobiliser l'ecran le temps d'une
// copie d'ecran.
void MainWindow::on_pbPause_clicked() {
    enPause = !enPause;
    pbPause->setText(enPause ? tr("reprendre") : tr("pause"));
}

// Les deux boutons de geste n'ont de sens qu'en pas a pas, et il faut un bot
// pour les executer : ce sont ses primitives -- sa tete de construction, son
// plan de defausse -- que la fenetre declenche, pas des coups improvises.
void MainWindow::majPasAPas() {
    bool actif = cbStep->isChecked() && bot != nullptr;

    pbGeste->setEnabled(actif);
    pbPose->setEnabled(actif);
    pbDefausse->setEnabled(actif);

    // Hors du mode, un vecteur vide : l'overlay s'eteint. Le calcul n'a lieu
    // que la ou on le regarde.
    game->setRegion(actif ? bot->regionApresPoseSurTete() : QVector<unsigned char>());
}

void MainWindow::on_cbStep_toggled(bool actif) {
    // Les deux gestes du pas a pas sont ceux d'un bot -- sa tete de
    // construction, son plan de defausse. Cocher la case sans bot ne donnerait
    // donc que des boutons eteints, ce qui ressemble a une panne : on installe
    // le dernier bot en date, et son bouton s'enfonce pour dire lequel joue.
    if(actif && bot == nullptr) {
        installerBot("spaceAnticp");
        rafraichir();

        return;
    }

    majPasAPas();
    rafraichir();
}

// Le bot joue un geste, le sien : c'est sa strategie qu'on regarde, et c'est le
// seul des trois boutons qui montre ce qu'il ferait tout seul. veutFoncer() est
// volontairement ignore ici -- foncer lancerait le flux et l'accelererait, ce
// qui est exactement ce que le pas a pas sert a eviter.
void MainWindow::on_pbGeste_clicked() {
    if(bot == nullptr) {
        return;
    }

    Piece sommet = p->file()->getPiece(0);
    bot->unGeste();
    Piece suivante = p->file()->getPiece(0);

    if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}

// Pose sur la tete de construction, sans demander son avis au bot : en pas a
// pas c'est l'oeil qui juge, et une pose qui se condamne est justement ce
// qu'on veut pouvoir provoquer pour voir la region se fermer.
void MainWindow::on_pbPose_clicked() {
    if(bot == nullptr) {
        return;
    }

    if(bot->poserSurTete()) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}

// Defausse comme le bot le ferait de lui-meme, plan de defausse compris. Le
// bot ne rend pas la main : on lit le depilage sur la file, comme
// battementUnitaire() lit une pose sur le plateau.
void MainWindow::on_pbDefausse_clicked() {
    if(bot == nullptr) {
        return;
    }

    Piece sommet = p->file()->getPiece(0);
    bot->defausser();
    Piece suivante = p->file()->getPiece(0);

    if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}
