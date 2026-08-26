#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), horloge() {
    setupUi(this);

    p = new Partie(15, 15);

    game->setPartie(p);
    panneau->setPartie(p);

    connect(game, &WGame::pieceDeposee, panneau, &WPanneau::animerDepilage);
    connect(game, &WGame::pieceDeposee, this, &MainWindow::rafraichir);

    game->setFixedSize(p->plateau()->getLargeur() * TAILLE_CASE,
                       p->plateau()->getHauteur() * TAILLE_CASE);

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
    delete p;
}

void MainWindow::rafraichir() {
    depart->setFraction(p->fractionAvantDepart());
    panneau->update();

    switch(p->etat()) {
    case epAttente:
        lbStatut->setText(tr("Posez vos tuyaux..."));
        break;
    case epEcoulement:
        lbStatut->setText(tr("Ecoulement..."));
        break;
    case epReussie:
        lbStatut->setText(tr("Manche reussie : %1 cases").arg(p->casesTraversees()));
        break;
    case epPerdue:
        lbStatut->setText(tr("Perdu : %1 cases sur %2 requises")
                          .arg(p->casesTraversees()).arg(p->longueurMinimale()));
        break;
    }
}

void MainWindow::battement() {
    EEtatPartie avant = p->etat();

    p->avancer(horloge.interval() / 1000.0f);

    if(p->etat() == epEcoulement) {
        game->repaint();
    }

    // Le statut et la jauge ne bougent qu'en attente ou sur changement d'etat :
    // inutile de les reecrire soixante fois par seconde une fois la manche finie.
    if(p->etat() != avant || p->etat() == epAttente) {
        rafraichir();
    }
}

void MainWindow::on_pbGen_clicked() {
    p->plateau()->genererReseauTest();
    p->nouvelleManche();
    game->repaint();
    rafraichir();
}
