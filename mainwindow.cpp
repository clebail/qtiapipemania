#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), floodTimer() {
    setupUi(this);

    g = new Game(15, 15);
    e = new Ecoulement(g);
    pf = new PieceFile(FILE_SIZE);

    game->setGame(g);
    game->setEcoulement(e);
    game->setPieceFile(pf);
    file->setPieceFile(pf);

    connect(game, &WGame::pieceDeposee, file, &WPieceFile::animerDepilage);

    int spriteW = TAILLE_CASE;
    int spriteH = TAILLE_CASE;
    game->setFixedSize(g->getLargeur() * spriteW, g->getHauteur() * spriteH);

    // La fenetre se dimensionne d'elle-meme autour de la grille : inutile de
    // recalculer sa taille a la main a chaque widget ajoute (barre de boutons...).
    centralWidget()->layout()->setSizeConstraint(QLayout::SetFixedSize);

    // Cadence d'animation : la vitesse de l'ecoulement ne depend pas de cet
    // intervalle mais de Ecoulement::setDureeRemplissage().
    floodTimer.setInterval(16);
    connect(&floodTimer, &QTimer::timeout, this, &MainWindow::avancerFlood);
}

MainWindow::~MainWindow() {
    delete g;
    delete e;
    delete pf;
}

void MainWindow::on_pbFlood_clicked() {
    floodTimer.start();
    e->reinitialiser();
    e->demarrer();
}

void MainWindow::on_pbGen_clicked() {
    floodTimer.stop();
    g->genererReseauTest();
    game->repaint();
}

void MainWindow::avancerFlood() {
    auto etat = e->avancer(floodTimer.interval() / 1000.0f);

    // TEMPORAIRE : on laisse couler malgre une fuite, pour pouvoir observer le
    // remplissage complet du reseau de test. A remettre en "etat != eEnCours"
    // quand le game over sera branche sur eFuite.
    if(etat == eTermine) {
        floodTimer.stop();
    }

    game->repaint();
}
