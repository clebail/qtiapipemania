#ifndef WGAME_H
#define WGAME_H

#include <QWidget>
#include "partie.h"

class Bot;

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

    int spriteWidth() const;
    int spriteHeight() const;
    QPoint caseSous(const QPoint& position) const;
signals:
    // Emis a chaque relachement dans la grille, meme quand le coup est refuse :
    // c'est un geste depense, et le journal doit le compter.
    void pieceDeposee(int col, int row, bool accepte);
};

#endif // WGAME_H
