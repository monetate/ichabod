#!/bin/bash

source common.sh
name='ichabod'
name=${name%.spec}
topdir=$(mktemp -d)
VERSION=$(ichabod_version)
builddir=${TMPDIR:-/tmp}/${name}-${VERSION}
sourcedir="${topdir}/SOURCES"
buildroot="${topdir}/BUILD/${name}-${VERSION}-root"
mkdir -p ${topdir}/RPMS ${topdir}/SRPMS ${topdir}/SOURCES ${topdir}/BUILD
mkdir -p ${buildroot} ${builddir}
echo "=> Copying sources..."
mv ichabod-$VERSION.tar.gz ${topdir}/SOURCES
echo "=> Building RPM..."
rpmbuild --define "_topdir ${topdir}" --buildroot ${buildroot} --clean -ba ${name}.spec
find ${topdir}
