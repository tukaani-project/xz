#!/bin/sh

###############################################################################
# Author: Anders F Björklund <afb@users.sourceforge.net>
#
# This file has been put into the public domain.
# You can do whatever you want with this file.
###############################################################################

mkdir -p Root
mkdir -p Resources

# Abort immediately if something goes wrong.
set -e

# Clean up if it was already configured.
[ -f Makefile ] && make distclean

# Build the regular fat program

CC="gcc-4.0" \
CFLAGS="-O2 -g -arch ppc -arch ppc64 -arch i386 -arch x86_64 -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.4" \
../configure --disable-dependency-tracking --disable-xzdec --disable-lzmadec i686-apple-darwin8

make

make check

make DESTDIR=`pwd`/Root install

make distclean

# Build the size-optimized program

CC="gcc-4.0" \
CFLAGS="-Os -g -arch ppc -arch i386 -isysroot /Developer/SDKs/MacOSX10.4u.sdk -mmacosx-version-min=10.4" \
../configure --disable-dependency-tracking --disable-shared --disable-nls --disable-encoders --enable-small --disable-threads i686-apple-darwin8

make -C src/liblzma
make -C src/xzdec
make -C src/xzdec DESTDIR=`pwd`/Root install

cp -a ../extra Root/usr/local/share/doc/xz

make distclean

# Strip debugging symbols and make relocatable

for bin in xz lzmainfo xzdec lzmadec; do
    strip -S Root/usr/local/bin/$bin
    install_name_tool -change /usr/local/lib/liblzma.5.dylib @executable_path/../lib/liblzma.5.dylib Root/usr/local/bin/$bin
done

for lib in liblzma.5.dylib; do
    strip -S Root/usr/local/lib/$lib
    install_name_tool -id @executable_path/../lib/liblzma.5.dylib Root/usr/local/lib/$lib
done

strip -S  Root/usr/local/lib/liblzma.a
rm -f Root/usr/local/lib/liblzma.la

# Include pkg-config while making relocatable

sed -e 's|prefix=/usr/local|prefix=${pcfiledir}/../..|' < Root/usr/local/lib/pkgconfig/liblzma.pc > Root/liblzma.pc
mv Root/liblzma.pc Root/usr/local/lib/pkgconfig/liblzma.pc

# Create tarball, but without the HFS+ attrib

rmdir debug lib po src/liblzma/api src/liblzma src/lzmainfo src/scripts src/xz src/xzdec src tests

( cd Root/usr/local; COPY_EXTENDED_ATTRIBUTES_DISABLE=true COPYFILE_DISABLE=true tar cvjf ../../../XZ.tbz * )

# Include documentation files for package

cp -p ../README Resources/ReadMe.txt
cp -p ../COPYING Resources/License.txt

# Make an Installer.app package

ID="org.tukaani.xz"
VERSION=`cd ..; sh build-aux/version.sh`
PACKAGEMAKER=/Developer/Applications/Utilities/PackageMaker.app/Contents/MacOS/PackageMaker
$PACKAGEMAKER -r Root/usr/local -l /usr/local -e Resources -i $ID -n $VERSION -t XZ -o XZ.pkg -g 10.4 --verbose

# Put the package in a disk image

hdiutil create -fs HFS+ -format UDZO -quiet -srcfolder XZ.pkg -ov XZ.dmg
hdiutil internet-enable -yes -quiet XZ.dmg

echo
echo "Build completed successfully."
echo
