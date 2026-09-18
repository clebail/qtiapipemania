#ifndef SERVEURCONTROLE_H
#define SERVEURCONTROLE_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

class Controle;

// Le transport, et rien d'autre : une socket, une ligne JSON par message.
//
// Il ne connait du jeu que Controle::invoke() et Controle::etat(). Changer de
// protocole plus tard, c'est reecrire ce fichier -- pas le coeur.
//
// Le choix de QTcpServer plutot que des sockets POSIX n'est pas de la
// commodite. Il supprime deux pieges d'un coup : Qt neutralise SIGPIPE (ecrire
// sur une socket dont le pair est parti tuerait le processus par defaut), et
// ses ecritures sont asynchrones, donc un client qui cesse de lire ne peut pas
// bloquer la boucle de jeu. Restait a borner la memoire : voir OCTETS_MAX.
class ServeurControle : public QObject {
    Q_OBJECT

public:
    explicit ServeurControle(Controle *controle, QObject *parent = nullptr);
    ~ServeurControle() override;

    // Ecoute sur la boucle locale, JAMAIS sur 0.0.0.0 : l'acces distant passe
    // par un tunnel SSH, qui apporte le chiffrement et l'authentification sans
    // qu'on ecrive une ligne de crypto.
    bool demarrer(quint16 port = 9000);
    void arreter();

    bool ecoute() const;
    bool clientConnecte() const;
    quint16 port() const;

signals:
    // Pour que la fenetre puisse dire ou en est le serveur sans l'interroger.
    void etatChange();

private slots:
    void nouvelleConnexion();
    void lire();
    void deconnexion();
    void publierEtat();

private:
    void envoyer(const QJsonObject &objet);
    void traiter(const QByteArray &ligne);

    QTcpServer serveur;
    QTcpSocket *client = nullptr;
    QTimer horlogeEtat;
    Controle *controle;
    qint64 sequence = 0;
};

#endif // SERVEURCONTROLE_H
