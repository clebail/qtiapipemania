#ifndef BOTFACTORY_H
#define BOTFACTORY_H

#include <QString>
#include <QStringList>

#include "bot.h"

// Seul point du programme qui connaisse la liste des strategies : le banc et la
// fenetre de jeu ne manipulent qu'un Bot*, et changer de bot ne coute qu'un nom
// sur la ligne de commande.
class BotFactory
{
public:
    // Renvoie nullptr si le nom est inconnu -- a l'appelant de le signaler.
    // L'instance est a la charge de l'appelant.
    static Bot * createInstance(const QString &nom, Partie *p, float cadence, quint32 seed);
    // Noms acceptes, dans l'ordre d'anciennete. Sert a l'aide en ligne de
    // commande et a la validation, pour qu'elles ne puissent pas diverger.
    static QStringList noms();
};

#endif // BOTFACTORY_H
