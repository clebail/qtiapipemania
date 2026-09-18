#include "mainwindow.h"
#include <QDir>
#include <QFile>
#include <QKeyEvent>
#include <QRunnable>
#include <QTextStream>
#include <QThread>

#include "botfactory.h"
#include "journal.h"
#include "serveurcontrole.h"

// Cadence des bots joues a l'ecran. Deux poses par seconde, c'est le milieu de
// la fourchette que BOT.md prete a un humain en reflexion continue -- et c'est
// surtout assez lent pour qu'on voie le bot penser, ce qui est tout l'interet de
// le regarder jouer avant de croire ses chiffres.
#define CADENCE_BOT             2.0f
// Une fois l'objectif securise, la manche est jouee : plus aucune decision, plus
// rien a regarder. On deroule la fin a cette vitesse plutot que de la subir. Ne
// s'applique qu'a un bot : un joueur humain, lui, construit encore son bonus.
#define ACCELERATION_ACQUISE    8
// Cadence d'enregistrement des images. Le jeu bat a 62,5 Hz ; filmer chaque
// battement ferait quarante mille fichiers par partie pour un rendu que l'oeil
// ne distingue pas. Une image toutes les 2,6 battements suffit.
#define IMAGES_PAR_SECONDE      24
// Images en attente d'ecriture, au plus. Chacune pese la taille logique du
// widget en RGB32 -- environ deux mega-octets et demi -- d'ou une file courte :
// elle sert a absorber les a-coups, pas a prendre de l'avance.
#define IMAGES_EN_ATTENTE       16
// Le PNG, et pas le JPEG : c'est mesure, sur la grille d'un niveau 38 bien
// remplie. L'image pese 73 ko en PNG, et le meme JPEG en fait 98 a qualite 92,
// 72 a 85, 62 a 80. Un ecran de jeu est un aplat sombre raye de traits nets --
// le terrain du PNG -- donc le JPEG n'y devient plus petit qu'en descendant
// assez bas pour abimer le trait, et pour un sixieme de place. Le volume d'une
// prise ne se joue pas la : deux heures de jeu font des dizaines de giga-octets
// dans tous les cas, quand le h264 en fait quinze mega-octets.
// Le releve des manches, dans le dossier des images. Sans lui la prise est
// illisible : le dossier ne dit pas ou commence un niveau, et deux heures
// d'images ne se parcourent pas a l'oeil.
#define FICHIER_MANCHES         "manches.txt"

namespace {

// L'ecriture d'une image, hors du fil de l'interface.
//
// C'est une QImage et non une QPixmap : la seconde ne se manipule que dans le
// fil principal, la premiere est une simple donnee. La conversion se fait donc
// a la capture, du bon cote de la frontiere.
class EcritureImage : public QRunnable {
public:
    EcritureImage(const QImage &image, const QString &nom, QSemaphore *places)
        : image(image), nom(nom), places(places) {
    }

    void run() override {
        if(!image.save(nom)) {
            qWarning("capture : echec de l'ecriture de %s", qUtf8Printable(nom));
        }

        // La place se rend meme en cas d'echec : sinon un disque plein figerait
        // la fenetre au lieu de se contenter de rater ses images.
        places->release();
    }

private:
    QImage image;
    QString nom;
    QSemaphore *places;
};

}   // namespace

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), horloge() {
    setupUi(this);

    p = new Partie(COLONNES_PLATEAU, LIGNES_PLATEAU);

    // Apres la partie, qu'il tient -- d'ou le pointeur plutot qu'un membre
    // construit dans la liste d'initialisation, ou l'ordre depend de l'ordre de
    // declaration et non de celui qu'on ecrit.
    controle = new Controle(p, this);
    serveur = new ServeurControle(controle, this);
    connect(serveur, &ServeurControle::etatChange, this, &MainWindow::serveurChange);

    game->setPartie(p);
    panneau->setPartie(p);

    // Les boutons de bot s'excluent, et leur etat enfonce dit lequel joue.
    pbGlouton->setCheckable(true);
    pbSpace->setCheckable(true);
    pbSpaceAnticp->setCheckable(true);
    pbMemoire->setCheckable(true);
    pbTrace->setCheckable(true);

    // Sans ca, un bouton garde le focus apres le clic et avale la barre
    // d'espace : elle rejouerait le bouton au lieu de lancer le flux.
    pbGlouton->setFocusPolicy(Qt::NoFocus);
    pbSpace->setFocusPolicy(Qt::NoFocus);
    pbSpaceAnticp->setFocusPolicy(Qt::NoFocus);
    pbMemoire->setFocusPolicy(Qt::NoFocus);
    pbTrace->setFocusPolicy(Qt::NoFocus);
    pbPause->setFocusPolicy(Qt::NoFocus);
    pbGeste->setFocusPolicy(Qt::NoFocus);
    pbPose->setFocusPolicy(Qt::NoFocus);
    pbDefausse->setFocusPolicy(Qt::NoFocus);

    connect(game, &WGame::pieceDeposee, this, &MainWindow::pieceDeposee);
    connect(game, &WGame::bombePosee, this, &MainWindow::bombePosee);
    connect(cbMEETas, &QCheckBox::toggled, game, &WGame::setAfficherTas);
    game->setAfficherTas(cbMEETas->isChecked());
    connect(cbPlan, &QCheckBox::toggled, game, &WGame::setAfficherPlan);
    game->setAfficherPlan(cbPlan->isChecked());

    connect(cbTrace, &QCheckBox::toggled, game, &WGame::setAfficherTrace);
    game->setAfficherTrace(cbTrace->isChecked());
    majPasAPas();

    game->setFixedSize(p->plateau()->getLargeur() * tailleCase(),
                       p->plateau()->getHauteur() * tailleCase());

    // La fenetre se dimensionne d'elle-meme autour de la grille : inutile de
    // recalculer sa taille a la main a chaque widget ajoute (barre de boutons...).
    centralWidget()->layout()->setSizeConstraint(QLayout::SetFixedSize);

    // Un fil de moins que la machine : celui de l'interface a du travail, et
    // c'est lui qui doit rester fluide.
    encodeurs.setMaxThreadCount(qMax(1, QThread::idealThreadCount() - 1));
    placesImages.release(IMAGES_EN_ATTENTE);

    graineVue = p->getGraine();
    niveauMax = p->niveau();

    // La partie attend : le bouton dit donc "demarrer" et non "reprendre". Voir
    // MainWindow::enPause -- on regle tout (bot, enregistrement) avant que quoi
    // que ce soit ne bouge.
    majBoutonPause();

    // Une seule horloge pour tout le jeu : c'est Partie qui sait, selon son
    // etat, s'il faut decompter avant le depart ou faire avancer le flux.
    horloge.setInterval(16);
    connect(&horloge, &QTimer::timeout, this, &MainWindow::battement);
    horloge.start();

    rafraichir();
}

MainWindow::~MainWindow() {
    // Le serveur d'abord : il tient un pointeur sur le controle, qui tient la
    // partie qu'on s'apprete a detruire.
    delete serveur;
    serveur = nullptr;
    delete controle;
    controle = nullptr;

    // Les dernieres images ensuite : elles tiennent un semaphore de cet objet,
    // et une prise se termine entiere ou ne sert a rien.
    encodeurs.waitForDone();

    // Le bot ensuite : il tient un pointeur sur la partie.
    game->setBot(nullptr);
    delete bot;
    delete journal;
    delete p;
}

// Le bot est reinstalle apres coup : il tient un tas et un plan de defausse
// calcules sur le plateau precedent, qui vient d'etre remplace.
void MainWindow::setNiveauDepart(int niveau) {
    p->setNiveauDepart(niveau);

    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    rafraichir();
}

// Le bot est reinstalle pour la meme raison qu'au changement de niveau : son
// tas et son plan de defausse valaient pour le plateau precedent.
void MainWindow::setGraine(quint32 graine) {
    p->nouvellePartie(graine);

    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    rafraichir();
}

// Pas de bot a reinstaller ici, au contraire des deux reglages precedents : ni
// le plateau ni la file ne changent, seuls les compteurs bougent.
void MainWindow::setVies(int vies) {
    p->setViesDepart(vies);
    rafraichir();
}

void MainWindow::setBombes(int bombes) {
    p->setBombesDepart(bombes);
    rafraichir();
}

quint32 MainWindow::graine() const {
    return p->getGraine();
}

void MainWindow::ouvrirJournal(const QString &chemin) {
    delete journal;
    journal = new Journal(chemin);
}

// Un depot du joueur. Le coup refuse compte aussi : c'est un geste, il occupe la
// main et il pese dans la cadence.
void MainWindow::pieceDeposee(int col, int row, bool accepte) {
    if(accepte) {
        panneau->animerDepilage();
    }

    if(journal != nullptr) {
        journal->geste(p, tempsSimule_, col, row, accepte, false);
    }

    rafraichir();
}

// Poser une bombe n'est pas un geste de construction : la file ne bouge pas, et
// le journal, qui suit les poses de pieces, n'a rien a en dire. Reste a
// rafraichir le panneau, ou le stock vient de descendre d'un cran.
void MainWindow::bombePosee(int, int, bool) {
    rafraichir();
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // Troisieme porte fermee quand un script pilote : la barre d'espace engage
    // la manche pour de bon (elle lance le flux et fait foncer, sans retour).
    // Le script a la commande `espace` pour ca -- et lui seul.
    if(cbServeur->isChecked()) {
        QMainWindow::keyPressEvent(event);
        return;
    }

    if(event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        // Espace veut dire "je n'attends pas", et c'est au joueur d'en decider.
        // Aucun test d'etat ici : la touche fait tout ce qu'elle peut faire, et
        // chaque action se refuse d'elle-meme la ou elle n'a pas de sens. Avant
        // le depart on lance le flux contre la prime, pendant la pause de fin de
        // manche on enchaine -- et dans tous les cas on fonce.
        //
        // Ca ne se reprend pas. Acceleree, la manche ne laisse plus le temps de
        // poser : le jeu avance huit fois plus vite, la main du joueur non, et
        // il perd donc les cases qu'il aurait construites devant le flux. C'est
        // le meme marche que la prime de depart -- du temps de construction
        // echange contre autre chose -- donc le meme engagement, sans retour.
        foncer();
        rafraichir();

        return;
    }

    QMainWindow::keyPressEvent(event);
}

void MainWindow::foncer() {
    p->lancerFluxAnticipe();
    p->passerLaSuite();
    accelereManuel = true;
}

void MainWindow::annoncerManche() {
    if(p->numeroManche() == mancheAnnoncee) {
        return;
    }

    mancheAnnoncee = p->numeroManche();

    // Vies et bombes comptent dans la commande au meme titre que la graine :
    // elles ne changent pas le plateau, mais elles changent ce que le bot en
    // fait -- le veto de la memoire cede sur la derniere vie, et une bombe en
    // stock ouvre des coups qui n'existent pas sans elle. Relevees ici, donc au
    // PREMIER battement de la manche : le stock d'apres coup ne rejouerait pas
    // la meme chose.
    qInfo("--- manche : niveau %d, objectif %d, %d vie(s), %d bombe(s)"
          "   |   rejouer avec :  --graine %u --niveau %d --vies %d --bombes %d",
          p->niveau(), p->longueurMinimale(), p->vies(), p->bombes(),
          p->getGraine(), p->niveau(), p->vies(), p->bombes());

    // De quoi consigner ce debut de manche des que la prochaine image lui
    // donnera un numero -- voir consignerManche(). Releve MAINTENANT, au
    // premier battement : c'est la que les vies et les bombes sont encore
    // celles qui rejoueraient la manche a l'identique.
    ligneManche = QString("%1 %2 %3 %4").arg(p->niveau(), 6)
                                        .arg(p->getGraine(), 10)
                                        .arg(p->vies(), 4)
                                        .arg(p->bombes(), 6);
}

void MainWindow::rafraichir() {
    // Le bouton change de metier a la fin de la partie : c'est ici qu'on s'en
    // apercoit, rafraichir() etant appelee a chaque changement d'etat.
    majBoutonPause();

    depart->setFraction(p->fractionAvantDepart());
    panneau->update();
    // La grille aussi : un changement d'etat peut signifier un plateau neuf.
    game->update();

    capturerImage();
}

// Fin de partie pendant un enregistrement : on ferme la prise.
//
// LA DERNIERE IMAGE EST LE VERDICT, et c'est tout l'objet de cette fonction.
// La partie est deja dans son etat final quand on arrive ici -- la grille
// dessine donc GAME OVER ou ABANDON par-dessus le plateau -- mais le credit
// d'images n'a aucune raison d'etre arrive a un compte rond a cet instant
// precis. Sans la prise forcee ci-dessous, la video se termine sur une image
// quelconque prise jusqu'a un quarantieme de seconde plus tot, c'est-a-dire
// sur le plateau sans son verdict.
//
// Plus de critere de niveau. La prise se gardait autrefois sous condition --
// niveau 40 atteint, sinon on recommencait toute la nuit -- et la partie
// perdue enchainait d'elle-meme sur une neuve. Les deux sont tombes ensemble :
// la fin de partie reste desormais a l'ecran, et c'est elle qu'on veut filmer.
// Ce qui est enregistre est garde.
void MainWindow::terminerPrise() {
    // Un credit plein, et on prend : capturerImage refuse en dessous de un, et
    // ce dernier sursaut de cadence ne se voit pas sur une image fixe qu'on
    // regarde a la fin d'une video.
    creditImage = 1.0f;
    capturerImage();

    priseGardee = true;
    cbImages->setChecked(false);

    // Et on fige. Plus rien ne repart tout seul depuis que la fin de partie
    // reste a l'ecran, mais la pause coupe aussi le bot et les
    // rafraichissements : la fenetre attend vraiment, jusqu'au matin s'il le
    // faut, sur l'image qui termine la video.
    enPause = true;
    majBoutonPause();

    qInfo("=== PRISE GARDEE : niveau %d, %d images dans images/"
          "   |   la partie :  --graine %u", niveauMax, imageSuivante, graineVue);
}

// Efface la prise precedente, au premier fichier de la nouvelle.
//
// Sans ca, une prise plus courte que la precedente laisserait derriere elle la
// fin de l'ancienne -- meme dossier, meme numerotation repartie de zero -- et
// ffmpeg enchainerait les deux sans rien signaler. On ne le fait donc qu'une
// fois, au debut, et jamais en cours de prise : decocher puis recocher la case
// poursuit la meme sequence au lieu de detruire ce qui est deja filme.
//
// Le filtre ne vise QUE nos fichiers -- six chiffres et .png, plus le releve.
// Ce que quelqu'un aurait range dans ce dossier ne nous appartient pas.
//
// Le releve des manches part avec les images : il numerote celles qu'on
// efface, le garder ferait pointer ses lignes sur les images de la prise
// suivante.
void MainWindow::viderLesImages(const QString &dossier) {
    QDir rep(dossier);

    foreach(const QString &fichier,
            rep.entryList(QStringList() << "??????.png" << FICHIER_MANCHES,
                          QDir::Files)) {
        if(!rep.remove(fichier)) {
            qWarning("capture : impossible d'effacer %s", qUtf8Printable(fichier));
        }
    }
}

// Consigne dans le releve la manche qui vient de commencer, en face du numero
// de l'image ou elle commence.
//
// C'est ce qui rend la prise exploitable. Le numero d'image donne le point
// d'entree dans la sequence -- de quoi couper l'extrait qu'on veut sans
// parcourir deux heures a l'oeil -- et graine, vies et bombes donnent de quoi
// REJOUER cette manche-la, seule, au lieu de refaire la nuit entiere.
//
// Ouvert et referme a chaque ligne : une session s'arrete en general d'un
// Ctrl-C ou d'une croix, jamais proprement, et un tampon en attente
// n'arriverait alors jamais sur le disque.
void MainWindow::consignerManche(const QString &dossier, int image) {
    QFile fichier(dossier + "/" + FICHIER_MANCHES);

    if(!fichier.open(QIODevice::Append | QIODevice::Text)) {
        qWarning("capture : impossible d'ecrire %s",
                 qUtf8Printable(fichier.fileName()));
        return;
    }

    QTextStream flux(&fichier);

    // L'en-tete au premier passage seulement. viderLesImages a supprime le
    // releve de la prise precedente, donc un fichier vide est un fichier neuf.
    if(fichier.size() == 0) {
        flux << "# image niveau     graine vies bombes\n";
    }

    // Six chiffres, comme le nom du fichier : le numero du releve se recopie
    // tel quel dans "images/......png", sans rien avoir a recompter.
    flux << QString("%1 %2\n").arg(image, 6, 10, QChar('0')).arg(ligneManche);
}

// Une image par changement de l'interface, dans images/ a la racine du projet.
//
// C'est `grab()` et non une copie d'ecran : il rend le widget hors ecran, donc
// il ignore ce qui le recouvre, la fenetre peut etre derriere une autre ou
// meme minimisee, et l'image ne depend pas du facteur d'echelle de l'ecran.
// Les paintEvent du jeu ne lisent que l'etat de la partie, ils n'ecrivent rien
// -- ce repaint supplementaire est donc sans effet sur le jeu.
//
// `update()` juste au-dessus ne fait que PLANIFIER un repaint : la capture,
// elle, peint tout de suite. Elle voit donc le meme etat que le repaint a
// venir, et non celui d'avant.
void MainWindow::capturerImage() {
    // En pause, rien ne bouge : les rafraichissements qui restent viennent des
    // reglages (choix du bot, cases a cocher), et les enregistrer mettrait dans
    // la sequence des images d'avant la partie. La prise commence au clic.
    //
    // Le credit retombe a zero plutot que de s'accumuler : sans ca, cocher la
    // case en cours de partie deverserait d'un coup toutes les images qu'on
    // n'avait pas prises.
    // La prise gardee ne se laisse plus toucher : recocher la case par megarde
    // ne doit pas effacer la partie qu'on a attendue toute la nuit.
    if(priseGardee || !cbImages->isChecked() || enPause) {
        creditImage = 0.0f;
        // La manche annoncee n'aura aucune image a designer : la garder ferait
        // pointer son numero sur une image d'une autre manche le jour ou on
        // recocherait la case.
        ligneManche.clear();
        return;
    }

    // Une image par unite de credit, et on garde le reste : la cadence tombe
    // juste a la longue, la ou un compteur de battements entier deriverait
    // (2,6 battements par image, ca ne se compte pas en entiers).
    if(creditImage < 1.0f) {
        return;
    }

    creditImage -= 1.0f;

    static const QString dossier = QStringLiteral(RACINE_PROJET) + "/images";

    if(imageSuivante == 0) {
        if(!QDir().mkpath(dossier)) {
            qWarning("capture : impossible de creer %s", qUtf8Printable(dossier));
            return;
        }

        viderLesImages(dossier);
    }

    // Six chiffres et un pas de un : c'est la sequence que ffmpeg lit sans
    // qu'on ait a lui expliquer quoi que ce soit. Le numero est relu juste
    // apres pour le releve, d'ou la variable plutot que l'increment en place.
    const int numero = imageSuivante++;
    QString nom = QString("%1/%2.png").arg(dossier)
                                      .arg(numero, 6, 10, QChar('0'));

    // Une manche attend son numero : c'est cette image, la premiere ecrite
    // depuis son premier battement.
    if(!ligneManche.isEmpty()) {
        consignerManche(dossier, numero);
        ligneManche.clear();
    }

    // Rendu a la taille LOGIQUE du widget, pas a celle de l'ecran. Sur un ecran
    // a facteur deux, grab() rendrait quatre fois plus de pixels pour rien : le
    // cache de sprites (dessinpiece.cpp) est rempli a la taille logique de la
    // tuile, donc l'affichage les agrandit deja. Rendre ici a 1:1 donne des
    // pieces a leur resolution native, et quatre fois moins de travail.
    //
    // Le rendu reste dans ce fil -- Qt ne dessine que la -- et c'est la partie
    // rapide. La QImage passe de l'autre cote de la frontiere ; la compression,
    // qui coute dix fois plus, part au pool.
    QImage image(widget->size(), QImage::Format_RGB32);

    // Le fond d'abord : sans alpha, une zone que le widget ne peindrait pas
    // resterait indefinie.
    image.fill(palette().color(QPalette::Window));
    widget->render(&image);

    // Plus de place : on attend qu'un encodeur se libere. Voir placesImages --
    // attendre ralentit la session, jamais la video.
    placesImages.acquire();
    encodeurs.start(new EcritureImage(image, nom, &placesImages));
}

// Accelerer, c'est jouer PLUSIEURS battements de 16 ms, jamais un battement plus
// gros : Ecoulement quantifie le remplissage a chaque appel, donc un dt multiplie
// ne donnerait pas la meme partie. La simulation reste celle du jeu, tick pour
// tick, on la deroule seulement plus vite que le temps reel.
int MainWindow::facteurTemps() const {
    if(p->etat() != epEcoulement) {
        return 1;
    }

    // Barre d'espace, ou bot qui n'a plus rien a construire et l'a demandee :
    // on deroule la fin a vitesse acceleree, il n'y a plus de decision a voir.
    if(accelereManuel) {
        return ACCELERATION_ACQUISE;
    }

    // Sinon le flux avance au temps reel -- que le joueur construise encore
    // devant lui ou qu'on regarde un bot le faire. Jamais zero : ca figerait la
    // manche tant que personne ne demande a accelerer, et le flux parti de
    // lui-meme a l'expiration du delai n'aurait alors plus rien pour repartir.
    return 1;
}

void MainWindow::battement() {
    annoncerManche();

    // Le pas a pas fige la partie comme la pause : plus de delai qui s'ecoule,
    // plus de flux qui avance, plus de bot qui joue de lui-meme. C'est tout
    // l'interet du mode -- deplier une manche geste par geste, sans le
    // chronometre qui decide a votre place.
    if(enPause || cbStep->isChecked()) {
        return;
    }

    EEtatPartie avant = p->etat();
    float dt = horloge.interval() / 1000.0f;

    // Le droit a une image, accumule au rythme de l'horloge -- ici et pas dans
    // battementUnitaire : c'est une fois par battement d'HORLOGE que l'ecran
    // change, l'acceleration jouant plusieurs pas de jeu dans le meme instant
    // affiche. Filmer les pas ferait defiler la fin de manche huit fois trop
    // vite dans la video.
    creditImage += IMAGES_PAR_SECONDE * dt;

    // L'acceleration demandee ne vaut que pour la manche en cours : elle tombe
    // des que le flux s'arrete, sans quoi la suivante demarrerait lancee.
    if(avant != epEcoulement) {
        accelereManuel = false;
    }

    // Le facteur se calcule une fois pour toute la rafale : il decide aussi si
    // le bot joue, et l'interroger dans la boucle le ferait changer d'avis en
    // cours de route.
    int facteur = facteurTemps();

    for(int pas=facteur; pas>0; pas--) {
        // Accelerer, c'est n'avoir plus rien a regarder : le bot pose les mains
        // pour de bon. Sans ca il joue huit fois plus vite a l'ecran, et la fin
        // de manche qu'on voulait rendre lisible devient illisible.
        //
        // Le prix est celui que paie deja un joueur humain -- le jeu avance huit
        // fois plus vite, pas sa main, donc il perd les cases qu'il aurait
        // construites devant le flux. Le marche est ainsi le meme pour les deux :
        // on fonce, on ne construit plus.
        battementUnitaire(dt, facteur == 1);
    }

    // Une fois la rafale finie, et non a chaque battement unitaire : les bombes
    // qui sautent dans la meme rafale tiennent le meme instant a l'ecran, ce
    // qui est exactement ce qu'on veut a 8x -- huit pas de simulation, un seul
    // instant vecu. La grille arme ses flashs et les anime toute seule : la
    // fenetre cesse de repeindre des que la manche est finie, et c'est le sort
    // de l'explosion qui tue.
    game->releverExplosions();

    // Fin de manche : c'est la que le journal peut dire quels gestes ont servi.
    if(journal != nullptr && p->etat() != avant
       && (p->etat() == epReussie || p->etat() == epPerdue || p->etat() == epGameOver
           || p->etat() == epAbandon)) {
        journal->finDeManche(p);
    }

    if(p->etat() == epEcoulement) {
        game->repaint();
    }

    // --- la prise de la nuit --------------------------------------------
    //
    // Releve au fil de l'eau : le niveau retombe a sa valeur de depart au game
    // over, donc le lire a ce moment-la ne dirait rien de ce que la partie a
    // accompli.
    if(p->etat() == epEcoulement || p->etat() == epAttente) {
        niveauMax = qMax(niveauMax, p->niveau());
    }

    // Partie NEUVE : la graine change, et elle seule le dit -- le numero de
    // manche monte aussi bien au rejeu qu'au niveau suivant. La prise repart
    // donc de zero, ce qui videra le dossier a l'image suivante.
    if(p->getGraine() != graineVue) {
        graineVue = p->getGraine();
        niveauMax = p->niveau();
        imageSuivante = 0;
        // La manche annoncee au debut de ce battement est celle de la partie
        // qui vient de mourir : son numero d'image n'existe plus, la
        // numerotation venant de repartir de zero.
        ligneManche.clear();
        // Et la partie neuve doit s'annoncer, meme si elle porte le meme
        // numero de manche que celle qui l'a precedee -- au game over en
        // premiere manche, le releve n'aurait sinon aucune ligne.
        mancheAnnoncee = -1;
    }

    // Fin de partie, quelle qu'elle soit : la prise se ferme. L'abandon compte
    // autant que le game over -- c'est la meme derniere image, et plus rien ne
    // viendra apres.
    bool finieAvant = avant == epGameOver || avant == epAbandon;
    bool finieApres = p->etat() == epGameOver || p->etat() == epAbandon;

    if(!finieAvant && finieApres && cbImages->isChecked()) {
        terminerPrise();
    }

    // Le statut et la jauge ne bougent qu'en attente ou sur changement d'etat :
    // inutile de les reecrire soixante fois par seconde une fois la manche finie.
    // Avec un bot en revanche, le plateau et la file changent a n'importe quel
    // battement sans qu'aucun signal ne l'annonce : poserPiece() n'est pas
    // WGame::pieceDeposee.
    //
    // Un SCRIPT est exactement dans la meme position : le serveur de controle
    // appelle Partie::poserPiece sans passer par la grille. Sans cette clause,
    // la file et les compteurs se figeaient des le depart du flux -- ils ne se
    // rafraichissaient qu'en attente, ou l'etat suffit a declencher le repaint.
    if(p->etat() != avant || p->etat() == epAttente
       || bot != nullptr || cbServeur->isChecked()) {
        rafraichir();
    }
}

void MainWindow::battementUnitaire(float dt, bool joueLeBot) {
    tempsSimule_ += dt;

    if(bot != nullptr && joueLeBot) {
        Piece sommet = p->file()->getPiece(0);
        // Le bot ne dit pas ou il pose : on le lit sur le plateau, par
        // difference. Une copie de 225 octets par battement, et seulement quand
        // le journal est ouvert -- plutot que d'imposer a l'API des bots une
        // obligation de compte-rendu qui ne sert qu'a la mesure.
        Game avant(*p->plateau());
        // Et les marques du tas, pour la meme raison et avec la meme reserve :
        // origineTas dit ce que la case PORTE, pas qui vient de poser. Le trace
        // qui reprend un rebut laisse la marque en place jusqu'au geste suivant,
        // donc sans l'avant une reprise se lirait comme une defausse -- et c'est
        // exactement la distinction que la colonne `origine` existe pour faire.
        QVector<unsigned char> tasAvant;

        if(journal != nullptr) {
            tasAvant.resize(avant.getSize());

            for(int i = 0; i < avant.getSize(); i++) {
                tasAvant[i] = bot->origineTas(i % avant.getLargeur(),
                                              i / avant.getLargeur());
            }
        }

        // Le bot joue avant Partie::avancer(), comme le clic du joueur precede
        // le battement suivant : c'est l'ordre du banc d'essai, et les deux
        // doivent derouler exactement la meme partie.
        bot->avancer(dt);

        Piece suivante = p->file()->getPiece(0);

        // La file a bouge, donc une piece a ete posee. Le panneau se repeint de
        // toute facon sur l'etat courant : l'animation n'est que du confort, et
        // la manquer quand deux pieces identiques se suivent ne se voit pas.
        if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
            panneau->animerDepilage();

            if(journal != nullptr) {
                for(int i=0; i<avant.getSize(); i++) {
                    int col = i % avant.getLargeur();
                    int row = i / avant.getLargeur();

                    if(avant.getTypePiece(col, row) != p->plateau()->getTypePiece(col, row)
                       || avant.getSens(col, row) != p->plateau()->getSens(col, row)) {
                        unsigned char apres = bot->origineTas(col, row);
                        unsigned char origine;

                        if(avant.getTypePiece(col, row) == tpNone) {
                            // Case vierge : la marque du tas ne peut venir que
                            // de la pose qu'on est en train de consigner.
                            origine = apres;
                        } else if(apres != 0 && apres != tasAvant.at(i)) {
                            // La marque a change : une defausse a ecrase une
                            // piece. C'est la defausse qu'on compte.
                            origine = apres;
                        } else {
                            // Marque inchangee sur une case occupee : le trace
                            // reprend un rebut, ou une pose en remplace une
                            // autre. Ni l'un ni l'autre n'est un placement neuf.
                            origine = 4;
                        }

                        journal->geste(p, tempsSimule_, col, row, true, true, origine);
                        break;
                    }
                }
            }
        }

        // Le bot n'a plus de coup utile : on lui accorde la barre d'espace,
        // exactement comme au joueur qui appuierait dessus.
        if(bot->veutFoncer()) {
            foncer();
        }
    }

    p->avancer(dt);
}

// Nom du bot enfonce, vide si c'est le joueur qui tient la souris.
QString MainWindow::nomBotCourant() const {
    if(pbGlouton->isChecked())     return "glouton";
    if(pbSpace->isChecked())       return "space";
    if(pbSpaceAnticp->isChecked()) return "spaceAnticp";
    if(pbMemoire->isChecked())     return "memoire";
    if(pbTrace->isChecked())       return "trace";

    return QString();
}

void MainWindow::installerBot(const QString &nom) {
    delete bot;

    bot = nom.isEmpty() ? nullptr
                        : BotFactory::createInstance(nom, p, CADENCE_BOT, p->getGraine());

    // Ici et nulle part ailleurs : c'est la fenetre qui a une image a ne pas
    // figer. Le banc, lui, garde le calcul dans son thread -- deux mesures de
    // la meme graine doivent rendre le meme chiffre, et un bot qui depend de
    // l'instant ou un thread rend la main ne le peut pas.
    if(bot != nullptr) {
        bot->setPlanificationAsynchrone(true);
    }

    // La grille n'affiche l'overlay du tas que tant qu'un bot joue.
    game->setBot(bot);

    pbGlouton->setChecked(nom == "glouton");
    pbSpace->setChecked(nom == "space");
    pbSpaceAnticp->setChecked(nom == "spaceAnticp");
    pbMemoire->setChecked(nom == "memoire");
    pbTrace->setChecked(nom == "trace");

    majPasAPas();
}

// Le bouton est bascule : un deuxieme clic rend la main au joueur, et cliquer
// l'autre bot remplace celui qui jouait.
void MainWindow::on_pbGlouton_clicked() {
    installerBot(pbGlouton->isChecked() ? "glouton" : QString());
}

void MainWindow::on_pbSpace_clicked() {
    installerBot(pbSpace->isChecked() ? "space" : QString());
}

void MainWindow::on_pbSpaceAnticp_clicked() {
    installerBot(pbSpaceAnticp->isChecked() ? "spaceAnticp" : QString());
}

void MainWindow::on_pbMemoire_clicked() {
    installerBot(pbMemoire->isChecked() ? "memoire" : QString());
}

void MainWindow::on_pbTrace_clicked() {
    installerBot(pbTrace->isChecked() ? "trace" : QString());
}

// --- HoteControle -----------------------------------------------------------

void MainWindow::mettreEnPause(bool pause) {
    enPause = pause;
    demarre = demarre || !pause;
    // Le libelle a quatre etats depuis que la fin de partie reste a l'ecran :
    // c'est majBoutonPause qui les connait, ici on ne fait que la declencher.
    majBoutonPause();
}

bool MainWindow::estEnPause() const {
    return enPause;
}

// Avance de n battements, exactement comme le ferait l'horloge -- meme dt
// nominal, meme ordre, meme acceleration. C'est ce qui rend une partie pilotee
// rejouable : le script decide du temps, pas le reseau.
//
// Le bot ne joue pas : verrouillerPourServeur l'a retire. Le seul joueur est
// celui qui envoie les commandes.
void MainWindow::avancerDeBattements(int n) {
    float dt = horloge.interval() / 1000.0f;

    for(int i = 0; i < n; i++) {
        battementUnitaire(dt, false);
    }

    rafraichir();
}

void MainWindow::espace() {
    foncer();
    rafraichir();
}

// Le temps de jeu, pas celui de la montre : il monte de 16 ms par battement,
// donc il vaut la meme chose que le jeu tourne a sa vitesse ou qu'un script
// l'avance pas a pas. C'est ce qui permet de tarifer les gestes pareil dans les
// deux regimes.
float MainWindow::tempsSimule() const {
    return this->tempsSimule_;
}

// L'horloge appartient au client tant que personne ne pilote. Des qu'un script
// est aux commandes, elle lui echappe : le jeu tourne en temps reel, et sa
// reflexion se paie comme celle d'un joueur.
bool MainWindow::horlogeVerrouillee() const {
    return cbServeur->isChecked();
}

// Le jeu attend un client, puis repart. Figer pendant l'attente evite que le
// script herite d'un plateau deja entame ; le relacher a la connexion fait que
// la partie commence avec lui, et pas avant.
void MainWindow::serveurChange() {
    if(cbServeur->isChecked()) {
        mettreEnPause(!serveur->clientConnecte());
    }

    rafraichir();
}

void MainWindow::partieRemplacee() {
    // Le bot tenait un tas et un plan calcules sur le plateau precedent. En
    // mode serveur il n'y en a pas, mais cette methode sert aussi hors serveur.
    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    mancheAnnoncee = -1;
    graineVue = p->getGraine();
    niveauMax = p->niveau();
    rafraichir();
}

// Les trois portes par lesquelles on entre dans le jeu : la souris sur la
// grille, le clavier, et le bot. Quand un script pilote, elles se ferment
// toutes -- sinon deux joueurs poseraient dans la meme partie sans le savoir,
// et le script verrait un plateau qu'il n'a pas construit.
void MainWindow::verrouillerPourServeur(bool verrouille) {
    game->setJouable(!verrouille);

    if(verrouille) {
        installerBot(QString());
    }

    pbGlouton->setEnabled(!verrouille);
    pbSpace->setEnabled(!verrouille);
    pbSpaceAnticp->setEnabled(!verrouille);
    pbMemoire->setEnabled(!verrouille);

    // Le bouton de pause aussi : c'est le script qui decide desormais quand le
    // temps passe, et deux mains sur le meme levier ne donnent rien de bon.
    pbPause->setEnabled(!verrouille);
    cbStep->setEnabled(!verrouille);

    majPasAPas();
}

// La case a cocher du serveur. Des qu'elle est cochee, le jeu se fige et attend
// une connexion : on ne veut pas que la partie coure pendant que le script se
// connecte, sinon il herite d'un plateau deja entame.
void MainWindow::on_cbServeur_toggled(bool actif) {
    if(actif) {
        if(!serveur->demarrer()) {
            cbServeur->setChecked(false);
            return;
        }

        // En attente d'un client : on fige. serveurChange() relachera des qu'il
        // sera la.
        mettreEnPause(true);
        verrouillerPourServeur(true);
    } else {
        serveur->arreter();
        verrouillerPourServeur(false);
        mettreEnPause(true);
    }

    rafraichir();
}

// Simple bascule : un clic fige la partie (bot compris, battement() ne fait
// plus rien), le suivant la relache. Sert a immobiliser l'ecran le temps d'une
// copie d'ecran -- et, au lancement, a tout regler avant que rien ne parte.
//
// Le premier clic DEMARRE : la fenetre s'ouvre en pause, donc le bot choisi ne
// joue pas encore et aucune image n'est enregistree. C'est ce qui rend la prise
// synchrone -- la premiere image est le plateau intact, pas un plateau ou le
// bot a deja pose trois pieces pendant qu'on cochait les cases.
void MainWindow::on_pbPause_clicked() {
    // PARTIE FINIE : le bouton change de metier. Mettre en pause ce qui ne
    // bouge plus n'a aucun sens, et depuis que le game over reste a l'ecran il
    // fallait bien un moyen de repartir -- autant celui-la, qui est deja sous
    // la main et deja le bouton du "on y va".
    if(partieFinie()) {
        nouvellePartie();
        return;
    }

    // Et on passe par mettreEnPause plutot que de basculer le drapeau ici :
    // c'est aussi le point d'entree de l'API de controle, les deux doivent
    // faire exactement la meme chose.
    mettreEnPause(!enPause);
}

bool MainWindow::partieFinie() const {
    return p->etat() == epGameOver || p->etat() == epAbandon;
}

void MainWindow::nouvellePartie() {
    p->nouvellePartie();

    // Le bouton promet une partie, pas un plateau immobile : on releve la
    // pause. Elle est le plus souvent deja levee -- sauf apres une prise
    // gardee, qui fige tout, et c'est justement la qu'un clic veut dire "on
    // repart".
    enPause = false;
    demarre = true;

    // Meme raison qu'au changement de graine : le bot tient un tas, un plan de
    // defausse et des interdits calcules sur le plateau d'avant.
    if(bot != nullptr) {
        installerBot(nomBotCourant());
    }

    rafraichir();
}

void MainWindow::majBoutonPause() {
    QString libelle = partieFinie() ? tr("nouvelle partie")
                    : enPause       ? (demarre ? tr("reprendre") : tr("démarrer"))
                                    : tr("pause");

    // Le meme texte ne se repose pas : rafraichir() passe ici a chaque
    // battement des qu'un bot joue, et setText repeint le bouton.
    if(pbPause->text() != libelle) {
        pbPause->setText(libelle);
    }
}

// Les deux boutons de geste n'ont de sens qu'en pas a pas, et il faut un bot
// pour les executer : ce sont ses primitives -- sa tete de construction, son
// plan de defausse -- que la fenetre declenche, pas des coups improvises.
void MainWindow::majPasAPas() {
    bool actif = cbStep->isChecked() && bot != nullptr;

    pbGeste->setEnabled(actif);
    pbPose->setEnabled(actif);
    pbDefausse->setEnabled(actif);

    // Hors du mode, un vecteur vide : l'overlay s'eteint. Le calcul n'a lieu
    // que la ou on le regarde.
    game->setRegion(actif ? bot->regionApresPoseSurTete() : QVector<unsigned char>());
}

void MainWindow::on_cbStep_toggled(bool actif) {
    // Les deux gestes du pas a pas sont ceux d'un bot -- sa tete de
    // construction, son plan de defausse. Cocher la case sans bot ne donnerait
    // donc que des boutons eteints, ce qui ressemble a une panne : on installe
    // le dernier bot en date, et son bouton s'enfonce pour dire lequel joue.
    if(actif && bot == nullptr) {
        installerBot("spaceAnticp");
        rafraichir();

        return;
    }

    majPasAPas();
    rafraichir();
}

// Le bot joue un geste, le sien : c'est sa strategie qu'on regarde, et c'est le
// seul des trois boutons qui montre ce qu'il ferait tout seul. veutFoncer() est
// volontairement ignore ici -- foncer lancerait le flux et l'accelererait, ce
// qui est exactement ce que le pas a pas sert a eviter.
void MainWindow::on_pbGeste_clicked() {
    if(bot == nullptr) {
        return;
    }

    Piece sommet = p->file()->getPiece(0);
    bot->unGeste();
    Piece suivante = p->file()->getPiece(0);

    if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}

// Pose sur la tete de construction, sans demander son avis au bot : en pas a
// pas c'est l'oeil qui juge, et une pose qui se condamne est justement ce
// qu'on veut pouvoir provoquer pour voir la region se fermer.
void MainWindow::on_pbPose_clicked() {
    if(bot == nullptr) {
        return;
    }

    if(bot->poserSurTete()) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}

// Defausse comme le bot le ferait de lui-meme, plan de defausse compris. Le
// bot ne rend pas la main : on lit le depilage sur la file, comme
// battementUnitaire() lit une pose sur le plateau.
void MainWindow::on_pbDefausse_clicked() {
    if(bot == nullptr) {
        return;
    }

    Piece sommet = p->file()->getPiece(0);
    bot->defausser();
    Piece suivante = p->file()->getPiece(0);

    if(suivante.type != sommet.type || suivante.sens != sommet.sens) {
        panneau->animerDepilage();
    }

    majPasAPas();
    rafraichir();
}
