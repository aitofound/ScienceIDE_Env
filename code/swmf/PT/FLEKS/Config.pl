#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

# Allow in-place editing
$^I = "";

# Add local directory to search
push @INC, ".";

use strict;
use File::Copy qw(copy);

our $Component       = 'PC';
our $Code            = 'FLEKS';
our $MakefileDefOrig = 'Makefile.def.FLEKS';
our $MakefileConf    = 'Makefile.conf';
our @Arguments       = @ARGV;

my $config     = "share/Scripts/Config.pl";
my $ConstantsFile = "include/Constants.h";
my $UserSourceFile = "include/UserSource.h";
my $UserSourceDir = "userfiles";

if (@Arguments == 1 and $Arguments[0] eq "-u") {
    &print_user_source_options;
    exit 0;
}

my $GITDIR = "git\@github.com:SWMFsoftware";

if (not -f $config and not -f "../../$config"){
    # Stand-alone
    if (not -f $config) {
        print "--- Cloning SWMFsoftware/share ---\n";
        if (-d "share") {
            system("git clone $GITDIR/share share_tmp && cp -rp share_tmp/. share/ && rm -rf share_tmp") == 0 or die "Error: could not clone share\n";
        } else {
            system("git clone $GITDIR/share") == 0 or die "Error: could not clone share\n";
        }
    }
    if (not -d "util") {
        print "--- Cloning SWMFsoftware/util ---\n";
        system("git clone $GITDIR/util") == 0 or die "Error: could not clone util\n";
    }
    if (not -d "util/AMREX") {
        print "--- Cloning SWMFsoftware/AMREX ---\n";
        system("git clone $GITDIR/AMREX util/AMREX") == 0 or die "Error: could not clone AMREX\n";
    }
}

my $AmrexDir = "util/AMREX";
if(-f $config){
    # Local FLEKS dependency tree. Turn on AMReX automatically, honoring
    # -amrex2d/-amrex3d or defaulting to the currently-linked library's dimension.
    my $AmrexDimArg = "3d";
    foreach (@Arguments){
        if (/^-amrex(2d|3d)?$/i){
            $AmrexDimArg = lc($1 || "3d");
            last;
        }
    }
    unless (grep { /^-amrex(2d|3d)?$/i } @Arguments){
        my $AmrexConfig = "${AmrexDir}/InstallDir/include/AMReX_Config.H";
        if (-f $AmrexConfig) {
            open(FILE, $AmrexConfig) or warn "Config.pl WARNING: could not open $AmrexConfig \n";
            while(<FILE>){
                $AmrexDimArg = "$1d" if /^#define\s+AMREX_SPACEDIM\s+(\d+)/i;
            }
            close FILE;
        }
    }
    push @Arguments, "-amrex$AmrexDimArg";
    push @Arguments, "-show" unless @ARGV;
    push @Arguments, "-nodebug", "-install" unless -f $MakefileConf;
    require $config;

    # Compile share lib if it's not there
    if (not -f "lib/libSHARE.a") {
        print "Building libSHARE.a...\n";
        mkdir "lib" unless -d "lib";
        system("cd share/Library/src; make LIB") == 0 or die "Error: could not build libSHARE.a\n";
    }

    die "Error: AMReX does not exist!\n" unless -d $AmrexDir;
}else{

    my $IsInstall = 0;
    foreach (@Arguments) {
        if (/^-install$/) {
            $_ = "-install=c";
        }
        $IsInstall = 1 if /^-install(=.*)?$/;
    }
    push @Arguments, "-install=c" unless -f $MakefileConf or $IsInstall;
    require "../../$config";
    $AmrexDir = "../../util/AMREX"; 
}

# These are inherited from $config
our %Remaining;   # Arguments not handled by share/Scripts/Config.pl
our $Show;
our $Help;
our $ERROR;
our $WARNING;

my $TPSave = "P";
my $NewTPSave;
my %nTPString=(7=>'P', 10=>'PB', 13=>'PBE', 22=>'PBEG');
my %nTPSave=('P' => 7, 'PB' => 10, 'PBE' => 13, 'PBEG' => 22);
my %TPInfo=('P' => "Particle", 'PB' => "Particle+B", 'PBE' => "Particle+B+E", 'PBEG' => "Particle+B+E+GradB");

my $AmrexDim;

my $nLevMax=1;
my $NewLevMax;
my $NewUserSource;
my $ListUserSources;
my $VerboseGmres;

foreach (@Arguments){
     if(/^-s$/)                 {$Show=1;       next};
     if(/^-h$/)                 {$Help=1;       next};     
     if(/^-tp=(.*)$/)           {$NewTPSave=$1; next};
     if(/^-lev$/)               {$NewLevMax=1;  next};
     if(/^-lev=(.*)$/)          {$NewLevMax=$1; next};
     if(/^-u$/)                 {$ListUserSources=1; next};
     if(/^-u=(.*)$/)            {$NewUserSource=$1; next};
     if(/^-debug$/i)            {$VerboseGmres="yes"; next};
     if(/^-nodebug$/i)          {$VerboseGmres="no";  next};
     warn "WARNING: Unknown flag $_\n" if $Remaining{$_};
}

die "$ERROR -tp input should be 'P', 'PB', 'PBE', or 'PBEG'.\n"
    if $NewTPSave and not exists $nTPSave{$NewTPSave};

die "$ERROR -u input should use letters, numbers, or underscores.\n"
    . &get_user_source_options_text
    if defined $NewUserSource and $NewUserSource !~ /^[A-Za-z0-9_]+$/;

my $AmrexComp;
my $AmrexDebug;
my $AmrexTinyProfile; 

if ($ListUserSources) {
    &print_user_source_options;
    exit 0;
}

&get_settings;

# During install, seed a local UserSource.h from the default template only if
# the user did not select one and no local UserSource.h exists yet.
if (!defined $NewUserSource && !-f $UserSourceFile &&
    scalar grep { /^-install(=.*)?$/ } @Arguments) {
    $NewUserSource = "Default";
}

&set_user_source if defined $NewUserSource;

&set_test_particle if $NewTPSave and $NewTPSave ne $TPSave;

&set_grid if $NewLevMax and $NewLevMax ne $nLevMax;

&set_verbose_gmres if defined $VerboseGmres;

&print_help if $Help;

&show_settings if $Show;

exit 0;
#############################################################################

sub get_settings{
    my $AmrexMakefile = "${AmrexDir}/GNUmakefile";
    open(FILE, $AmrexMakefile) or warn "$WARNING could not open $AmrexMakefile \n";
    while(<FILE>){
       	next if /^\s*!/; # skip commented out lines
       	$AmrexComp = $1 if/^COMP = (\S*)/i;
    }
    close FILE;

    $AmrexDebug = "False";
    $AmrexTinyProfile = "False";
    my $AmrexConfig = "${AmrexDir}/InstallDir/include/AMReX_Config.H";
    open(FILE, $AmrexConfig) or warn "$WARNING could not open $AmrexConfig \n";
    while(<FILE>){
      	$AmrexDebug = "True" if/^#define AMREX_DEBUG/i; 
        $AmrexTinyProfile = "True" if/^#define AMREX_TINY_PROFILING/i; 
        $AmrexDim = $1 if/^#define AMREX_SPACEDIM (\S*)/i; 
    }
    close FILE;

    system("make", "-s", $ConstantsFile) == 0
        or die "$ERROR could not create $ConstantsFile\n";
    my $nSize;
    open(FILE, $ConstantsFile) or die "$ERROR could not open $ConstantsFile\n";   
    while(<FILE>){                                                                      
        $nSize = $2   if /\b(ptRecordSize\s*=\s*)(\d+)/i;
	$nLevMax = $2 if /\b(nLevMax\s*=\s*)(\d+)/i;
    }
    $TPSave = $nTPString{$nSize};
    close FILE;
}
################################################################################
sub set_grid{
    $nLevMax = $NewLevMax if $NewLevMax;

    @ARGV = ($ConstantsFile);
    while(<>){
        s/\b(nLevMax\s*=[^0-9]*)(\d+)/$1$nLevMax/i;
        print;
    }
}
################################################################################
sub set_verbose_gmres{
    # Set the verbose GMRES residual logging flag in src/Makefile:
    #   -debug   -> VERBOSE_GMRES = -DGMRES_VERBOSE (log residual of every matvec)
    #   -nodebug -> VERBOSE_GMRES is empty (only iteration count + final residual)
    my $VERBOSE_GMRES = "";
    $VERBOSE_GMRES = '-DGMRES_VERBOSE' if $VerboseGmres eq "yes";

    print "Setting verbose GMRES logging to '$VerboseGmres' in src/Makefile\n";

    @ARGV = ("src/Makefile");
    while(<>){
        s/^(\s*VERBOSE_GMRES\s*=).*/$1 $VERBOSE_GMRES/;
        print;
    }
}
################################################################################
sub set_test_particle{
    $TPSave = $NewTPSave if $NewTPSave;

    my $nSize = $nTPSave{$TPSave};
    @ARGV = ($ConstantsFile);
    while(<>){
        s/\b(ptRecordSize\s*=[^0-9]*)(\d+)/$1$nSize/i;
        print;
    } 
}
################################################################################
sub set_user_source{
    my $sourceFile = "$UserSourceDir/${NewUserSource}Source.h";

    die "$ERROR could not find $sourceFile\n" . &get_user_source_options_text
        unless -f $sourceFile;

    copy($sourceFile, $UserSourceFile)
        or die "$ERROR could not copy $sourceFile to $UserSourceFile: $!\n";
}
################################################################################
sub print_user_source_options{
    print &get_user_source_options_text;
}
################################################################################
sub get_user_source_options_text{
    my $text = "Available user source options:\n";

    my @sourceFiles = sort glob("$UserSourceDir/*Source.h");
    unless (@sourceFiles) {
        return $text . "  None found in $UserSourceDir\n";
    }

    foreach my $sourceFile (@sourceFiles) {
        my $name = $sourceFile;
        $name =~ s#^\Q$UserSourceDir\E/##;
        $name =~ s/Source\.h$//;
        $text .= "  $name\n";
    }

    return $text;
}
################################################################################
sub show_settings{
    &get_settings;    

    print "AMReX: compiler     = $AmrexComp \n";
    print "AMReX: debug        = $AmrexDebug \n";
    print "AMReX: nDim         = $AmrexDim \n";
    print "AMReX: nLevMax      = $nLevMax \n";
    print "AMReX: tiny profile = $AmrexTinyProfile \n";
    print "Test Particle info  = $TPInfo{$TPSave} \n";
    print "User Source         = " . &get_user_source_info . " \n";
}
################################################################################
sub get_user_source_info{
    open(my $file, "<", $UserSourceFile)
        or return "Unavailable ($UserSourceFile not found)";

    local $/;
    my $content = <$file>;
    close $file;

    if($content =~ /\binfo\s*=\s*"((?:\\.|[^"\\])*)"\s*;/s){
        my $info = $1;
        $info =~ s/\\(["\\])/$1/g;
        return $info;
    }

    return "Unavailable (info not set in $UserSourceFile)";
}
################################################################################
sub print_help{

    print "
-s            Show the configuration for AMReX.

-debug        Compile with debug flags. Verbose GMRES residual logging is
              switched on: VERBOSE_GMRES = -DGMRES_VERBOSE in src/Makefile.
-nodebug      Do not use debug flags, and switch off the verbose GMRES
              residual logging (only the iteration count and the final
              residual are reported by the solver).

-lev          Number of maximum grid levels. It is a uniform grid without AMR if lev=1.

-amrex2d      Link with the 2D AMReX library (AMReX_SPACEDIM = 2).
-amrex3d      Link with the 3D AMReX library (AMReX_SPACEDIM = 3). This is the default.
              AMReX is enabled automatically in the standalone FLEKS setup; by default the
              dimension of the currently-linked library is kept unless -amrex2d/-amrex3d is given.

-u            Show available user source options.

-u=NAME       Select user source file userfiles/NAMESource.h.

-tp=P,PB,PBE,PBEG  Test particle output information.
              P: only save particle velocity + location
              PB: particle + magnetic field. 
              PBE: particle + magnetic field + electric field
              PBEG: particle + B-field + E-field + gradient of B-field

\n";
    exit -0;
}
