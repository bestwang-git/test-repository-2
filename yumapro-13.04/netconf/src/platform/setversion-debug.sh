#!/bin/sh
#
# get the subversion build number
# updated 2012-06-09 for git use
# DEBUG VERSION

echo "#define SVNVERSION \"`../util/gitversion-debug`\"" > platform/curversion.h

