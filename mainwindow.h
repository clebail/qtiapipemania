#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "ui_mainwindow.h"
#include "partie.h"

class Bot;
class Journal;

class MainWindow : public QMainWindow, private Ui::MainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    // Ouvre le journal de gestes. Sans appel, rien n'est enregistre.
    void ouvrirJournal(const QString &chemin);
    // Demarre les parties a ce niveau plutot qu'au premier (--niveau).
    void setNiveauDepart(int niveau);
    // Rejoue la partie de cette graine (--graine). A graine et niveau egaux,
    // le plateau et la file sont identiques a la case pres.
    void setGraine(quint32 graine);
    // Graine de la partie en cours, a noter pour la rejouer.
    quint32 graine() const;
    ~MainWindow();
private:
    Partie *p;
    // Nul quand c'est le joueur qui tient la souris. Le meme bot que le banc
    // d'essai, avec le meme bridage : ce qu'on voit ici est ce qui est mesure.
    Bot *bot = nullptr;
    // Nul sans --journal. Enregistre les gestes du joueur comme ceux du bot,
    // avec la meme horloge et la meme definition : c'est la condition pour que
    // les deux chiffres soient comparables.
    Journal *journal = nullptr;
    // Temps de jeu ecoule, en secondes simulees. C'est l'horloge du jeu et du
    // banc, pas celle du mur : si la machine decroche, la partie ralentit mais
    // la mesure reste comparable.
    float tempsSimule = 0.0f;
    // Acceleration demandee a la barre d'espace. Sans retour possible : elle ne
    // retombe qu'a la fin de la manche, la suivante ne demarrant pas lancee.
    bool accelereManuel = false;
    // Fige la partie : battement() ne fait plus rien tant qu'elle est vraie.
    // Sert a immobiliser l'ecran (bot compris) le temps d'une copie d'ecran.
    bool enPause = false;
    // Derniere manche annoncee, pour ne l'annoncer qu'une fois. Le couple
    // (graine de partie, niveau) suffit a rejouer une manche a l'identique.
    quint32 graineAnnoncee = 0;
    int niveauAnnonce = 0;
    QTimer horloge;

    void rafraichir();
    // Affiche la commande qui rejoue la manche en cours, des qu'elle change.
    // Sans elle, une partie perdue emporte sa graine avec elle : le bot en
    // recommence une au hasard, et la manche qu'on voulait revoir est perdue.
    void annoncerManche();
    // Combien de battements de jeu jouer par battement d'horloge. Vaut 1 tant
    // qu'il y a quelque chose a regarder.
    int facteurTemps() const;
    // Un battement de jeu et un seul, a la cadence nominale de 16 ms. Le bot ne
    // joue que si la partie se deroule a vitesse normale : des qu'on accelere,
    // il n'y a plus rien a regarder et il pose les mains.
    void battementUnitaire(float dt, bool joueLeBot);
    // Barre d'espace : depart anticipe du flux. La fenetre recoit la touche
    // parce que ni la grille ni les boutons ne prennent le focus.
    void keyPressEvent(QKeyEvent *event) override;
    // Ce que fait la barre d'espace : lancer le flux, sauter la pause de fin de
    // manche, derouler le reste a vitesse acceleree. Le joueur l'appelle par la
    // touche, un bot par veutFoncer().
    void foncer();
    // Installe le bot portant ce nom, ou rend la main au joueur si le nom est
    // vide. Les noms sont ceux de BotFactory, donc ceux de --bot.
    void installerBot(const QString &nom);
    QString nomBotCourant() const;
    // Mode pas a pas : la partie est figee et le bot ne joue plus de lui-meme,
    // c'est la fenetre qui lui dicte chaque geste. Reactive les deux boutons de
    // geste et recalcule l'overlay sur l'etat courant.
    void majPasAPas();
private slots:
    // Nom volontairement hors du motif on_<objet>_<signal> : l'horloge est
    // connectee explicitement, pas via connectSlotsByName.
    void battement();
    void pieceDeposee(int col, int row, bool accepte);
    void on_pbGlouton_clicked();
    void on_pbSpace_clicked();
    void on_pbSpaceAnticp_clicked();
    void on_pbPause_clicked();
    void on_cbStep_toggled(bool actif);
    void on_pbGeste_clicked();
    void on_pbPose_clicked();
    void on_pbDefausse_clicked();
};
#endif // MAINWINDOW_H
