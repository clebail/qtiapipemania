#include "mainwindow.h"

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUi(this);

    g = new Game(15, 15);
    pf = new PieceFile(FILE_SIZE);

    game->setGame(g);
    game->setPieceFile(pf);
    file->setPieceFile(pf);

    connect(game, &WGame::pieceDeposee, file, QOverload<>::of(&QWidget::update));
}

MainWindow::~MainWindow() {
    delete g;
    delete pf;
}
