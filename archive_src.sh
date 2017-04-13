#!/bin/bash
source common.sh
VERSION=$(ichabod_version)
echo "Making S/RPM for version $VERSION"
mkdir srcbuild
pushd srcbuild
git clone https://github.com/monetate/ichabod.git
pushd ichabod
subm
popd
echo "Tarring source..."
tar cf ichabod-$VERSION.tar ichabod/
echo "Zipping source..."
gzip ichabod-$VERSION.tar
mv ichabod-$VERSION.tar.gz ../
popd
echo "Cleaning up..."
rm -rf srcbuild
