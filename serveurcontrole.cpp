#include "serveurcontrole.h"

#include <QDateTime>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>

#include "controle.h"

// Cadence de publication de l'etat. Dix par seconde : le jeu bat a 62,5 Hz mais
// un joueur -- bot compris -- ne pose que deux pieces par seconde, donc dix
// photos suffisent tres largement a decider.
#define PERIODE_ETAT_MS         100
// Au-dela, on saute la publication. Un client qui cesse de lire (un point
// d'arret dans le debogueur suffit) verrait sinon les etats s'empiler dans le
// tampon d'envoi jusqu'a manger la memoire -- et surtout il piloterait ensuite
// sur des photos perimees sans que rien ne le signale. Sauter est le bon
// reflexe : l'etat suivant remplace celui qu'on n'a pas envoye.
#define OCTETS_MAX              (256 * 1024)
// Une ligne de commande plus longue que ca n'est pas une commande.
#define LIGNE_MAX               (64 * 1024)

ServeurControle::ServeurControle(Controle *controle, QObject *parent)
    : QObject(parent), controle(controle) {
    connect(&serveur, &QTcpServer::newConnection, this, &ServeurControle::nouvelleConnexion);

    horlogeEtat.setInterval(PERIODE_ETAT_MS);
    connect(&horlogeEtat, &QTimer::timeout, this, &ServeurControle::publierEtat);
}

ServeurControle::~ServeurControle() {
    arreter();
}

bool ServeurControle::demarrer(quint16 portVoulu) {
    if(serveur.isListening()) {
        return true;
    }

    if(!serveur.listen(QHostAddress::LocalHost, portVoulu)) {
        qWarning("serveur : impossible d'ecouter sur 127.0.0.1:%u (%s)",
                 portVoulu, qUtf8Printable(serveur.errorString()));
        return false;
    }

    qInfo("serveur : en ecoute sur 127.0.0.1:%u -- en attente d'un client",
          serveur.serverPort());
    emit etatChange();
    return true;
}

void ServeurControle::arreter() {
    horlogeEtat.stop();

    if(client != nullptr) {
        client->disconnectFromHost();
        client->deleteLater();
        client = nullptr;
    }

    if(serveur.isListening()) {
        serveur.close();
        qInfo("serveur : arrete");
    }

    emit etatChange();
}

bool ServeurControle::ecoute() const {
    return serveur.isListening();
}

bool ServeurControle::clientConnecte() const {
    return client != nullptr && client->state() == QAbstractSocket::ConnectedState;
}

quint16 ServeurControle::port() const {
    return serveur.serverPort();
}

// Un seul client, et on le dit au second plutot que de le laisser attendre une
// reponse qui ne viendra pas.
void ServeurControle::nouvelleConnexion() {
    while(serveur.hasPendingConnections()) {
        QTcpSocket *entrant = serveur.nextPendingConnection();

        if(client != nullptr) {
            entrant->write("{\"type\":\"reponse\",\"ok\":false,"
                           "\"erreur\":\"un client est deja connecte\"}\n");
            entrant->flush();
            entrant->disconnectFromHost();
            entrant->deleteLater();
            continue;
        }

        client = entrant;

        // Sans lui, l'algorithme de Nagle attend de quoi remplir un paquet et
        // ajoute jusqu'a 40 ms sur de petits messages -- exactement ce trafic.
        // Sur une boucle de 100 ms, c'est 40 % de perdu.
        client->setSocketOption(QAbstractSocket::LowDelayOption, 1);

        connect(client, &QTcpSocket::readyRead, this, &ServeurControle::lire);
        connect(client, &QTcpSocket::disconnected, this, &ServeurControle::deconnexion);

        qInfo("serveur : client connecte");
        horlogeEtat.start();
        publierEtat();
        emit etatChange();
    }
}

void ServeurControle::deconnexion() {
    horlogeEtat.stop();

    if(client != nullptr) {
        client->deleteLater();
        client = nullptr;
    }

    qInfo("serveur : client parti -- en attente d'un autre");
    emit etatChange();
}

void ServeurControle::lire() {
    if(client == nullptr) {
        return;
    }

    while(client->canReadLine()) {
        QByteArray ligne = client->readLine(LIGNE_MAX);

        if(!ligne.trimmed().isEmpty()) {
            traiter(ligne);
        }
    }
}

// Une commande, une reponse. Le client joue : il doit savoir si sa pose est
// passee, et il ne peut pas le deduire du flux d'etat -- a 100 ms d'intervalle
// il reposerait deux fois avant d'avoir vu le resultat de la premiere.
//
// L'`id` de la requete est repris tel quel dans la reponse : c'est le minimum
// pour apparier les deux quand plusieurs commandes se croisent.
void ServeurControle::traiter(const QByteArray &ligne) {
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(ligne, &err);

    if(err.error != QJsonParseError::NoError || !doc.isObject()) {
        QJsonObject r;
        r["ok"] = false;
        r["type"] = "reponse";
        r["erreur"] = QString("JSON invalide : %1").arg(err.errorString());
        envoyer(r);
        return;
    }

    QJsonObject requete = doc.object();
    QString nom = requete.value("cmd").toString();

    if(nom.isEmpty()) {
        QJsonObject r;
        r["ok"] = false;
        r["type"] = "reponse";
        r["erreur"] = "champ \"cmd\" manquant";
        envoyer(r);
        return;
    }

    QJsonObject reponse = controle->invoke(nom, requete.value("args").toObject());

    // Le type est porte par CHAQUE message. Sans lui, le client devrait deduire
    // la nature d'un message de la presence d'un champ -- une regle qui ne se
    // documente pas et qu'on finit par se rappeler de travers.
    reponse["type"] = "reponse";
    reponse["cmd"] = nom;

    if(requete.contains("id")) {
        reponse["id"] = requete.value("id");
    }

    envoyer(reponse);
}

void ServeurControle::publierEtat() {
    if(!clientConnecte()) {
        return;
    }

    // Le tampon deborde : le client ne lit plus. On saute -- voir OCTETS_MAX.
    if(client->bytesToWrite() > OCTETS_MAX) {
        return;
    }

    QJsonObject message;
    message["type"] = "etat";
    message["etat"] = controle->etat();
    envoyer(message);
}

void ServeurControle::envoyer(const QJsonObject &objet) {
    if(!clientConnecte()) {
        return;
    }

    QJsonObject message = objet;

    // Un horodatage et un numero dans CHAQUE message. Le jour ou le script se
    // comporte bizarrement, on sait en trois secondes si le probleme vient de
    // lui ou de la fraicheur de ses donnees.
    message["t"] = (double)QDateTime::currentMSecsSinceEpoch() / 1000.0;
    message["seq"] = (double)(++sequence);

    client->write(QJsonDocument(message).toJson(QJsonDocument::Compact));
    client->write("\n");
}
