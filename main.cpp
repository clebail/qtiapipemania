#include "mainwindow.h"

#include <QApplication>
#include <QLocale>
#include <QTranslator>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QTranslator translator;
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = "qtiapipemania_" + QLocale(locale).name();
        if (translator.load(":/i18n/" + baseName)) {
            a.installTranslator(&translator);
            break;
        }
    }
    MainWindow w;

    // Deux options de mise au point, a poser dans cet ordre : la graine refait
    // la partie, le niveau choisit ou l'on y entre.
    //
    //   --graine N   rejoue exactement cette partie
    //   --niveau N   demarre au niveau N plutot qu'au premier
    //
    // A graine et niveau egaux, le plateau et la file sont identiques a la case
    // pres : c'est ce qui permet de revoir une manche qui s'est mal passee au
    // lieu d'attendre qu'elle se represente.
    const QStringList args = QCoreApplication::arguments();

    int posGraine = args.indexOf("--graine");

    if(posGraine >= 0 && posGraine + 1 < args.size()) {
        bool ok = false;
        uint graine = args.at(posGraine + 1).toUInt(&ok);

        if(ok) {
            w.setGraine(graine);
        } else {
            qWarning("--graine attend un entier positif, recu \"%s\"",
                     qPrintable(args.at(posGraine + 1)));
        }
    }

    int posNiveau = args.indexOf("--niveau");

    if(posNiveau >= 0 && posNiveau + 1 < args.size()) {
        bool ok = false;
        int niveau = args.at(posNiveau + 1).toInt(&ok);

        if(ok && niveau >= 1) {
            w.setNiveauDepart(niveau);
        } else {
            qWarning("--niveau attend un entier >= 1, recu \"%s\"",
                     qPrintable(args.at(posNiveau + 1)));
        }
    }

    // La graine n'est plus annoncee ici mais a chaque manche, par la fenetre :
    // une partie perdue en tire une nouvelle au hasard, et c'est justement
    // celle-la qu'on veut connaitre.

    w.show();
    return a.exec();
}
