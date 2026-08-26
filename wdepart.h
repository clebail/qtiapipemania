#ifndef WDEPART_H
#define WDEPART_H

#include <QWidget>

// Barregraphe vertical du temps restant avant le depart du flux. La jauge est
// pleine en debut de manche et se vide par le bas.
class WDepart : public QWidget
{
    Q_OBJECT
public:
    explicit WDepart(QWidget *parent = nullptr);

    // fraction : 1 = delai entier restant, 0 = le flux part maintenant.
    void setFraction(float fraction);

protected:
    virtual void paintEvent(QPaintEvent *event);

private:
    float fraction = 0.0f;
};

#endif // WDEPART_H
