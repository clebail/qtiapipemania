#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QSemaphore>
#include <QThreadPool>
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
    // Vies et bombes de depart (--vies, --bombes) : de quoi se placer d'emblee
    // dans la situation qu'on veut regarder -- une derniere vie, un stock de
    // bombes plein -- sans jouer la partie qui y menerait.
    void setVies(int vies);
    void setBombes(int bombes);
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
    //
    // VRAI AU DEMARRAGE, et c'est le point : rien ne bouge tant qu'on n'a pas
    // clique. On choisit son bot, on coche ce qu'on veut enregistrer, et tout
    // part au meme instant -- sans quoi la partie a deja commence pendant qu'on
    // reglait, et les premieres images manquent.
    bool enPause = true;
    // Le bouton a-t-il deja ete clique ? Sert au seul libelle : "demarrer"
    // avant, "pause" / "reprendre" ensuite.
    bool demarre = false;
    // Derniere manche annoncee, pour ne l'annoncer qu'une fois. Son NUMERO, et
    // non le couple (graine, niveau) : la manche perdue rejoue le meme niveau
    // de la meme partie, et le couple ne bougeant pas, le rejeu passait sous
    // silence -- avec lui les vies, qui viennent justement d'en perdre une.
    int mancheAnnoncee = -1;
    // Numero de la prochaine image capturee. Reparti de zero a chaque lancement,
    // donc une session ecrase la precedente -- c'est voulu, on ne veut pas
    // melanger deux prises.
    int imageSuivante = 0;
    // Droit a enregistrer, en images. Monte de IMAGES_PAR_SECONDE x dt a chaque
    // battement, une image le depense. Un flottant et non un compteur de
    // battements : a 24 images pour 62,5 battements, le pas n'est pas entier.
    float creditImage = 0.0f;
    // Les encodeurs : compresser et ecrire un PNG coute dix fois le rendu du
    // widget, et ne demande aucun contexte graphique. Seul le rendu reste dans
    // le fil de l'interface.
    QThreadPool encodeurs;
    // Les places libres dans la file d'encodage. Quand il n'y en a plus, la
    // capture ATTEND au lieu de laisser tomber l'image : le jeu avance sur un
    // `dt` nominal et non sur le temps reellement ecoule (voir battement()),
    // donc une attente ne fait pas sauter la simulation -- elle ralentit la
    // session en temps reel, sans trouer la video. Laisser tomber des images,
    // au contraire, se verrait au montage.
    QSemaphore placesImages;
    // Le plus haut niveau atteint dans la partie en cours. Releve au fil de
    // l'eau : au game over le niveau retombe a celui de depart, le lire a ce
    // moment-la ne dirait rien.
    int niveauMax = 1;
    // La graine de la partie en cours, pour reconnaitre une partie NEUVE --
    // c'est la seule chose qui distingue un game over suivi d'un enchainement
    // d'un simple changement de manche.
    quint32 graineVue = 0;
    // La prise est faite : le bot a atteint le niveau vise et on ne touche plus
    // a rien. Voir terminerPrise().
    bool priseGardee = false;
    // Le debut de manche qui attend son numero d'image : "niveau graine vies
    // bombes", tel qu'il ira dans le releve. Vide quand il n'y a rien en
    // attente. Il ne s'ecrit pas a l'annonce mais a la premiere image qui
    // suit -- seule une image reellement ecrite peut porter un numero, et
    // c'est ce numero qui fait tout l'interet de la ligne.
    QString ligneManche;
    QTimer horloge;

    void rafraichir();
    // Ecrit le widget de jeu dans images/ a la racine du projet, un fichier par
    // changement de l'interface. C'est la matiere premiere de la video : le nom
    // est un numero de sequence sur six chiffres, ce qu'attend ffmpeg.
    void capturerImage();
    // Ajoute au releve de la prise la manche qui commence, en face du numero de
    // l'image ou elle commence. Voir ligneManche.
    void consignerManche(const QString &dossier, int image);
    // Efface les images et le releve de la prise precedente. Appelee une fois,
    // au premier fichier de la prise en cours.
    void viderLesImages(const QString &dossier);
    // Appelee au game over pendant un enregistrement : on garde la prise si le
    // bot a atteint le niveau vise, sinon on la jette et on recommence.
    void terminerPrise();
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
    // La partie est-elle finie pour de bon -- plus de vie, ou l'eponge jetee ?
    // C'est le seul cas ou plus rien ne repart tout seul.
    bool partieFinie() const;
    // Partie neuve, graine au hasard : le bot est reinstalle avec, comme au
    // changement de graine -- il tient un tas, un plan et des interdits
    // calcules sur un plateau qui n'existe plus.
    void nouvellePartie();
    // Le libelle du bouton de pause, qui en a quatre selon le moment :
    // "demarrer" avant le premier clic, "pause" pendant le jeu, "reprendre"
    // quand il est fige, et "nouvelle partie" une fois la partie finie.
    void majBoutonPause();
private slots:
    // Nom volontairement hors du motif on_<objet>_<signal> : l'horloge est
    // connectee explicitement, pas via connectSlotsByName.
    void battement();
    void pieceDeposee(int col, int row, bool accepte);
    // Un clic droit dans la grille. Rien a animer -- la bombe ne vient pas de
    // la file -- mais le stock a pu changer, et le panneau ne se repeint pas de
    // lui-meme pendant l'ecoulement.
    void bombePosee(int col, int row, bool accepte);
    void on_pbGlouton_clicked();
    void on_pbSpace_clicked();
    void on_pbSpaceAnticp_clicked();
    void on_pbMemoire_clicked();
    void on_pbTrace_clicked();
    void on_pbPause_clicked();
    void on_cbStep_toggled(bool actif);
    void on_pbGeste_clicked();
    void on_pbPose_clicked();
    void on_pbDefausse_clicked();
};
#endif // MAINWINDOW_H
