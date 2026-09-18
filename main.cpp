#include "mainwindow.h"

#include <climits>

#include <QApplication>
#include <QLocale>
#include <QTranslator>

// Lit une option entiere de la ligne de commande. Renvoie false -- et ne touche
// pas a `valeur` -- si l'option est absente, sans argument, ou hors des bornes :
// une faute de frappe laisse le reglage par defaut plutot que de tordre la
// partie en silence.
static bool lireEntier(const QStringList &args, const QString &nom,
                       int min, int max, int *valeur) {
    int pos = args.indexOf(nom);

    if(pos < 0 || pos + 1 >= args.size()) {
        return false;
    }

    bool ok = false;
    int lu = args.at(pos + 1).toInt(&ok);

    if(!ok || lu < min || lu > max) {
        qWarning("%s attend un entier entre %d et %d, recu \"%s\"",
                 qPrintable(nom), min, max, qPrintable(args.at(pos + 1)));
        return false;
    }

    *valeur = lu;
    return true;
}

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

    // Cinq options de mise au point, a poser dans cet ordre : la graine refait
    // la partie, les trois suivantes disent ou l'on y entre, la derniere ouvre
    // le carnet.
    //
    //   --graine N      rejoue exactement cette partie
    //   --niveau N      demarre au niveau N plutot qu'au premier
    //   --vies N        demarre avec N vies (1 a VIES_MAX) au lieu de trois
    //   --bombes N      demarre avec N bombes (0 a BOMBES_MAX) au lieu d'aucune
    //   --journal FICH  consigne un geste par ligne dans ce CSV (voir journal.h)
    //
    // A graine et niveau egaux, le plateau et la file sont identiques a la case
    // pres : c'est ce qui permet de revoir une manche qui s'est mal passee au
    // lieu d'attendre qu'elle se represente. Vies et bombes, elles, ne changent
    // ni le plateau ni la file : elles ne font que placer la partie dans l'etat
    // qu'on veut observer -- la derniere vie, le stock plein -- sans avoir a
    // jouer ce qui y aurait mene.
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

    int niveau = 0;

    if(lireEntier(args, "--niveau", 1, INT_MAX, &niveau)) {
        w.setNiveauDepart(niveau);
    }

    // Apres --graine : celle-ci repart sur une partie neuve, donc sur les vies
    // et les bombes du depart.
    int vies = 0;

    if(lireEntier(args, "--vies", 1, VIES_MAX, &vies)) {
        w.setVies(vies);
    }

    int bombes = 0;

    if(lireEntier(args, "--bombes", 0, BOMBES_MAX, &bombes)) {
        w.setBombes(bombes);
    }

    // En dernier, et ce n'est pas indifferent : le journal date chacune de ses
    // lignes du niveau courant, il doit donc s'ouvrir sur une partie deja
    // reglee. Le fichier s'ouvre en AJOUT (voir Journal) -- plusieurs sessions
    // s'y accumulent, la colonne `manche` repart a zero a chaque lancement.
    int posJournal = args.indexOf("--journal");

    if(posJournal >= 0) {
        if(posJournal + 1 < args.size()) {
            w.ouvrirJournal(args.at(posJournal + 1));
        } else {
            qWarning("--journal attend un chemin de fichier");
        }
    }

    // La graine n'est plus annoncee ici mais a chaque manche, par la fenetre :
    // une partie perdue en tire une nouvelle au hasard, et c'est justement
    // celle-la qu'on veut connaitre.

    w.show();
    return a.exec();
}
