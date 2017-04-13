
#!/bin/bash

if cat /etc/issue | grep "Amazon Linux AMI" > /dev/null; then

    yum groupinstall "Developer Tools"
    yum install perl-version
    yum install libxcb libxcb-devel xcb-util xcb-util-devel
    yum install flex bison gperf libicu-devel libxslt-devel ruby sqlite-devel
    yum install -y cmake netpbm-devel python-multiprocessing gcc44-c++ libX11-devel libXext-devel libXrender-devel freetype-devel fontconfig-devel xorg-x11-xfs xorg-x11-xfs-utils rpm-build libtool.i386 automake autoconf m4

    if [ ! -e /usr/bin/g++ ]; then
        ln -s /usr/bin/g++44 /usr/bin/g++
    fi
fi

