#!/bin/sh
#
# prep-rpm-eval.sh
#
# make the yumapro build code and tar it up for rpmbuild
# EVAL version

VER="12.09"
SPEC="yumapro-eval.spec"
YNAME=yumapro-eval-$VER
TARGET=$HOME/swdev/$YNAME

if [ ! -d $TARGET ]; then
  echo "Error: $TARGET not found"
  echo "  1: git clone https://github.com/YumaWorks/ypwork $YNAME"
  echo "  2: cd $YNAME; git checkout cheetah-integ; git pull"
  exit 1
fi

mkdir -p ~/rpmprep
rm -rf ~/rpmprep/*

cd ~/rpmprep

cd $HOME/swdev
touch $YNAME/configure
chmod 775 $YNAME/configure
rm -f $YNAME.tar.gz
tar --exclude=.git* --exclude=.svn* -cvf $YNAME.tar  $YNAME/
gzip $YNAME.tar
rm $YNAME/configure

mkdir -p ~/rpmbuild/SPECS
mkdir -p ~/rpmbuild/SOURCES
mkdir -p ~/rpmbuild/SRPMS
mkdir -p ~/rpmbuild/RPMS

mv $YNAME.tar.gz ~/rpmbuild/SOURCES
cp $YNAME/SPECS/$SPEC ~/rpmbuild/SPECS
