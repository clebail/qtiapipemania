QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

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
