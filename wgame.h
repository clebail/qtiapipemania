#ifndef WGAME_H
#define WGAME_H

#include <QWidget>
#include <QElapsedTimer>
#include "partie.h"

class Bot;
class QTimer;

// Une explosion en cours d'affichage : la case de la bombe qui a saute, et le
// moment ou elle l'a fait sur l'HORLOGE D'AFFICHAGE. Pas le temps simule : la
// destruction est deja faite cote moteur, donc le flash ne decide de rien et
// n'a aucune raison d'etre comprime par l'acceleration (BOMBES.md, §5).
typedef struct _SFlash {
    int idx;
    qint64 debut;
} SFlash;

class WGame : public QWidget
{
    Q_OBJECT
public:
    explicit WGame(QWidget *parent = nullptr);
    void setPartie(Partie *partie);
    // Bot courant, ou nullptr quand c'est le joueur qui tient la souris. Sert
    // seulement a l'overlay du tas de defausse : la grille ne joue pas le bot.
    void setBot(Bot *bot);
    // Affiche ou non l'overlay du tas de defausse (case a cocher de la fenetre).
    void setAfficherTas(bool afficher);
    // Affiche ou non le plan de defausse : le reseau que le bot vise quand il
    // jette une piece, en trait fin par-dessus le plateau.
    void setAfficherPlan(bool afficher);
    // Releve les bombes qui viennent de sauter et arme leur flash. Appelee par
    // la fenetre apres avoir avance la partie : une explosion ne laisse aucune
    // trace sur le plateau, elle ne se lit qu'a l'instant ou elle a lieu.
    void releverExplosions();
    // Overlay du mode pas a pas : une marque par case (Bot::EMarqueRegion),
    // indexee row * largeur + col. Un vecteur vide eteint l'overlay -- c'est
    // ainsi que la fenetre le coupe hors du mode pas a pas.
    void setRegion(const QVector<unsigned char> &region);

protected:
    virtual void paintEvent(QPaintEvent *event);
    virtual void mouseReleaseEvent(QMouseEvent *event);
    virtual void mouseMoveEvent(QMouseEvent *event);
    virtual void leaveEvent(QEvent *event);

private:
    Partie *partie = nullptr;
    Bot *bot = nullptr;
    bool afficherTas = true;
    bool afficherPlan = false;
    QVector<unsigned char> region;
    QPoint caseSurvolee = QPoint(-1, -1);
    // Les flashs en cours, et l'horloge qui les date. Un QTimer a nous plutot
    // que les battements de la fenetre : celle-ci cesse de repeindre des que la
    // manche est finie, et c'est justement le cas de l'explosion qui tue.
    QVector<SFlash> flashs;
    QElapsedTimer horlogeEcran;
    QTimer *animation = nullptr;

    int spriteWidth() const;
    int spriteHeight() const;
    QPoint caseSous(const QPoint& position) const;
signals:
    // Emis a chaque relachement dans la grille, meme quand le coup est refuse :
    // c'est un geste depense, et le journal doit le compter.
    void pieceDeposee(int col, int row, bool accepte);
    // Emis a chaque clic droit dans la grille, accepte ou non. Le stock a pu
    // changer : c'est ce qui fait repeindre le panneau, qui ne se rafraichit
    // pas de lui-meme pendant l'ecoulement.
    void bombePosee(int col, int row, bool accepte);
};

#endif // WGAME_H
