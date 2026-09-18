#ifndef CONTROLE_H
#define CONTROLE_H

#include <QJsonObject>
#include <QString>

class Partie;

// Ce que seule la fenetre sait faire : arreter le temps, l'avancer d'un pas,
// remplacer la partie. Le controle ne depend que de cette interface -- ni de la
// fenetre, ni du reseau. C'est ce qui permet de le tester sans monter de
// serveur, et de changer de transport sans y toucher.
class HoteControle {
public:
    virtual ~HoteControle() = default;
    virtual void mettreEnPause(bool pause) = 0;
    virtual bool estEnPause() const = 0;
    // Avance de n battements de jeu, exactement comme le ferait l'horloge.
    virtual void avancerDeBattements(int n) = 0;
    // La barre d'espace : lance le flux en avance, enchaine la fin de manche,
    // et fonce.
    virtual void espace() = 0;
    // La partie vient d'etre remplacee : la fenetre doit se remettre d'aplomb
    // (bot, plan de defausse, affichage).
    virtual void partieRemplacee() = 0;
    // Le temps de JEU ecoule, en secondes -- celui qui avance de 16 ms par
    // battement, pas celui de la montre. C'est lui qui tarife les gestes.
    virtual float tempsSimule() const = 0;
    // Vrai quand l'horloge n'appartient plus au client : le jeu tourne en temps
    // reel et `pause` comme `pas` sont refuses. C'est le cas des qu'un script
    // pilote -- arreter le temps lui rendrait sa reflexion gratuite, ce qu'un
    // joueur n'a pas.
    virtual bool horlogeVerrouillee() const = 0;
};

// L'API de pilotage, sans le moindre octet de reseau.
//
// Une seule porte d'entree -- invoke(nom, arguments) -> resultat -- et une
// lecture d'etat. Le protocole, le cadrage des messages et le transport sont
// au-dessus ; ici on ne parle que du jeu.
class Controle {
public:
    Controle(Partie *partie, HoteControle *hote);

    // Execute une commande. Rend toujours un objet : {"ok":true, ...} ou
    // {"ok":false, "erreur":"...", "motif":"..."} -- un refus est une reponse,
    // pas une exception. Le client DOIT pouvoir distinguer "pose refusee parce
    // que la case est prise" de "commande inconnue".
    QJsonObject invoke(const QString &nom, const QJsonObject &args);

    // Tout ce qu'il faut pour decider du coup suivant.
    QJsonObject etat() const;

    // Les commandes DISPONIBLES ici et maintenant : `pause` et `pas` disparaissent
    // quand l'horloge est verrouillee. Une aide qui listerait des commandes
    // refusees serait pire que pas d'aide.
    QStringList commandes() const;

private:
    QJsonObject poser(const QJsonObject &args);
    QJsonObject bomber(const QJsonObject &args);
    QJsonObject reinitialiser(const QJsonObject &args);
    QJsonObject mettreEnPause(const QJsonObject &args);
    QJsonObject avancer(const QJsonObject &args);
    QJsonObject horlogeRefusee() const;

    // Lit un entier d'un objet JSON en distinguant "absent" de "present mais
    // pas un nombre" -- sans quoi un {"x":"3"} passerait pour un x a zero et la
    // piece partirait dans le coin du plateau.
    static bool lireEntier(const QJsonObject &args, const QString &cle, int &valeur,
                           QString &erreur);

    // Temps de jeu de la derniere pose acceptee. Voir CADENCE_POSE dans le .cpp
    // pour ce que ca tarife et pourquoi.
    float tempsDernierePose;

    Partie *p;
    HoteControle *hote;
};

#endif // CONTROLE_H
