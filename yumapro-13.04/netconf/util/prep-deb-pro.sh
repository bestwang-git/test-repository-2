#!/bin/sh
#
# prep-deb-pro.sh
#
# make the yumapro build code and tar it up for debian
# Only supports packaging of integ branch!!!

VER="12.09"

if [ ! -d $HOME/swdev/yumapro-$VER ]; then
  echo "Error: $HOME/swdev/yumapro-$VER not found"
  echo "  1: git clone https://github.com/YumaWorks/ypwork yumapro-$VER"
  echo "  2: cd yumapro-$VER; git checkout bobcat; git pull origin bobcat"
  exit 1
fi

mkdir -p $HOME/build

cd $HOME/swdev
tar --exclude=.git* --exclude=.svn* -cvf yumapro_$VER.tar  yumapro-$VER/
gzip yumapro_$VER.tar
mv yumapro_$VER.tar.gz $HOME/build

cd $HOME/build
if [ ! -f yumapro_$VER.orig.tar.gz ]; then
  cp yumapro_$VER.tar.gz yumapro_$VER.orig.tar.gz
fi

rm -rf yumapro-$VER
tar xvf yumapro_$VER.tar.gz





