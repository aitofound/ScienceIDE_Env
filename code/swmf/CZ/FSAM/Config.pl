#!/usr/bin/perl

# Allow in-place editing                                                        
$^I = "";

# Add local directory to search                                                 
push @INC, ".";

use strict;
our @Arguments       = @ARGV;
our $MakefileDefOrig = "src/Makefile.def";
our $Component = "CZ";
our $Code      = "FSAM";

my $config = "share/Scripts/Config.pl";

# get util and share
my $GITCLONE = "git clone"; my $GITDIR = "git\@gitlab.umich.edu:swmf_software/";
if (-f $config or -f "../../$config"){
}else{
    `$GITCLONE $GITDIR/share.git; $GITCLONE $GITDIR/util.git`;
}



if(-f $config){
    require $config;
}else{
    require "../../$config";
}
