#include "mainwindow.h"

// Bareme : la penalite de remplacement est celle du jeu d'origine, les points
// par case s'y calent (un remplacement coute une case de progression).
#define POINTS_PAR_CASE         50
#define PENALITE_REMPLACEMENT   50
// Longueur minimale du pipeline pour que la manche soit reussie.
#define LONGUEUR_MINIMALE       20

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
    connect(game, &WGame::pieceDeposee, this, &MainWindow::compterPiece);

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

void MainWindow::majScore() {
    lbScore->setText(tr("Score : %1").arg(score));
}

void MainWindow::compterPiece(bool remplacement) {
    if(remplacement) {
        score -= PENALITE_REMPLACEMENT;
        majScore();
    }
}

void MainWindow::terminerManche() {
    floodTimer.stop();

    int traversees = e->nbCasesTraversees();
    score += traversees * POINTS_PAR_CASE;
    majScore();

    if(traversees >= LONGUEUR_MINIMALE) {
        lbStatut->setText(tr("Manche reussie : %1 cases").arg(traversees));
    } else {
        lbStatut->setText(tr("Perdu : %1 cases sur %2 requises")
                          .arg(traversees).arg(LONGUEUR_MINIMALE));
    }
}

void MainWindow::on_pbFlood_clicked() {
    lbStatut->setText(tr("Ecoulement..."));
    e->reinitialiser();
    e->demarrer();
    floodTimer.start();
}

void MainWindow::on_pbGen_clicked() {
    floodTimer.stop();
    g->genererReseauTest();
    e->reinitialiser();
    lbStatut->clear();
    game->repaint();
}

void MainWindow::avancerFlood() {
    auto etat = e->avancer(floodTimer.interval() / 1000.0f);

    // Une fuite ne fait pas perdre a elle seule : elle arrete l'ecoulement,
    // et c'est la longueur atteinte qui decide de l'issue de la manche.
    if(etat != eEnCours) {
        terminerManche();
    }

    game->repaint();
}
