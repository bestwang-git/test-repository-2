#!/bin/sh
#
# prep-rpm.sh
#
# make the yumapro build code and tar it up for rpmbuild
#

VER="12.09"
BRANCH="integ"
SPEC="yumapro.spec"
EVALSPEC="yumapro-eval.spec"
MINSPEC="yumapro-min.spec"

if [ ! -d $HOME/swdev/yumapro-$VER ]; then
  echo "Error: $HOME/swdev/yumapro-$VER not found"
  echo "  1: git clone https://github.com/YumaWorks/ypwork yumapro-$VER"
  echo "  2: cd yumapro-$VER; git checkout cheetah-integ; git pull"
  exit 1
fi

mkdir -p ~/rpmprep
rm -rf ~/rpmprep/*

cd ~/rpmprep

cd $HOME/swdev
touch yumapro-$VER/configure
chmod 775 yumapro-$VER/configure
rm -f yumapro-$VER.tar.gz
tar --exclude=.git* --exclude=.svn* -cvf yumapro-$VER.tar  yumapro-$VER/
gzip yumapro-$VER.tar
rm yumapro-$VER/configure

mkdir -p ~/rpmbuild/SPECS
mkdir -p ~/rpmbuild/SOURCES
mkdir -p ~/rpmbuild/SRPMS
mkdir -p ~/rpmbuild/RPMS

mv yumapro-$VER.tar.gz ~/rpmbuild/SOURCES
cp yumapro-$VER/SPECS/$SPEC ~/rpmbuild/SPECS
cp yumapro-$VER/SPECS/$MINSPEC ~/rpmbuild/SPECS
