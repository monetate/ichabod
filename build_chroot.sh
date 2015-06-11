#!/bin/bash

if [ ! $(uname -m | grep 64) ]; then
    echo "Not 64-bit!"
    exit 666
fi
if [ ! -f /etc/debian_version ] ; then
    echo "Not debian!"
    exit 666
fi
git submodule add https://github.com/cesanta/mongoose.git
git submodule add https://github.com/wkhtmltopdf/wkhtmltopdf.git
git submodule update --init

pushd wkhtmltopdf
git submodule update --init
popd

# prep build area
./wkhtmltopdf/scripts/setup-linux.sh root

# build qt
pushd wkhtmltopdf
./scripts/build.py centos5-i386
popd

# build ichabod
schroot -c wkhtmltopdf-centos5-i386 -- wkhtmltopdf/static-build/centos5-i386/qt/bin/qmake ichabod.pro
schroot -c wkhtmltopdf-centos5-i386 -- make
