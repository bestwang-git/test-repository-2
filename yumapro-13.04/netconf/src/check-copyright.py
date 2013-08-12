#!/usr/bin/env python
import sys
import os
import commands

copyright = " * Copyright (c) 2008 - 2012, Andy Bierman, All Rights Reserved.\n"

yw_copyright = " * Copyright (c) 2012, YumaWorks, Inc., All Rights Reserved.\n"

copyright_start = " * Copyright (c) "

yw_find = "YumaWorks"

# ----------------------------------------------------------------------------|
class NullFilter:
    def apply( self, filename ):
        """Apply a 'null' filter to the filename supplied."""
        return True

# ----------------------------------------------------------------------------|
class EndswithFilenameFilter:
    def __init__( self, filters = [ '.c', '.h', '.cpp', '.inl' ] ):
        self.filters_ = filters

    def apply( self, filename ):
        """Apply an 'endswith' filter to the filename supplied."""
        for f in self.filters_:
            if filename.endswith( f ):
                return True
        return False

# ----------------------------------------------------------------------------|
class StartswithFilenameFilter:
    def __init__( self, filters = [ 'M ', 'A ' ] ):
        self.filters_ = filters

    def apply( self, filename ):
        """Apply an 'startswith' filter to the filename supplied."""
        for f in self.filters_:
            if filename.startswith( f ):
                return True
        return False

# ----------------------------------------------------------------------------|
class SVNModifiedFilenameFilter:
    def __init__( self ):
        self.startFilter = StartswithFilenameFilter()
        self.endFilter = EndswithFilenameFilter()

    def apply( self, filename ):
        """Apply the filter"""
        if self.startFilter.apply( filename ) and self.endFilter.apply( filename ):
            return True
        return False

# ----------------------------------------------------------------------------|
def DisplayUsage():
    print ( """Usage: check-copyright.py [-a]
               Check the copyright lines in the modified source files.

               [-a] : Check all source files.""" )
    sys.exit(-1)

# ----------------------------------------------------------------------------|
def GetAllFilenames( rootDir = "./", filenameFilter = NullFilter() ):
    """Get the list of all filenames matching the supplied filter"""
    filenames = []
    for root, dirs, files in os.walk( rootDir ):
        filtered = [ n for n in files 
                     if filenameFilter.apply( n ) ]
        filenames += [ root + "/" + n for n in filtered ]
    return filenames

# ----------------------------------------------------------------------------|
def GetModifiedFiles():
    gitOp = commands.getoutput( "git status -s" )
    gitOp = gitOp.split( '\n' )
    filenameFilter = SVNModifiedFilenameFilter()
    filenames = []
    for entry in svnOp:
        if filenameFilter.apply( entry ):
            filenames.append( entry.lstrip( "MA ") )
    return filenames


# ----------------------------------------------------------------------------|
def CheckFile( filename ):
    """Check the copyright line as the file is being copied"""
    print "Checking %s...." %filename
    nextline = 0
    foundcopy = 0
    checkdone = 0
    copydone = 0

    outfilename = filename + ".tmp"
    f = open( filename, 'r' )
    of = open( outfilename, 'w' )
    lines = f.readlines()

    for line in lines:
        if checkdone == 0 and line.startswith(copyright_start):
            if line.find(yw_find) == -1:
                # found the Andy Bierman copyright
                nextline = 1
            else:
                # found YumaWorks copyright, done
                checkdone = 1
                nextline = 0
            of.write(line)
        elif nextline == 1:
            # did not find YumaWorks Copyright after the AB copyright
            nextline = 0
            of.write(yw_copyright)
            of.write(line)
            copydone = 1
            checkdone = 1
        else:
            of.write(line)
        
    f.close()
    of.close()
    if copydone:
        os.remove( filename )
        os.rename( outfilename, filename )
    else:
        os.remove( outfilename )



# ----------------------------------------------------------------------------|
def CheckFiles( filenames ):
    """Check each of the files"""
    for filename in filenames:
        CheckFile( filename )
        

# ----------------------------------------------------------------------------|
if __name__ == '__main__':
    if len ( sys.argv ) >1 :
        if sys.argv[1] == "-a":
            filenames = GetAllFilenames( filenameFilter = EndswithFilenameFilter() )
        else:
            DisplayUsage()
    else:
        filenames = GetModifiedFiles()

    CheckFiles( filenames )


