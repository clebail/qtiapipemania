#include "controle.h"

#include <QJsonArray>
#include <QtMath>

#include "partie.h"

// Le plateau se lit a un caractere par case, et il n'y a PAS de sens a
// transmettre : Ecoulement::ouvertures() l'ignore pour tout sauf le reservoir
// -- un coude porte son orientation dans son type. Le sens du reservoir part
// donc a part, dans "depart".
static char lettreDe(ETypePiece type) {
    switch(type) {
    case tpNone:            return '.';
    case tpHorizontal:      return '-';
    case tpVertical:        return '|';
    case tpCoudeHautGauche: return 'J';
    case tpCoudeHautDroite: return 'L';
    case tpCoudeBasGauche:  return '7';
    case tpCoudeBasDroite:  return 'F';
    case tpCroix:           return '+';
    case tpReservoir:       return 'R';
    case tpBombe:           return 'B';
    case tpBloque:          return '#';
    }

    return '?';
}

static QString nomDe(ETypePiece type) {
    switch(type) {
    case tpNone:            return "vide";
    case tpHorizontal:      return "horizontal";
    case tpVertical:        return "vertical";
    case tpCoudeHautGauche: return "coudeHG";
    case tpCoudeHautDroite: return "coudeHD";
    case tpCoudeBasGauche:  return "coudeBG";
    case tpCoudeBasDroite:  return "coudeBD";
    case tpCroix:           return "croix";
    case tpReservoir:       return "reservoir";
    case tpBombe:           return "bombe";
    case tpBloque:          return "bloc";
    }

    return "inconnu";
}

static QString nomDe(ESens sens) {
    switch(sens) {
    case sHaut:   return "haut";
    case sBas:    return "bas";
    case sGauche: return "gauche";
    case sDroite: return "droite";
    }

    return "inconnu";
}

static QString nomDe(EEtatPartie etat) {
    switch(etat) {
    case epAttente:    return "attente";
    case epEcoulement: return "ecoulement";
    case epReussie:    return "reussie";
    case epPerdue:     return "perdue";
    case epGameOver:   return "gameover";
    case epAbandon:    return "abandon";
    }

    return "inconnu";
}

// CE QUE COUTE UN GESTE, et c'est le coeur de l'honnetete de cette API.
//
// Le moteur n'a jamais limite la cadence de pose : la main du joueur s'en
// chargeait, et le bot se rationne lui-meme par ses jetons. Exposer
// Partie::poserPiece tel quel rendait donc `pose` INSTANTANE -- et comme seul
// `pas` fait avancer l'horloge, un client en pas a pas pouvait bruler toute la
// file et batir son trace entier avant le premier battement.
//
// Trois raisons qui rendaient l'exploit total, et pas seulement genant :
//
//   - poser sur une case VIDE ne coute rien du tout ;
//   - poser sur une case occupee coute 25 points, mais le compteur est plancher
//     a zero (convention arcade) -- et une partie demarre a zero, donc c'est
//     gratuit ;
//   - la file etant un flux infini, un defaussage gratuit permet d'attendre
//     n'importe quel type. Or "4 types sur 7 conviennent pour une entree
//     donnee" est LE facteur de difficulte du jeu.
//
// Le jeu cessait d'etre une course contre la montre pour devenir un probleme de
// plus long chemin dans une grille 15x15 -- interessant, mais ce n'est plus le
// meme jeu, et plus rien n'etait comparable au bot.
//
// On tarife donc la pose au temps de JEU, a la cadence du bot : deux gestes par
// seconde, soit une pose tous les 31 battements. Meme monnaie pour les deux,
// donc les memes graines donnent des resultats comparables.
#define CADENCE_POSE            2.0f
#define INTERVALLE_POSE         (1.0f / CADENCE_POSE)

Controle::Controle(Partie *partie, HoteControle *hote)
    : tempsDernierePose(-1000.0f), p(partie), hote(hote) {
}

QStringList Controle::commandes() const {
    QStringList noms;
    noms << "etat" << "pose" << "bombe" << "espace" << "reset" << "aide";

    if(!hote->horlogeVerrouillee()) {
        noms << "pause" << "pas";
    }

    return noms;
}

bool Controle::lireEntier(const QJsonObject &args, const QString &cle, int &valeur,
                          QString &erreur) {
    QJsonValue v = args.value(cle);

    if(v.isUndefined()) {
        erreur = QString("argument manquant : %1").arg(cle);
        return false;
    }

    // isDouble et non toInt() seul : un "3" entre guillemets rendrait zero en
    // silence, et la piece partirait dans le coin du plateau.
    if(!v.isDouble()) {
        erreur = QString("argument %1 : un entier est attendu").arg(cle);
        return false;
    }

    valeur = v.toInt();
    return true;
}

QJsonObject Controle::invoke(const QString &nom, const QJsonObject &args) {
    if(nom == "etat") {
        QJsonObject r;
        r["ok"] = true;
        r["etat"] = etat();
        return r;
    }

    if(nom == "pose")   return poser(args);
    if(nom == "bombe")  return bomber(args);
    if(nom == "reset")  return reinitialiser(args);
    if(nom == "pause")  return mettreEnPause(args);
    if(nom == "pas")    return avancer(args);

    if(nom == "espace") {
        hote->espace();

        QJsonObject r;
        r["ok"] = true;
        r["etat"] = etat();
        return r;
    }

    if(nom == "aide") {
        QJsonObject r;
        r["ok"] = true;
        r["commandes"] = QJsonArray::fromStringList(commandes());
        return r;
    }

    QJsonObject r;
    r["ok"] = false;
    r["erreur"] = QString("commande inconnue : %1").arg(nom);
    r["commandes"] = QJsonArray::fromStringList(commandes());
    return r;
}

// Poser, et surtout DIRE POURQUOI ca n'a pas marche. Un refus muet obligerait
// le script a deviner, et il devinerait mal : les trois causes n'appellent pas
// la meme reaction -- attendre, viser ailleurs, ou constater que la manche est
// finie.
QJsonObject Controle::poser(const QJsonObject &args) {
    QJsonObject r;
    int x = 0, y = 0;
    QString erreur;

    if(!lireEntier(args, "x", x, erreur) || !lireEntier(args, "y", y, erreur)) {
        r["ok"] = false;
        r["erreur"] = erreur;
        return r;
    }

    if(x < 0 || x >= p->getLargeur() || y < 0 || y >= p->getHauteur()) {
        r["ok"] = false;
        r["erreur"] = "hors du plateau";
        return r;
    }

    // Le peage AVANT la legalite : "tu ne peux pas encore jouer" n'est pas la
    // meme information que "cette case est interdite", et les confondre ferait
    // chercher au client une erreur de visee qui n'existe pas.
    //
    // Une pose REFUSEE ne consomme rien : elle n'a rien pose. Sonder le plateau
    // reste gratuit, et c'est sans consequence -- l'etat dit deja tout ce qu'un
    // sondage apprendrait.
    float attente = INTERVALLE_POSE - (hote->tempsSimule() - tempsDernierePose);

    if(attente > 0.0f) {
        r["ok"] = false;
        r["erreur"] = "pose refusee";
        r["motif"] = "trop tot";
        r["attendre_secondes"] = attente;
        r["attendre_battements"] = (int)qCeil(attente / 0.016f);
        r["etat"] = etat();
        return r;
    }

    // Le type part dans la reponse : la file a deja descendu d'un cran quand le
    // client relira l'etat, et il doit pouvoir savoir ce qu'il vient de poser.
    ETypePiece pose = p->file()->getPiece(0).type;
    bool accepte = p->poserPiece(x, y);

    if(accepte) {
        tempsDernierePose = hote->tempsSimule();
    }

    r["ok"] = accepte;

    // `type_piece` et non `type` : le transport pose un `type` sur chaque
    // message ("reponse" ou "etat") et l'ecraserait. Deux sens pour un nom,
    // c'est le genre de collision qu'on met une heure a voir.
    r["type_piece"] = nomDe(pose);

    if(!accepte) {
        EEtatPartie e = p->etat();
        ETypePiece actuelle = p->plateau()->getTypePiece(x, y);

        r["erreur"] = "pose refusee";
        r["motif"] = (e != epAttente && e != epEcoulement) ? "manche finie"
                   : p->ecoulement()->estRempli(x, y)      ? "case deja traversee par le flux"
                   : actuelle == tpBloque                  ? "bloc"
                   : actuelle == tpReservoir               ? "reservoir"
                   : actuelle == tpBombe                   ? "bombe armee"
                                                           : "case interdite";
    }

    r["etat"] = etat();
    return r;
}

QJsonObject Controle::bomber(const QJsonObject &args) {
    QJsonObject r;
    int x = 0, y = 0;
    QString erreur;

    if(!lireEntier(args, "x", x, erreur) || !lireEntier(args, "y", y, erreur)) {
        r["ok"] = false;
        r["erreur"] = erreur;
        return r;
    }

    bool accepte = p->poserBombe(x, y);
    r["ok"] = accepte;

    if(!accepte) {
        r["erreur"] = "bombe refusee";
        r["motif"] = p->bombes() <= 0 ? "stock vide"
                                      : "la case doit etre totalement vide";
    }

    r["etat"] = etat();
    return r;
}

// Partie neuve, reproductible si on donne la graine.
//
// L'ORDRE compte et n'est pas negociable : la graine refait la partie depuis le
// premier niveau, le niveau refait la manche, et les compteurs se posent
// ensuite -- les regler avant les verrait ecrases.
QJsonObject Controle::reinitialiser(const QJsonObject &args) {
    QJsonObject r;

    if(args.contains("graine")) {
        QJsonValue v = args.value("graine");

        if(!v.isDouble()) {
            r["ok"] = false;
            r["erreur"] = "argument graine : un entier est attendu";
            return r;
        }

        p->nouvellePartie((quint32)v.toDouble());
    } else {
        p->nouvellePartie();
    }

    if(args.contains("niveau")) {
        p->setNiveauDepart(args.value("niveau").toInt(1));
    }

    if(args.contains("vies")) {
        p->setViesDepart(args.value("vies").toInt(3));
    }

    if(args.contains("bombes")) {
        p->setBombesDepart(args.value("bombes").toInt(0));
    }

    hote->partieRemplacee();

    // Partie neuve, credit neuf : on ne fait pas payer au niveau 1 le geste
    // joue dans la partie precedente.
    tempsDernierePose = -1000.0f;

    r["ok"] = true;
    r["etat"] = etat();
    return r;
}

// Arreter le temps rendrait la reflexion gratuite -- une heure de recherche
// arborescente entre deux battements --, ce qu'aucun joueur n'a. Tant qu'un
// script pilote, l'horloge ne lui appartient pas.
//
// Le pas a pas reste ce pour quoi il a ete fait : un outil de mise au point,
// dans la fenetre, a la main.
QJsonObject Controle::horlogeRefusee() const {
    QJsonObject r;
    r["ok"] = false;
    r["erreur"] = "le temps n'est pas pilotable : le jeu tourne en temps reel";
    r["motif"] = "horloge verrouillee";
    return r;
}

QJsonObject Controle::mettreEnPause(const QJsonObject &args) {
    if(hote->horlogeVerrouillee()) {
        return horlogeRefusee();
    }

    QJsonObject r;
    QJsonValue v = args.value("pause");

    if(!v.isBool()) {
        r["ok"] = false;
        r["erreur"] = "argument pause : true ou false";
        return r;
    }

    hote->mettreEnPause(v.toBool());

    r["ok"] = true;
    r["en_pause"] = hote->estEnPause();
    r["etat"] = etat();
    return r;
}

// Avancer d'un nombre exact de battements. C'est ce qui rend une partie pilotee
// REJOUABLE : le jeu n'avance que quand le script le demande, donc la meme
// graine et la meme suite de commandes redonnent la meme partie, a la case
// pres. En temps reel, la latence du reseau suffit a tout changer.
QJsonObject Controle::avancer(const QJsonObject &args) {
    if(hote->horlogeVerrouillee()) {
        return horlogeRefusee();
    }

    QJsonObject r;
    int n = args.value("n").toInt(1);

    if(n < 1 || n > 100000) {
        r["ok"] = false;
        r["erreur"] = "argument n : entre 1 et 100000";
        return r;
    }

    if(!hote->estEnPause()) {
        r["ok"] = false;
        r["erreur"] = "le jeu n'est pas en pause : `pas` ne veut rien dire "
                      "tant que l'horloge tourne";
        return r;
    }

    hote->avancerDeBattements(n);

    r["ok"] = true;
    r["etat"] = etat();
    return r;
}

QJsonObject Controle::etat() const {
    Game *plateau = p->plateau();
    Ecoulement *ecoul = p->ecoulement();
    int largeur = p->getLargeur();
    int hauteur = p->getHauteur();

    QJsonObject partie;
    partie["graine"] = (double)p->getGraine();
    partie["manche"] = p->numeroManche();
    partie["niveau"] = p->niveau();
    partie["etat"] = nomDe(p->etat());

    QJsonObject compteurs;
    compteurs["score"] = p->score();
    compteurs["vies"] = p->vies();
    compteurs["bombes"] = p->bombes();
    compteurs["objectif"] = p->longueurMinimale();
    compteurs["tracee"] = p->longueurTracee();
    compteurs["traversees"] = p->casesTraversees();
    compteurs["remplacements"] = p->nbRemplacements();

    QJsonObject depart;
    depart["x"] = p->getXDepart();
    depart["y"] = p->getYDepart();
    depart["sens"] = nomDe(plateau->getSens(p->getXDepart(), p->getYDepart()));
    depart["fraction"] = p->fractionAvantDepart();
    depart["secondes"] = p->secondesAvantDepart();

    // La file dans l'ordre de jeu : [0] est la piece que `pose` deposera.
    QJsonArray file;

    for(int i = 0; i < p->file()->getTaille(); i++) {
        file.append(nomDe(p->file()->getPiece(i).type));
    }

    // Deux calques. Le second dit ce que le flux a DEJA pris, par axe : une
    // croix se traverse une fois par axe, et les confondre rendrait tout
    // croisement injouable pour le client.
    QJsonArray lignes;
    QJsonArray remplies;

    for(int y = 0; y < hauteur; y++) {
        QString ligne;
        QString prise;

        for(int x = 0; x < largeur; x++) {
            ligne += QChar(lettreDe(plateau->getTypePiece(x, y)));

            bool h = ecoul->progression(x, y, AXE_HORIZONTAL) > 0.0f;
            bool v = ecoul->progression(x, y, AXE_VERTICAL) > 0.0f;
            prise += QChar(h && v ? 'X' : h ? 'h' : v ? 'v' : '.');
        }

        lignes.append(ligne);
        remplies.append(prise);
    }

    QJsonObject etat;
    etat["partie"] = partie;
    etat["compteurs"] = compteurs;
    etat["depart"] = depart;
    etat["file"] = file;
    etat["plateau"] = lignes;
    etat["remplies"] = remplies;
    etat["en_pause"] = hote->estEnPause();

    // La tete : la seule case ou construire, et le cote par lequel le flux y
    // entrera. C'est Ecoulement::tete et non celle du bot -- la premiere est un
    // constat, la seconde modifie l'etat du bot au passage.
    int tc, tr;
    ESens te;

    if(ecoul->tete(tc, tr, te)) {
        QJsonObject tete;
        tete["x"] = tc;
        tete["y"] = tr;
        tete["entree"] = nomDe(te);
        etat["tete"] = tete;
    } else {
        etat["tete"] = QJsonValue::Null;
    }

    // Combien de tuyau le flux doit encore parcourir avant d'arriver a la
    // tete : c'est ce qui dit combien de gestes il reste, et rien d'autre ne le
    // dit.
    etat["aval"] = ecoul->casesEnAval();

    // Dans combien de temps de jeu la prochaine pose sera acceptee. Zero quand
    // c'est maintenant. Sans ca le client devrait tenir ce compte lui-meme, et
    // il le tiendrait de travers le jour ou une pose est refusee.
    etat["prochaine_pose"] = qMax(0.0f, INTERVALLE_POSE
                                        - (hote->tempsSimule() - tempsDernierePose));

    QJsonArray bombes;

    for(int y = 0; y < hauteur; y++) {
        for(int x = 0; x < largeur; x++) {
            float reste = p->minage()->fractionRestante(x, y);

            if(reste >= 0.0f) {
                QJsonObject b;
                b["x"] = x;
                b["y"] = y;
                b["reste"] = reste;
                bombes.append(b);
            }
        }
    }

    etat["bombes_armees"] = bombes;

    return etat;
}
