#!/bin/bash

function subm() {

    git config status.submodulesummary 1
    git config -f .gitmodules submodule.qt.branch 5.5.1

    # initial setup
    #git submodule add https://github.com/cesanta/mongoose.git
    #git submodule add https://github.com/wkhtmltopdf/wkhtmltopdf.git
    #git submodule add https://github.com/jacobsa/jsoncpp.git
    #git submodule add https://code.qt.io/qt/qt5.git

    git submodule init
    git submodule update

}

function die()
{
    echo $1
    exit 666
}

function ichabod_version() {
    cat version.h | awk '{print $3;}' | tr -d \"
}
