DEFINES += QT_STATIC

MOC_DIR      = build
OBJECTS_DIR  = build
UI_DIR       = build

QT += webkit network xmlpatterns svg webkitwidgets
QT -= gui

TEMPLATE = app
TARGET = ichabod
INCLUDEPATH += . mongoose 
CONFIG += debug

# mongoose
SOURCES += mongoose/mongoose.c

# giflib
INCLUDEPATH += usr/include
LIBS += -L/usr/local/lib -lgif
SOURCES += code/lib/dgif_lib.c \
       code/lib/egif_lib.c \
       code/lib/gifalloc.c \
       code/lib/gif_err.c \
       code/lib/gif_font.c \
       code/lib/gif_hash.c \
       code/lib/quantize.c

# jsoncpp
INCLUDEPATH += jsoncpp/include
LIBS += -Ljsoncpp/build/lib -ljsoncpp

# statsd
INCLUDEPATH += statsd-client-cpp/src
SOURCES += statsd-client-cpp/src/statsd_client.cpp

# netpbm
INCLUDEPATH += netpbm netpbm/lib
LIBS += -Lnetpbm/lib -lnetpbm

# ichabod
HEADERS += conv.h engine.h
SOURCES += agif.cpp conv.cpp main.cpp mediancut.cpp engine.cpp


