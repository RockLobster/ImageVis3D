#!/bin/sh

VERSION=4.8.4
PREFIX="${HOME}/sw"
QTDIR="qt-everywhere-opensource-src-${VERSION}"
echo "Removing old build..."
rm -fr ${QTDIR}

tarball="${QTDIR}.tar"

echo "Extracting..."
# Do they have a bzip'd or a gzip'd tarball?
if test -f ${tarball}.bz2 ; then
  tar jxf ${tarball}.bz2
elif test -f ${tarball}.gz ; then
  tar zxf ${tarball}.gz
else
  echo "${tarball}.gz not found; Downloading Qt..."
  curl -kLO http://releases.qt-project.org/qt4/source/${tarball}.gz
  tar zxf ${tarball}.gz
fi
pushd ${QTDIR} || exit 1
echo "yes" | \
./configure \
        -prefix ${HOME}/sw \
        -buildkey "imagevis3d" \
        -release \
        -opensource \
        -largefile \
        -exceptions \
        -fast \
        -stl \
        -no-qt3support \
        -no-xmlpatterns \
        -no-multimedia \
        -no-audio-backend \
        -no-phonon \
        -no-phonon-backend \
        -no-svg \
        -no-webkit \
        -no-javascript-jit \
        -no-script \
        -no-scripttools \
        -no-declarative \
        -no-declarative-debug \
        -platform macx-g++ \
        -no-scripttools \
        -system-zlib \
        -no-gif \
        -qt-libtiff \
        -qt-libpng \
        -qt-libmng \
        -qt-libjpeg \
        -no-openssl \
        -make libs \
        -make tools \
        -nomake examples \
        -nomake demos \
        -nomake docs \
        -nomake translations \
        -no-nis \
        -no-cups \
        -no-iconv \
        -no-pch \
        -no-dbus \
        -arch "x86 x86_64"

if test $? -ne 0; then
        echo "configure failed"
        exit 1
fi

nice make -j6 || exit 1

rm -fr ${PREFIX}/bin/qmake ${PREFIX}/lib/libQt* ${PREFIX}/lib/Qt*
rm -fr ${PREFIX}/include/Qt*

nice make install || exit 1

popd
