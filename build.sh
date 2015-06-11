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
    # go back to an older version of cmake
    git revert --no-edit ae5a56f9ff6932feb641bafe323195b6aa30bd21
    rm -rf build
    mkdir build
    pushd build
    cmake -DJSONCPP_LIB_BUILD_SHARED=OFF -DJSONCPP_WITH_TESTS=OFF -G "Unix Makefiles" ../
    make
    popd
    popd
fi

if [ ! -e qt_install ]; then
    mkdir -p qt_build
    pushd qt_build
    ../qt/configure -opensource -confirm-license -fast -release -static -graphicssystem raster -webkit -exceptions -xmlpatterns -qt-zlib -qt-libpng -qt-libjpeg -no-libmng -no-libtiff -no-accessibility -no-stl -no-qt3support -no-phonon -no-phonon-backend -no-opengl -no-declarative -no-scripttools -no-sql-ibase -no-sql-mysql -no-sql-odbc -no-sql-psql -no-sql-sqlite -no-sql-sqlite2 -no-mmx -no-3dnow -no-sse -no-sse2 -no-multimedia -nomake demos -nomake docs -nomake examples -nomake tools -nomake tests -nomake translations -silent -script -xrender -largefile -rpath -openssl -no-dbus -no-nis -no-cups -no-iconv -no-pch -no-gtkstyle -no-nas-sound -no-sm -no-xshape -no-xinerama -no-xcursor -no-xfixes -no-xrandr -no-mitshm -no-xinput -no-xkb -no-glib -no-gstreamer -D ENABLE_VIDEO=0 -no-openvg -no-xsync -no-audio-backend -no-sse3 -no-ssse3 -no-sse4.1 -no-sse4.2 -no-avx -no-neon --prefix=../qt_install || die "cannot configure qt"
    if ! make -j8 -q; then
        make -j8 || die "cannot make qt"
    fi
    make install || die "cannot install qt"
    popd
    #rm -rf qt_build
fi

./qt_install/bin/qmake ichabod.pro
make

./test.sh 

./archive_src.sh

./build_rpm.sh

# all done, reset everything
trap - EXIT
exit 0
