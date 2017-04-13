#!/bin/bash

set -e
function error_handler() {
    echo "Error during build!"
    exit 666
}

trap error_handler EXIT

./setup_build_env.sh

# linking directly to source for now
#./build_giflib.sh

source common.sh
subm

if [ ! -e netpbm/lib/libnetpbm.a ]; then
    pushd netpbm
    make
    popd
fi

if [ ! -e jsoncpp/build/lib/libjsoncpp.a ]; then
    pushd jsoncpp
    rm -rf build
    mkdir build
    pushd build
    cmake -DJSONCPP_LIB_BUILD_SHARED=OFF -DJSONCPP_WITH_TESTS=OFF -G "Unix Makefiles" ../
    make
    popd
    popd
fi

############
# Building Qt.
# NB: Compile and install from /mnt/ where there is
#     generally more room on AMIs.
######################

if [ ! -e /mnt/qt_install ]; then
    mkdir -p /mnt/qt_build
    pushd /mnt/qt_build
    # skiping all modules for now and just installing base Qt.
    ~/ichabod/qt/configure -v -opensource -confirm-license -release -static -no-strip -qt-xcb -qt-zlib -qt-libpng -qt-libjpeg -no-accessibility -no-xinput2 -no-pulseaudio -no-qml-debug -no-sql-ibase -no-sql-mysql -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2 -nomake examples -nomake tests -silent -xrender -largefile -openssl -rpath -no-dbus -no-nis -no-cups -no-iconv -no-pch -no-gtkstyle -no-sm -no-xshape -no-xinerama -no-xcursor -no-xfixes -no-xrandr -no-mitshm -no-xinput -no-xkb -no-glib -no-gstreamer -D ENABLE_VIDEO=0 -no-openvg -no-xsync -no-audio-backend -no-sse3 -no-ssse3 -no-sse4.1 -no-sse4.2 -no-avx -no-avx2 -no-mips_dsp -no-mips_dspr2 -skip qtandroidextras -skip qtmacextras -skip qtwinextras -skip qtactiveqt -skip qtlocation -skip qtimageformats -skip qtqa -skip qtx11extras -skip qtdeclarative -skip qtwayland -skip qtscript -skip qtquick1 -skip qtserialport -skip qttranslations -skip qttools -skip qtconnectivity -skip qtmultimedia -skip qtsensors -skip qtsystems -skip qtwebchannel -skip qtwebsockets -skip qtdoc -skip qtwebkit-examples -skip qtpim -skip qtxmlpatterns -skip qtsvg -skip qtwebkit --prefix=../../qt_install || die "cannot configure qt"
    if ! gmake -j8 -q; then
        gmake -j8 || die "cannot make qt"
    fi
    gmake install || die "cannot install qt"
    popd
    #rm -rf qt_build
fi

###########
# Build all required modules.
######################

# Build and install qtwebkit module
if [ ! -e /mnt/qtwebkit_build ]; then
    mkdir /mnt/qtwebkit_build
    pushd /mnt/qtwebkit_build
    /mnt/qt_install/bin/qmake ~/ichabod/qt/qtwebkit/WebKit.pro
    make && make install
    popd
fi

# Build and install qtsvg module
if [ ! -e /mnt/qtsvg_build ]; then
    mkdir /mnt/qtsvg_build
    pushd /mnt/qtwebkit_build
    /mnt/qt_install/bin/qmake ~/ichabod/qt/qtsvg/qtsvg.pro
    make && make install
    popd
fi

# Build and install qtxmlpatterns module
if [ ! -e /mnt/qtxmlpatterns ]; then
    mkdir /mnt/qtxmlpatterns_build
    pushd /mnt/qtxmlpatterns_build
    /mnt/qt_install/bin/qmake ~/ichabod/qt/qtxmlpatterns/qtxmlpatterns.pro
    make && make install
    popd
fi

############
# Build and install Ichabod.
######################

/mnt/qt_install/bin/qmake ichabod.pro
make && make install


# test
./test.sh

./archive_src.sh

./build_rpm.sh

# all done, reset everything
trap - EXIT
exit 0
