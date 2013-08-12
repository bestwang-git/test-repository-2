#!/bin/sh
#
# make-src-tarball-pro.sh \
#  <major-version> <minor-version> <release-number> <branch>
#
# make the YumaPro source tarball
#
# the $1 parameter must be a major version number, such as 12
# the $2 parameter must be a minor version number, such as 09
# the $3 parameter must be the release number, such as 1
# the $4 parameter must be the branch, such as bobcat or cheetah

if [ $# != 4 ]; then
  echo "Usage: make-src-tarball <major-version> <minor-version> <release-number> <branch>"
  echo "Example:   'make-src-tarball 12 09 1 bobcat' for version 12.09-1"
  exit 1
fi

# temp! using integ branch instead of master!!
echo "Making yumapro source tarball for $1.$2-$3 from $4 branch"
mkdir -p ~/srctarballprep
cd ~/srctarballprep
rm -rf yumapro-$1.$2-$3 yumapro-$1.$2-$3.tar.gz
git clone https://github.com/YumaproWorks/ypwork yumapro-$1.$2-$3  
cd yumapro-$1.$2-$3
git checkout $4

echo "echo \"#define RELEASE $3\" > platform/curversion.h" > \
    netconf/src/platform/setversion.sh
cd ..
tar --exclude=.git* --exclude=.svn* -cvf yumapro-$1.$2-$3.tar yumapro-$1.$2-$3
gzip yumapro-$1.$2-$3.tar






