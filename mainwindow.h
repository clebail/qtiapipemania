#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "ui_mainwindow.h"
#include "game.h"
#include "ecoulement.h"
#include "piecefile.h"

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    Game *g;
    Ecoulement *e;
    PieceFile *pf;
    QTimer floodTimer;
private slots:
    void on_pbFlood_clicked();
    void on_pbGen_clicked();
    // Nom volontairement hors du motif on_<objet>_<signal> : le timer est
    // connecte explicitement, pas via connectSlotsByName.
    void avancerFlood();
};
#endif // MAINWINDOW_H
