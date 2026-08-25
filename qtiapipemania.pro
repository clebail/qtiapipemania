QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    game.cpp \
    main.cpp \
    mainwindow.cpp \
    piecefile.cpp \
    sprite.cpp \
    spritebombe.cpp \
    spritecoudebasdroite.cpp \
    spritecoudebasgauche.cpp \
    spritecoudehautdroite.cpp \
    spritecoudehautgauche.cpp \
    spritecroix.cpp \
    spritehorizontal.cpp \
    spritenone.cpp \
    spriteoriente.cpp \
    spritereservoir.cpp \
    spritevertical.cpp \
    wgame.cpp \
    wpiecefile.cpp

HEADERS += \
    common.h \
    game.h \
    mainwindow.h \
    piecefile.h \
    sprite.h \
    spritebombe.h \
    spritecoudebasdroite.h \
    spritecoudebasgauche.h \
    spritecoudehautdroite.h \
    spritecoudehautgauche.h \
    spritecroix.h \
    spritehorizontal.h \
    spritenone.h \
    spriteoriente.h \
    spritereservoir.h \
    spritevertical.h \
    wgame.h \
    wpiecefile.h

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

RESOURCES += \
    qtiapipemania.qrc
