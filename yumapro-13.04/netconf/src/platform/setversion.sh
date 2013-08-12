#!/bin/sh
#
# get the subversion build number
# updated 2012-06-09 for git use

echo "#define SVNVERSION \"`../util/gitversion`\"" > platform/curversion.h



