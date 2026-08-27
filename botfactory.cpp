#include "botfactory.h"
#include "botglouton.h"
#include "botspace.h"
#include "botspaceanticp.h"
#include "botmemoire.h"

// Ajouter une strategie : une ligne ici, une ligne dans noms(), et rien
// ailleurs.
Bot * BotFactory::createInstance(const QString &nom, Partie *p, float cadence, quint32 seed) {
    if(nom == "glouton") {
        return new BotGlouton(p, cadence, seed);
    }

    if(nom == "space") {
        return new BotSpace(p, cadence, seed);
    }

    if(nom == "spaceAnticp") {
        return new BotSpaceAnticp(p, cadence, seed);
    }

    if(nom == "memoire") {
        return new BotMemoire(p, cadence, seed);
    }

    return nullptr;
}

QStringList BotFactory::noms() {
    return QStringList() << "glouton" << "space" << "spaceAnticp" << "memoire";
}
