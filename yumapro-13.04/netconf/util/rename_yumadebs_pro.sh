#!/bin/sh
#
# Rename the yumapro package files
#
# P1 == the YumaPro version number to use

if [ $# != 1 ]; then
  echo "Usage: rename_yumadebs_pro.sh yumapro-version"
  echo "   rename_yumadebs_pro.sh 12.09-1"
  exit 1
fi

if [ -z $YUMA_ARCH ]; then
  echo "Error: Missing YUMA_ARCH environment variable.";
  exit 1
fi

if [ -z $YUMA_OS ]; then
  echo "Error: Missing YUMA_OS environment variable.";
  exit 1
fi

if [ ! -d ~/build ]; then
  echo "Error: ~/build directory not found"
  exit 1
fi

UV="$YUMA_OS"
ARCH="$YUMA_ARCH"

mkdir -p ~/YUMAPRO_PACKAGES

if [ ! -d ~/YUMAPRO_PACKAGES ]; then
  echo "Error: ~/YUMAPRO_PACKAGES directory could not be created"
  exit 1
fi

cd ~/build

if [ ! -f ./yumapro_$1_$ARCH.deb ]; then
  echo "Error: yumapro_$1_$ARCH.deb not found"
  exit 1
fi

cp -f ./yumapro_$1_$ARCH.deb ~/YUMAPRO_PACKAGES/yumapro-$1."$UV".$ARCH.deb
cp -f ./yumapro-eval_$1_$ARCH.deb ~/YUMAPRO_PACKAGES/yumapro-eval-$1."$UV".$ARCH.deb
cp -f ./yumapro-dev_$1_all.deb ~/YUMAPRO_PACKAGES/
cp -f ./yumapro-doc_$1_all.deb ~/YUMAPRO_PACKAGES/

    
    



