QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# Compilation dans le repertoire source interdite. qmake place le repertoire du
# .pro AVANT celui de build dans les -I : un ui_*.h ou moc_* laisse a la racine
# masque donc celui que le shadow build vient de generer, et Qt Creator compile
# du code mort sans rien signaler.
equals(PWD, $${OUT_PWD}): error("Compilez dans un repertoire separe : mkdir -p build/cli && cd build/cli && qmake6 ../../qtiapipemania.pro && make")

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    dessinpiece.cpp \
    ecoulement.cpp \
    game.cpp \
    main.cpp \
    mainwindow.cpp \
    partie.cpp \
    piecefile.cpp \
    wdepart.cpp \
    wgame.cpp \
    wpanneau.cpp

HEADERS += \
    common.h \
    dessinpiece.h \
    ecoulement.h \
    game.h \
    mainwindow.h \
    partie.h \
    piecefile.h \
    wdepart.h \
    wgame.h \
    wpanneau.h

FORMS += \
    mainwindow.ui

TRANSLATIONS += \
    qtiapipemania_fr_FR.ts
CONFIG += lrelease
CONFIG += embed_translations

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
