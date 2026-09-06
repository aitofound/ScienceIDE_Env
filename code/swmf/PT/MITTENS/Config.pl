#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

# Allow in-place editing                                                        
$^I = "";

# Add local directory to search                                                 
push @INC, ".";

use strict;

our $Component = "PT";
our $Code = "MITTENS";
our $MakefileDefOrig = 'src/Makefile.def';
our @Arguments= @ARGV;


my $config     = "share/Scripts/Config.pl";
# get util and share
my $GITCLONE = "git clone"; my $GITDIR = "git\@github.com:SWMFsoftware/";

if (-f $config or -f "../../$config"){
}else{
    `$GITCLONE $GITDIR/share.git; $GITCLONE $GITDIR/util.git`;
}

if(-f $config){
    require $config;
}else{
    require "../../$config";
}

# Variables inherited from share/Scripts/Config.pl
our %Remaining; # Unprocessed arguments
our $ERROR;
our $WARNING;
our $Help;
our $Verbose;
our $Show;
our $ShowGridSize;
our $NewGridSize;
our $NewGhostCell;


&print_help if $Help;

my $Src = 'src';

# Grid size variables
my $NameSizeFile = "$Src/ModSize.f90";
my $GridSize;
my $nX;

# Read previous grid size
&get_settings;

foreach (@Arguments){
    if(/^-s$/)                {$Show=1;          next};
    if(/^-g$/)                {$ShowGridSize=1;  next};
    warn "WARNING: Unknown flag $_\n" if $Remaining{$_};
}


# Set new grid size
&set_grid_size if ($NewGridSize and $NewGridSize ne $GridSize);

# Show current settings
my $Settings = &current_settings; print $Settings if $Show;

# Show grid
print "-g=$GridSize\n" if $ShowGridSize;

exit 0;

#############################################################################

sub get_settings{

    $nX = 0;

    # Read size of the grid from $NameSizeFile
    open(FILE, $NameSizeFile) or die "$ERROR could not open $NameSizeFile\n";
    while(<FILE>){
	next if /^\s*!/;
	$nX = $1 if /\bnVertexMax\s*=[^0-9]*(\d+)/i;
    }
    close FILE;

    die "$ERROR could not read nVertexMax from $NameSizeFile\n"
	unless $nX > 0;

    $GridSize = "$nX";

}

#############################################################################

sub set_grid_size{

    $GridSize = $NewGridSize if $NewGridSize;

    if($GridSize =~ /^[1-9]\d*$/){
	$nX = $GridSize;
    }elsif($GridSize){
	die "$ERROR -g=$GridSize must be a positive integer\n";
    }
    # Check the grid size (to be set)
    die "$ERROR nVertexMax=$nX must be positive\n" if $nX<=0;

    print "Writing new grid size $GridSize into ".
	"$NameSizeFile ...\n";

    @ARGV = ($NameSizeFile);
    while(<>){
	if(/^\s*!/){print; next} # Skip commented out lines
	s/\b(nVertexMax\s*=[^0-9]*)(\d+)/$1$nX/i;
	print;
    }
}

#############################################################################

sub print_help{

    print "
Additional options for MITTENS/Config.pl:

-g=nX
                Set grid size. nX is the maximum number of vertices
                per field line.
\n";
    exit 0;
}

#############################################################################

sub current_settings{
    $Settings  = "Number of nodes per line: $nX\n";
}

