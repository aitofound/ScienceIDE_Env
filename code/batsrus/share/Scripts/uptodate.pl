#!/usr/bin/perl -s
# Usage: uptodate.pl [-v] FILE [FILE1 FILE2 DIR1 DIR2 ...]
# Returns "FILE does not exist" if FILE does not exist.
# Returns all files that are newer than FILE listed
# in the optional file and directory arguments.
# If there is no optional argument, the local directory is used.
# If there is nothing newer and FILE exists, an empty string is returned.
# This can be used in Makefile-s as
# if [ `uptodate.pl ${TARGETFILE} $DEPENDENCIES` ]; then ... ; fi

use strict;

my $file = shift @ARGV or die "ERROR in uptodate.pl: Missing file name\n";
my $dir  = (join(" ", @ARGV) or ".");

if(-f $file){
    print "UPDATE\n" if `find $dir -type f -newer $file`;
}else{ 
    print "CREATE\n";
}
