#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "ui_mainwindow.h"
#include "partie.h"

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
private:
    Partie *p;
    QTimer horloge;

    void rafraichir();
private slots:
    void on_pbGen_clicked();
    // Nom volontairement hors du motif on_<objet>_<signal> : l'horloge est
    // connectee explicitement, pas via connectSlotsByName.
    void battement();
};
#endif // MAINWINDOW_H
