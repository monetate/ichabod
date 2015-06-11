DEFINES += QT_STATIC

MOC_DIR      = build
OBJECTS_DIR  = build
UI_DIR       = build

QT += webkit network xmlpatterns svg

TEMPLATE = app
TARGET = ichabod
INCLUDEPATH += . mongoose giflib/lib
CONFIG += debug

# mongoose
SOURCES += mongoose/mongoose.c

# giflib
SOURCES += giflib/lib/dgif_lib.c \
           giflib/lib/egif_lib.c \
           giflib/lib/gifalloc.c \
           giflib/lib/gif_err.c \
           giflib/lib/gif_font.c \
           giflib/lib/gif_hash.c \
           giflib/lib/quantize.c

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


