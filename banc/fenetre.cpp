// LA VRAIE FENETRE, pilotee en temps reel hors ecran.
//
//   fenetre <graine> <niveau> <vies> <bombes> <secondes>
//
// Tous les autres harnais de banc/ deroulent une manche de soixante secondes
// en quelques millisecondes de montre. C'est ce qu'on veut pour mesurer -- des
// centaines de parties par heure -- et c'est aveugle a tout ce qui depend du
// TEMPS REEL : une etude de bombe en thread, une planification qui rend sa
// reponse trop tard, un bot qui fonce avant que la bombe existe.
//
// Celui-ci fait tourner MainWindow elle-meme, avec son battement, son
// installerBot et sa repeinture, a raison d'un battement toutes les 16 ms de
// montre. Il a deja attrape deux pannes que le banc ne voyait pas (2026-09-19)
// -- dont un bot qui brulait huit vies sans jamais poser une bombe alors que
// le banc, lui, en posait quatre.
//
// Compilation (il faut les moc de la fenetre, donc un repertoire de build) :
//
//   QT=/opt/homebrew/Cellar/qt@5/5.15.19
//   clang++ -std=c++17 -O2 -fPIC -DRACINE_PROJET='"/tmp/capture"' -I. -Ibuild \
//     -I$QT/lib/Qt{Core,Gui,Widgets}.framework/Headers -F$QT/lib \
//     -o /tmp/fenetre banc/fenetre.cpp mainwindow.cpp wgame.cpp wpanneau.cpp \
//     wdepart.cpp dessinpiece.cpp journal.cpp common.cpp bot*.cpp trace.cpp \
//     minage.cpp ecoulement.cpp game.cpp partie.cpp piecefile.cpp \
//     build/moc_*.cpp -framework QtWidgets -framework QtGui -framework QtCore
//   QT_QPA_PLATFORM=offscreen /tmp/fenetre 2998034427 36 9 4 30
//
// RACINE_PROJET pointe ailleurs que le projet : la capture d'images efface son
// dossier, et on ne veut pas qu'un essai emporte une prise.
#include <QApplication>
#include <QPushButton>
#include <QElapsedTimer>
#include <QMetaObject>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include "mainwindow.h"
#include "partie.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);

    quint32 graine = argc > 1 ? (quint32)strtoul(argv[1], nullptr, 10) : 2998034427u;
    int niveau = argc > 2 ? atoi(argv[2]) : 36;
    int vies = argc > 3 ? atoi(argv[3]) : 9;
    int bombes = argc > 4 ? atoi(argv[4]) : 4;
    double secondes = argc > 5 ? atof(argv[5]) : 26.0;

    MainWindow w;
    w.setGraine(graine);
    w.setNiveauDepart(niveau);
    w.setVies(vies);
    w.setBombes(bombes);
    w.resize(1000, 700);

    QPushButton *trace = w.findChild<QPushButton *>("pbTrace");
    QPushButton *pause = w.findChild<QPushButton *>("pbPause");

    trace->click();     // on installe le bot, comme un clic
    pause->click();     // puis on demarre

    QElapsedTimer montre;
    montre.start();

    while(montre.elapsed() < secondes * 1000.0) {
        QMetaObject::invokeMethod(&w, "battement");
        a.processEvents();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    printf("fin a t=%.1f s\n", montre.elapsed() / 1000.0);
    return 0;
}
