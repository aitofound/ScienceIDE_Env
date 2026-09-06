#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

use strict;
use warnings;
use Math::Trig;

my $HELP = 
    "The script assists visualizing M-FLAMPA and relevant BATS-R-US output;\n".
    "parameters of visualization are set via input file (default: PLOT.in);\n".
    "Required software: Tecplot\n";

my $USAGE = "Usage:\n". 
    " ./PlotTEC.pl [-help] [-param=PLOT_PARAM_FILE_NAME] [-macro-only]\n".
    "Options:\n".
    " -h -help\t\tdisplay help message\n".
    " -param=FILENAME\tset plot param file to FILENAME (default: PLOT.in)\n".
    " -macro-only\t\tcreate macros without running Tecplot batch mode\n";

# name of the input file
my $INPUT = "PLOT.in";

# whether to run Tecplot batch mode
my $DoRunBatch = 1;

#\
# process command line arguments
#/
foreach my $arg (@ARGV){
    if($arg =~ m/^(-h|-help)$/){
	print "Displaying help message, other arguments are ignored\n".
	    ('-'x80)."\n" if(@ARGV > 1);
	print "$HELP\n$USAGE";
	exit;
    }
    elsif($arg =~ m/^-param=(.*)$/){
	$INPUT = $1;
    }
    elsif($arg =~ m/^-macro-only$/){
	$DoRunBatch = 0;
    }
    else{
	die "ERROR: Unkown option ($arg)\n".('-'x80)."\n$USAGE";
    }
}

#\
# Tecplot batch mode command
#/
my $TecBatch = 'tec360 -b -p';

#\
# Tecplot macro file
#/
my $MacroMain = 'SEP.mcr';

#\
# numerical constants
#/
my $pi = 3.14159265358979323846;
my $rad= $pi/180;
my $deg= 180/$pi;

#\
# Tecplot-style lists of variables in BATS-R-US and M-FLAMPA
#/
my $BatsrusVar = '"X [R]" "Y [R]" "Z [R]" "`r [g/cm^3]" "U_x [km/s]" "U_y [km/s]" "U_z [km/s]" "B_x [Gauss]" "B_y [Gauss]" "B_z [Gauss]" "ehot [erg/cm^3]" "I01 [erg/cm^3]" "I02 [erg/cm^3]" "pe [dyne/cm^2" "p [dyne/cm^2" "B1_x [Gauss]" "B1_y [Gauss]" "B1_z [Gauss]" "E [erg/cm^3]" "J_x [`mA/m^2]" "J_y [`mA/m^2]" "J_z [`mA/m^2]"';
my $nBatsrusVar = split(/" "/, $BatsrusVar);
my $MflampaVar = '"I" "LagrID" "X_[RSun]" "Y_[RSun]" "Z_[RSun]" "Rho_[amu/m3]" "T_[kev]" "Ux_[m/s]" "Uy_[m/s]" "Uz_[m/s]" "Bx_[T]" "By_[T]" "Bz_[T]" "Wave1_[J/m3]" "Wave2_[J/m3]" "flux_total_[pfu]" "flux_Channel01_[pfu]" "flux_Channel02_[pfu]" "flux_Channel03_[pfu]" "flux_Channel04_[pfu]" "flux_Channel05_[pfu]" "flux_Channel06_[pfu]" "eflux_[kev/cm2*s*sr]"';
my $nMflampaVar = split(/" "/, $MflampaVar);

#\
# names of SC and SP files
#/
my $ScFile  ='';
my $SpFile  ='';

#\
# directories
#/
my $ScDir = '';
my $SpDir = '';
my $OutDir= '';

#\
# Save flags
#/
# flag for CME views
my $SaveCME = 0;

# flag for orbit view
my $SaveOrbit = 0;

# flags for 3 default CME views
my $Save1 = 0;
my $Save2 = 0;
my $Save3 = 0;

#\
# Parameters of custom viewpoint plots
#/
my @CustomX = ();
my @CustomY = ();
my @CustomZ = ();
my @CustomP = ();
my @CustomT = ();

#\
# time signature of files to be plotted
#/
my @Time = ();

#\
# angular location of a CME
#/
my $LonCME = 0;
my $LatCME = 0;

#\
# levels of solar wind speed 
#/
my $nSpeed  = 10;  # number of levels (EXCLUDING zero)
my $DSpeed  = 100; # level increment
my $SpeedMax= 1000;# max solar wind speed

#\
# levels of SEP flux
#/
my $nFlux  = 10;    # number of levels (EXCLUDING zero)
my $iFluxMin= 1;    #
my $DFlux  = 30000; # level increment
my $FluxMax= 300000;# max flux


#\
# trim lines by Larg ID
#/
my $LineTrim = 9999999;


#\
# angular positions of Earth, STEREO A and B
#/
my @LonEarth=();
my @LonStA  =();
my @LonStB  =();

my @LatEarth=();
my @LatStA  =();
my @LatStB  =();

#----------------------------------------------------------------------
#\
# read plot parameters from input file
#/
read_input();

#\
# determine which plots are requested
#/
$SaveCME = $Save1 || $Save2 || $Save3 ||
    @CustomP && @CustomT;
$SaveOrbit = @LonEarth&&@LatEarth && @LonStA&&@LatStA && @LonStB&&@LatStB;

#\
# check that all necessary info has been provided
#/
die "Directory #SCDIR not found in $INPUT" unless ($ScDir || not $SaveCME);
die "Directory #SPDIR not found in $INPUT" unless ($SpDir);
die "Directory #OUTDIR not found in $INPUT" unless ($OutDir);
die "CME location #CME not found in $INPUT" unless ($LonCME && $LatCME);

#\
# check whether Tecplot batch executable is available
#/
unless(`which $TecBatch`){
    print 
	"Tecplot executable is not available; ".
	"re-run script with -macro-only option\n".
	('-'x80)."\n$USAGE";
    exit;
}

#\
# prepare Tecplot and execute macros
#/
for( my $t = 0; $t < @Time; $t++){
    
    my @Files = ();

    if($SaveCME){
	# get the name of SC file
	@Files = process_directory("$ScDir",
				   '3d__.*t'."$Time[$t]".'.*\.plt');
	$ScFile = "$Files[0]";
    }
    
    # get the name of SP files
    @Files = process_directory("$SpDir",
			       'MH_data_t'."$Time[$t]".'.*\.plt');
    $SpFile = "$Files[0]";

    create_macro_cme($Time[$t]);
    
    if($DoRunBatch){
	system("echo 'Plotting timestamp $Time[$t]'");
	system("$TecBatch $MacroMain > /dev/null");
    }
    else{
	system("mv $MacroMain SEP.$Time[$t].mcr");
    }
}

#========================================================================

# get list of files in the given directory matching the pattern
#     process_director($Dir)
sub process_directory{
    # check correctness
    die("Trying to process file $_[0] as a folder!")
        unless (-d "$_[0]");
    
    # copy the directory name without trailing '/'
    my $DirName = $_[0];
    $DirName =~ s/\/*$//;

    # the return values: names of files
    my @Res = ();

    # move to the given directory
    opendir(my $DIR, "$DirName") or die $!;

    # collect all files
    while(my $file = readdir($DIR)){
        # skip folders
	next if(-d "$DirName/$file");
	# ignore temporary files
	next if($file =~ m/^.*~/ || $file =~ m/^#.*#$/);
	# if the file name matches the input mask -> add to the list
	if($file =~ m/$_[1]/){
	    push @Res, $file;
	}
    }
    # move out of the given directory
    closedir($DIR);
    # return found files
    return @Res;
}

#===========================================================================

# get a new time signature:
#     add_time($T0, $Dt)
# both arguments are in the format 'ddhhmmss'
sub add_time{
    # current time stamp
    my $T0 = $_[0];
    # increment
    my $Dt = $_[1];
    # convert into day, hours, minutes, seconds
    my $dd = substr $T0, 0, 2;
    my $hh = substr $T0, 2, 2;
    my $mm = substr $T0, 4, 2;
    my $ss = substr $T0, 6, 2;
    my $Dd = substr $Dt, 0, 2;
    my $Dh = substr $Dt, 2, 2;
    my $Dm = substr $Dt, 4, 2;
    my $Ds = substr $Dt, 6, 2;
    
    # find new time
    $ss += $Ds; if($ss >= 60){$ss -= 60; $Dm += 1;}
    $mm += $Dm; if($mm >= 60){$mm -= 60; $Dh += 1;}
    $hh += $Dh; if($hh >= 24){$hh -= 24; $Dd += 1;}
    $dd += $Dd;

    # convert to timestamp and return
    my $res = 
	sprintf("%02d",$dd).sprintf("%02d",$hh).
	sprintf("%02d",$mm).sprintf("%02d",$ss);
    return $res;
}

#===========================================================================

# read plot parameters from $INPUT file
sub read_input{
    
    # check that input file exists
    die("Plot param file $INPUT doesn't exist") unless(-e "$INPUT");

    # get input file's content
    my $fh;
    open($fh,'<',$INPUT);
    my @lines = <$fh>;
    close($fh);

    # trim leading and trailing spaces
    for(my $i=0; $i < @lines; $i++){
	$lines[$i] =~ s/^\s+|\s+$//g;
    }

    # process input
    my $i = 0;
    until($i == @lines){
	# command for Tecplot in batch mode
	if($lines[$i] =~ m/#TECBATCH/i){
	    $TecBatch = $lines[$i+1];
	    $i += 1;
	}
	
	# directory with SC data files
	elsif($lines[$i] =~ m/#SCDIR/i){
	    $ScDir = $lines[$i+1];

            # unless is a full path, append './' to beginning
	    $ScDir = "./$ScDir" unless (substr($ScDir,0,1) eq '/');

	    die "Directory #SCDIR:\n$ScDir\n\tdoesn't exist" unless(-d $ScDir);
	    $i += 1;
	}
	
	# directory with SP data files
	elsif($lines[$i] =~ m/#SPDIR/i){
	    $SpDir = $lines[$i+1];

	    # unless is a full path, append './' to beginning
	    $SpDir = "./$SpDir" unless (substr($SpDir,0,1) eq '/');

	    die "Directory #SPDIR:\n$SpDir\n\tdoesn't exist" unless(-d $SpDir);
	    $i += 1;
	}
	
	# the output directory for figures
	elsif($lines[$i] =~ m/#OUTDIR/i){
	    $OutDir = $lines[$i+1];

	    # unless is a full path, append './' to beginning
	    $OutDir = "./$OutDir" unless (substr($OutDir,0,1) eq '/');

	    # ensure the folder exists
	    mkdir $OutDir unless(-d $OutDir);
	    $i += 1;
	}
	
	# angular location of CME
	elsif($lines[$i] =~ m/#CME/i){
	    $LonCME = $lines[$i+1];
	    $LatCME = $lines[$i+2];
	    $i += 2;
	}
	
	# angular location of Earth @ start and end
	elsif($lines[$i] =~ m/#EARTH/i){
	    push @LonEarth, $lines[$i+1];
	    push @LatEarth, $lines[$i+2];
	    $i += 2;
	}
	
	# angular location of STEREO A @ start and end
	elsif($lines[$i] =~ m/#STEREOA/i){
	    push @LonStA, $lines[$i+1];
	    push @LatStA, $lines[$i+2];
	    $i += 2;
	}
	
	# angular location of STEREO B @ start and end
	elsif($lines[$i] =~ m/#STEREOB/i){
	    push @LonStB, $lines[$i+1];
	    push @LatStB, $lines[$i+2];
	    $i += 2;
	}
	
	# times of a time series output
	elsif($lines[$i] =~ m/#TIMESERIES/i){
	    my $Begin = $lines[$i+1];
	    my $Dt    = $lines[$i+2];
	    my $N     = $lines[$i+3];
	    my $New   = $Begin;
	    push @Time, $New;
	    for(my $n = 0; $n < $N; $n++){
		$New = add_time($New, $Dt);
		push @Time, $New;
	    }
	    $i += 3;
	}
	
	# time of a snapshot output
	elsif($lines[$i] =~ m/#TIME/i){
	    push @Time, $lines[$i+1];
	    $i += 1;
	}
	
	# whether to save the 3 default projections
	elsif($lines[$i] =~ m/#SAVEDEFAULT/i){
	    $Save1 = $lines[$i+1];
	    $Save2 = $lines[$i+2];
	    $Save3 = $lines[$i+3];
	    $i += 3;
	}
    
	# where to trim lines
	elsif($lines[$i] =~ m/#LINETRIM/i){
	    $LineTrim = $lines[$i+1];
	    $i += 1;
	}    

	# parameters of a custom projection
	elsif($lines[$i] =~ m/#SAVECUSTOM/i){
	    my $Lat = $lines[$i+1];
	    my $Lon = $lines[$i+2];
	    $i += 2;

	    push @CustomP, $Lat;
	    push @CustomT, $Lon;
	    push @CustomX, (-1000*sin($Lat*$rad)*sin($Lon*$rad));
	    push @CustomY, (-1000*sin($Lat*$rad)*cos($Lon*$rad));
	    push @CustomZ, ( 1000*cos($Lat*$rad));
	}

	# custom solar wind speed levels
	elsif($lines[$i] =~ m/#SPEEDLEVEL/i){
	    $nSpeed   = $lines[$i+1];
	    $DSpeed   = $lines[$i+2];
	    $SpeedMax = $DSpeed * $nSpeed;
	    $i += 2;
	}
	
	# custom flux levels
	elsif($lines[$i] =~ m/#FLUXLEVEL/i){
	    $nFlux   = $lines[$i+1];
	    $iFluxMin= $lines[$i+2];
	    $DFlux   = $lines[$i+3];
	    $FluxMax = $DFlux * $nFlux;
	    $i += 2;
	}
	else {
	    # skip, do nothing
	}
	$i += 1;
    }
    
}

#------------------------------------------------------------------------

sub create_macro_cme{#args: $Time
    
    # arguments:
    my $TimeIn = $_[0];
    
    # open the filehandler for the main macro
    my $fh;
    open($fh,'>',$MacroMain);
    
    # TECPLOT HEADER
    print $fh '#!MC 1410';
    # -----------------------------------------------------------------
    print $fh '

#\
# Time signatures
#/
$!VarSet |TIMESTAMP| = \'t'.sprintf("%08d",$TimeIn).'\'
$!VarSet |TIME|= \'T = '.
substr($TimeIn,0,4).':'.substr($TimeIn,4,2).':'. substr($TimeIn,6,2).'\'';
    # -----------------------------------------------------------------
    print $fh "

#\\
# Directories
#/
\$!VarSet |SCDIR|  = '".($SaveCME ? $ScDir : "")."'
\$!VarSet |SPDIR|  = '$SpDir'
\$!VarSet |OUTDIR| = '$OutDir'";
    # -----------------------------------------------------------------
    print $fh '

#\
# Normal vectors for CME slice planes
#/
$!VarSet |N1x| = '.( sin($LonCME*$rad)).'
$!VarSet |N1y| = '.(-cos($LonCME*$rad)).'
$!VarSet |N1z| = 0.0000000000000000

$!VarSet |N2x| = '.(-sin($LatCME*$rad)*cos($LonCME*$rad)).'
$!VarSet |N2y| = '.(-sin($LatCME*$rad)*sin($LonCME*$rad)).'
$!VarSet |N2z| = '.( cos($LatCME*$rad)).               '

$!VarSet |N3x| = '.(cos($LatCME*$rad)*cos($LonCME*$rad)).'
$!VarSet |N3y| = '.(cos($LatCME*$rad)*sin($LonCME*$rad)).'
$!VarSet |N3z| = '.(sin($LatCME*$rad));
    # -----------------------------------------------------------------
    print $fh '

#\
# View Positions for CME slice planes
#/
$!VarSet |VIEW1x| =(1000*|N1x|)
$!VarSet |VIEW1y| =(1000*|N1y|)
$!VarSet |VIEW1z| =    0
$!VarSet |VIEW1p| =   90
$!VarSet |VIEW1t| = '.(360-$LonCME).'

$!VarSet |VIEW2x| = (1000*|N2x|)
$!VarSet |VIEW2y| = (1000*|N2y|)
$!VarSet |VIEW2z| = (1000*|N2z|)
$!VarSet |VIEW2p| = '.($LatCME).'
$!VarSet |VIEW2t| = '.(90-$LonCME).'

$!VarSet |VIEW3x| = (1000*|N3x|)
$!VarSet |VIEW3y| = (1000*|N3y|)
$!VarSet |VIEW3z| = (1000*|N3z|)
$!VarSet |VIEW3p| = '.( 90-$LatCME).'
$!VarSet |VIEW3t| = '.(270-$LonCME);
    # -----------------------------------------------------------------
    print $fh "

#\\
# Save files flags
#/
\$!VarSet |SAVE1| = $Save1
\$!VarSet |SAVE2| = $Save2
\$!VarSet |SAVE3| = $Save3";
    # -----------------------------------------------------------------
    print $fh "

#\
# Name of the datafiles to be read
#/
\$!VarSet |SCFILE|   = '".($SaveCME ? $ScFile : "")."'
\$!VarSet |SPFILE|   = '$SpFile'";
    # -----------------------------------------------------------------
    print $fh "

#\\
# Variables levels
#/
# solar wind speed
\$!VarSet |NSPEED| = '$nSpeed'
\$!VarSet |DSPEED| = '$DSpeed'
\$!VarSet |SPEEDMAX| = '$SpeedMax'
# flux
\$!VarSet |IFLUXMIN| = '$iFluxMin'
\$!VarSet |NFLUX| = '$nFlux'
\$!VarSet |DFLUX| = '$DFlux'
\$!VarSet |FLUXMAX| = '$FluxMax'";
    # -----------------------------------------------------------------
    print $fh "

#\\
# Line trimming
#/
\$!VarSet |LINETRIM| = '$LineTrim'";
    # -----------------------------------------------------------------
    if($SaveOrbit){
	print_orbit_param($fh);
    }
    # -----------------------------------------------------------------
    if($SaveCME){
	print $fh "

#\\
# IMPORT DATA FROM BATS-R-US
#/
\$!READDATASET  '\"|SCDIR|/|SCFILE|\"'
  READDATAOPTION  = NEW
  RESETSTYLE      = YES
  VARLOADMODE     = BYNAME
  ASSIGNSTRANDIDS = YES
  VARNAMELIST     = '$BatsrusVar'";
    # -----------------------------------------------------------------    
    print $fh '

#\
# Extract slices
#/
$!SLICELAYERS SHOW = YES';
    for(my $iSlice=1;$iSlice<=3;$iSlice++){
	print $fh "
\$!SLICEATTRIBUTES 1  SLICESURFACE = ARBITRARY
\$!SLICEATTRIBUTES 1  NORMAL{X = |N${iSlice}x|}
\$!SLICEATTRIBUTES 1  NORMAL{Y = |N${iSlice}y|}
\$!SLICEATTRIBUTES 1  NORMAL{Z = |N${iSlice}z|}
\$!CREATESLICEZONES";
    }
    print $fh '
$!SLICELAYERS SHOW = NO';
    # -----------------------------------------------------------------    
    print $fh '

#\
# Compute new variables
#/
$!ALTERDATA
  EQUATION=\'{B [Gauss]}=sqrt({B_x [Gauss]}**2+{B_y [Gauss]}**2+{B_z [Gauss]}**2)\'
$!ALTERDATA
  EQUATION=\'{U [km/s]} = sqrt({U_x [km/s]}**2+{U_y [km/s]}**2+{U_z [km/s]}**2)\'';
    }
    # -----------------------------------------------------------------
    print $fh '

#\
# IMPORT DATA FROM M-FLAMPA
#/
# first zone containing SP data
$!VarSet |SPFIRST| = ('.($SaveCME ? '|NUMZONES|+' : '')."1)

\$!READDATASET  '\"|SPDIR|/|SPFILE|\"'
  READDATAOPTION = ".($SaveCME ? "APPEND" : "NEW")."
  RESETSTYLE = NO
  VARLOADMODE = BYNAME
  ASSIGNSTRANDIDS = YES
  VARNAMELIST = '".
  ($SaveCME ? "$BatsrusVar \"B [Gauss]\" \"U [km/s]\"" : "")."$MflampaVar'

# last zone containing SP data
\$!VarSet |SPLAST| = |NUMZONES|";
    # -----------------------------------------------------------------
    print $fh '

#\
# set coordinate system and assign coordinates and origin
#/
$!PlotType = Cartesian3D
$!ThreeDAxis XDetail{VarNum = '.(1 + ($SaveCME ? 0 : 2)).'}
$!ThreeDAxis YDetail{VarNum = '.(2 + ($SaveCME ? 0 : 2)).'}
$!ThreeDAxis ZDetail{VarNum = '.(3 + ($SaveCME ? 0 : 2)).'}
$!GlobalThreeD RotateOrigin{X = 0}
$!GlobalThreeD RotateOrigin{Y = 0}
$!GlobalThreeD RotateOrigin{Z = 0}';
    # -----------------------------------------------------------------
    print $fh '

# change M-FLAMPA coordinate names to be consistent with BATS-R-US
$!ALTERDATA  [|SPFIRST|-|SPLAST|]
  EQUATION = \'{X [R]} = {X_[RSun]}\'
$!ALTERDATA  [|SPFIRST|-|SPLAST|]
  EQUATION = \'{Y [R]} = {Y_[RSun]}\'
$!ALTERDATA  [|SPFIRST|-|SPLAST|]
  EQUATION = \'{Z [R]} = {Z_[RSun]}\'

$!FIELDLAYERS SHOWMESH = YES
$!FIELDMAP [1-4]  MESH{SHOW = NO}
$!FIELDMAP [|SPFIRST|-|SPLAST|]  MESH{SHOW = YES}';
    # -----------------------------------------------------------------
    print $fh '

#\
# GOES FLUX
#/
$!ALTERDATA
  EQUATION = \'{FLUX > 10MeV [PFU]} = {flux_Channel02_[pfu]}\'';
    # -----------------------------------------------------------------
    print $fh '

#\
# Contour plot settings
#/
$!FIELDLAYERS SHOWCONTOUR = YES';

    if($SaveCME){
	print $fh '

# solar wind speed
$!SETCONTOURVAR
  VAR = 24
  CONTOURGROUP = 1
  LEVELINITMODE = RESETTONICE

$!CONTOURLEVELS NEW
  CONTOURGROUP = 1
  RAWDATA
1
0
$!LOOP |NSPEED|
  $!VarSet |LEVEL| = (|LOOP| * |DSPEED|)
  $!CONTOURLEVELS ADD
    CONTOURGROUP = 1
    RAWDATA
  1
  |LEVEL|
  $!RemoveVar |LEVEL|
$!ENDLOOP';
    }
    print $fh '

# SEP flux
$!SETCONTOURVAR
  VAR = '.(1 + $nMflampaVar + ($SaveCME ? 2 + $nBatsrusVar : 3)).'
  CONTOURGROUP = 2
  LEVELINITMODE = RESETTONICE

$!VarSet |LEVEL| = (10**((|IFLUXMIN|) * |DFLUX|))
$!CONTOURLEVELS NEW
  CONTOURGROUP = 2
  RAWDATA
1
|LEVEL|
$!RemoveVar |LEVEL|

$!LOOP |NFLUX|
  $!VarSet |LEVEL| = (10**((|IFLUXMIN|+|LOOP|) * |DFLUX|))
  $!CONTOURLEVELS ADD
    CONTOURGROUP = 2
    RAWDATA
  1
  |LEVEL|
  $!RemoveVar |LEVEL|
$!ENDLOOP';
    # -----------------------------------------------------------------
    print $fh '

#\
# Legend
#/

$!GLOBALCONTOUR 1  LEGEND{ISVERTICAL = YES}
$!GLOBALCONTOUR 1  LEGEND{XYPOS{Y = 85}}
$!GLOBALCONTOUR 1  LEGEND{XYPOS{X = 25}}
$!GLOBALCONTOUR 1  LEGEND{HEADERTEXTSHAPE{HEIGHT = 5.5}}
$!GLOBALCONTOUR 1  LEGEND{NUMBERTEXTSHAPE{HEIGHT = 4.5}}
$!GLOBALCONTOUR 1  LEGEND{BOX{BOXTYPE = FILLED}}
$!GLOBALCONTOUR 1  LEGEND{ROWSPACING = 2.00000}

$!GLOBALCONTOUR 1  COLORMAPNAME = \'Sequential - Pink/Purple\'
$!GLOBALCONTOUR 1  COLORMAPFILTER{COLORMAPDISTRIBUTION = CONTINUOUS}
$!GLOBALCONTOUR 1  COLORMAPFILTER{CONTINUOUSCOLOR{CMIN = 0}}
$!GLOBALCONTOUR 1  COLORMAPFILTER{CONTINUOUSCOLOR{CMAX = |SPEEDMAX|}}

$!GLOBALCONTOUR 2  LEGEND{ISVERTICAL = YES}
$!GLOBALCONTOUR 2  LEGEND{XYPOS{Y = 100}}
$!GLOBALCONTOUR 2  LEGEND{XYPOS{X = 100}}
$!GLOBALCONTOUR 2  LEGEND{HEADERTEXTSHAPE{HEIGHT = 2.5}}
$!GLOBALCONTOUR 2  LEGEND{NUMBERTEXTSHAPE{HEIGHT = 2.5}}
$!GLOBALCONTOUR 2  LEGEND{BOX{BOXTYPE = FILLED}}
$!GLOBALCONTOUR 2  LEGEND{BOX{MARGIN = 5}}
$!GLOBALCONTOUR 2  LEGEND{ROWSPACING = 1.50000}

$!GLOBALCONTOUR 2  LABELS{NUMFORMAT{FORMATTING = EXPONENTIAL}}
$!GLOBALCONTOUR 2  LABELS{NUMFORMAT{PRECISION = 1}}
$!GLOBALCONTOUR 2  LABELS{AUTOLEVELSKIP = 1}

$!GLOBALCONTOUR 2  COLORMAPFILTER{COLORMAPDISTRIBUTION = BANDED}
$!GLOBALCONTOUR 2  COLORMAPNAME = \'Hot Metal\'';
    # -----------------------------------------------------------------
    print $fh '

#\
# Appearence parameters
#/

# don\'t show the box
$!INTERFACE ZONEBOUNDINGBOXMODE = OFF

$!FRAMELAYOUT BACKGROUNDCOLOR = CUSTOM2
$!FIELDLAYERS USELIGHTINGEFFECT = NO
$!FIELDLAYERS USETRANSLUCENCY = YES
$!FIELDMAP [1-4]  EFFECTS{SURFACETRANSLUCENCY = 3}

# trim lines according to Lagr ID
$!BLANKING VALUE{INCLUDE = YES}
$!BLANKING VALUE{CONSTRAINT 1 {INCLUDE = YES}}
$!BLANKING VALUE{CONSTRAINT 1 {VARA = 26}}
$!BLANKING VALUE{CONSTRAINT 1 {RELOP = GREATERTHAN}}
$!BLANKING VALUE{CONSTRAINT 1 {VALUECUTOFF = |LINETRIM|}}';
    # -----------------------------------------------------------------
    print $fh '

#\
# Display the time of simulation
#/
$!ATTACHTEXT
  ANCHORPOS
    {
    X = 30
    Y = 10
    }
  TEXTSHAPE
    {
    HEIGHT = 36
    }
  TEXT = \'|TIME|\'';
    # -----------------------------------------------------------------
    print $fh '

#\
# Insert a circle to cover Sun
#/
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = 0.000000
    Y =-0.000000
    }
  IsFilled = Yes
  FillColor = Black
  RawData
1.150000
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = 0.000000
    Y =-0.000000
    }
  LineThickness = 0.2
  IsFilled = Yes
  Color = Custom11
  FillColor = Custom11
  RawData
1.000000';
    # -----------------------------------------------------------------
    print $fh '

#\
# Save figures
#/

# Position and size of 3D axes
$!THREEDAXIS FRAMEAXIS{XYPOS{X = 82}}
$!THREEDAXIS FRAMEAXIS{XYPOS{Y = 15}}
$!THREEDAXIS FRAMEAXIS{SIZE = 10}';
    if($SaveCME){
	for(my $iView=1; $iView<=3; $iView++){
	    print $fh "
\$!IF |SAVE$iView| == 1

  \$!THREEDVIEW VIEWWIDTH = 35
  \$!THREEDVIEW VIEWERPOSITION{X = |VIEW${iView}x|}
  \$!THREEDVIEW VIEWERPOSITION{Y = |VIEW${iView}y|}
  \$!THREEDVIEW VIEWERPOSITION{Z = |VIEW${iView}z|}
  \$!THREEDVIEW PSIANGLE         = |VIEW${iView}p|
  \$!THREEDVIEW THETAANGLE       = |VIEW${iView}t|
  \$!THREEDVIEW ALPHAANGLE       =  0.00

  # plot CME and field lines
  \$!PRINTSETUP PALETTE = COLOR
  \$!EXPORTSETUP IMAGEWIDTH = 1355
  \$!EXPORTSETUP EXPORTFNAME = '|OUTDIR|/SC.SP.default.$iView.|TIMESTAMP|.png'
  \$!EXPORT
    EXPORTREGION = CURRENTFRAME

  # plot CME only
  \$!ACTIVEFIELDMAPS -= [|SPFIRST|-|SPLAST|]
  \$!THREEDVIEW VIEWWIDTH = 10

  \$!PRINTSETUP PALETTE = COLOR
  \$!EXPORTSETUP IMAGEWIDTH = 1355
  \$!EXPORTSETUP EXPORTFNAME = '|OUTDIR|/SC.default.$iView.|TIMESTAMP|.png'
  \$!EXPORT
    EXPORTREGION = CURRENTFRAME
\$!ENDIF

\$!ACTIVEFIELDMAPS += [|SPFIRST|-|SPLAST|]

#---------------------------------------------------------------------------
";
	}
	# -----------------------------------------------------------------
	print $fh '

#\
# Custom viewing positions
#/';
	for(my $c=0; $c < @CustomX;$c++){
	    print $fh "
\$!THREEDVIEW VIEWWIDTH = 30
\$!THREEDVIEW VIEWERPOSITION{X = $CustomX[$c]}
\$!THREEDVIEW VIEWERPOSITION{Y = $CustomY[$c]}
\$!THREEDVIEW VIEWERPOSITION{Z = $CustomZ[$c]}
\$!THREEDVIEW PSIANGLE         = $CustomP[$c]
\$!THREEDVIEW THETAANGLE       = $CustomT[$c]
\$!THREEDVIEW ALPHAANGLE       =  0.00

\$!PRINTSETUP PALETTE = COLOR
\$!EXPORTSETUP IMAGEWIDTH = 1355
\$!EXPORTSETUP EXPORTFNAME = '|OUTDIR|/SC.SP.custom.$c.|TIMESTAMP|.png'
\$!EXPORT
  EXPORTREGION = CURRENTFRAME

# plot CME only
\$!ACTIVEFIELDMAPS -= [|SPFIRST|-|SPLAST|]
\$!THREEDVIEW VIEWWIDTH = 10

\$!PRINTSETUP PALETTE = COLOR
\$!EXPORTSETUP IMAGEWIDTH = 1355
\$!EXPORTSETUP EXPORTFNAME = '|OUTDIR|/SC.custom.$c.|TIMESTAMP|.png'
\$!EXPORT
  EXPORTREGION = CURRENTFRAME

\$!ACTIVEFIELDMAPS += [|SPFIRST|-|SPLAST|]

#---------------------------------------------------------------------------
";
	}
    }
    # -----------------------------------------------------------------
    if($SaveOrbit){
	print $fh '
#\
# show contour for field lines
#/
$!FIELDMAP [1-4]  CONTOUR{SHOW = NO}
$!FIELDMAP [|SPFIRST|-|SPLAST|]  CONTOUR{SHOW = YES}
$!FIELDMAP [|SPFIRST|-|SPLAST|]  MESH{COLOR = MULTI2}
$!FIELDMAP [|SPFIRST|-|SPLAST|]  MESH{LINETHICKNESS = 0.2}


#\
# set viewing position for orbit plot
#/
$!THREEDVIEW VIEWWIDTH = 750
$!THREEDVIEW VIEWERPOSITION{X = |ViewOrbitX|}
$!THREEDVIEW VIEWERPOSITION{Y = |ViewOrbitY|}
$!THREEDVIEW VIEWERPOSITION{Z = |ViewOrbitZ|}
$!THREEDVIEW PSIANGLE         = |PsiOrbit|
$!THREEDVIEW THETAANGLE       = |ThetaOrbit|
$!THREEDVIEW ALPHAANGLE       = |AlphaOrbit|

# plot circle for orbit
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = 0.000000
    Y =-0.000000
    }
  LineThickness = 0.2
  IsFilled = No
  Color = Black
  RawData
215.000000

# plot circle for Earth
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = |XEarth|
    Y = |YEarth|
    }
  LineThickness = 0.2
  IsFilled = Yes
  Color = Green
  FillColor = Green
  RawData
5.000000

# label Earth
$!AttachText 
  PositionCoordSys = Grid
  AnchorPos
    {
    X = |XEarth|
    Y = (|YEarth| + 15)
    }
  TextShape
    {
    IsBold = Yes
    Height = 20
    }

  Anchor = Center
  Text = \'Earth\'

# plot circle for STEREO A
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = |XStA|
    Y = |YStA|
    }
  LineThickness = 0.2
  IsFilled = Yes
  Color = Red
  FillColor = Red
  RawData
5.000000

# label STEREO A
$!AttachText 
  PositionCoordSys = Grid
  AnchorPos
    {
    X = |XStA|
    Y = (|YStA| - 15)
    }
  TextShape
    {
    IsBold = Yes
    Height = 20
    }
  Anchor = HeadCenter
  Text = \'STEREO A\'

# plot circle for STEREO B
$!AttachGeom
  GeomType = Circle
  AnchorPos
    {
    X = |XStB|
    Y = |YStB|
    }
  LineThickness = 0.2
  IsFilled = Yes
  Color = Blue
  FillColor = Blue
  RawData
5.000000

# label STEREO B
$!AttachText 
  PositionCoordSys = Grid
  AnchorPos
    {
    X = |XStB|
    Y = (|YStB| - 15)
    }
  TextShape
    {
    IsBold = Yes
    Height = 20
    }
  Anchor = HeadCenter
  Text = \'STEREO B\'

# export plot
  $!PRINTSETUP PALETTE = COLOR
  $!EXPORTSETUP IMAGEWIDTH = 1355
  $!EXPORTSETUP EXPORTFNAME = \'|OUTDIR|/SP.orbit.|TIME|.png\'
  $!EXPORT 
    EXPORTREGION = CURRENTFRAME
';
    }
    # -----------------------------------------------------------------
    print $fh '

#\
# Remove previosuly defined variables
#/
$!RemoveVar |SCDIR|
$!RemoveVar |SPDIR|
$!RemoveVar |OUTDIR|
$!RemoveVar |N1x|
$!RemoveVar |N1y|
$!RemoveVar |N1z|
$!RemoveVar |N2x|
$!RemoveVar |N2y|
$!RemoveVar |N2z|
$!RemoveVar |N3x|
$!RemoveVar |N3y|
$!RemoveVar |N3z|';

    close($fh);
}

#------------------------------------------------------------------------

# print orbit parameters to file
sub print_orbit_param{#args: $fh

    # file handler for macro being created
    my $fh = $_[0];
    
    # compute viewing position for orbit plot
    
    # interpolation weight between starting and final positions
    # NOTE: not precise, but worls well on short time scales
    my $w = 1;#$t/(@Time-1);

    # longitudes of Earth and STEREOs
    my $Lon0 = $LonEarth[0];
    my $LonA = $LonStA[0]  ;
    my $LonB = $LonStB[0]  ;

    # latitudes of Earth and STEREOs
    my $Lat0 = $LatEarth[0];
    my $LatA = $LatStA[0]  ;
    my $LatB = $LatStB[0]  ;

    # unit radius vectors in directions of Earth and STEREOs
    my @REarth = (cos($Lat0*$rad)*cos($Lon0*$rad),
		  cos($Lat0*$rad)*sin($Lon0*$rad),
		  sin($Lat0*$rad));
    my @RStA   = (cos($LatA*$rad)*cos($LonA*$rad),
		  cos($LatA*$rad)*sin($LonA*$rad),
		  sin($LatA*$rad));
    my @RStB   = (cos($LatB*$rad)*cos($LonB*$rad),
		  cos($LatB*$rad)*sin($LonB*$rad),
		  sin($LatB*$rad));
    # normal vector to the plane of Earth and STEREOS (i.e. orbit plane)
    my @Dir    = ($RStB[1]*$RStA[2]-$RStB[2]*$RStA[1],
		  $RStB[2]*$RStA[0]-$RStB[0]*$RStA[2],
		  $RStB[0]*$RStA[1]-$RStB[1]*$RStA[0]);
    my $Norm = sqrt($Dir[0]*$Dir[0]+$Dir[1]*$Dir[1]+$Dir[2]*$Dir[2]);
    foreach my $x (@Dir) { $x /= $Norm; }

    # compute viewing position and angles
    my $PsiOrbit   = 90 - $deg*asin($Dir[2]);
    my $ThetaOrbit =270 -$deg*asin($Dir[1]/sin($PsiOrbit*$rad));
    my $AlphaOrbit = $deg*acos(
	-($REarth[0]*cos($PsiOrbit*$rad)*sin($ThetaOrbit*$rad)+
	  $REarth[1]*cos($PsiOrbit*$rad)*cos($ThetaOrbit*$rad)+
	  $REarth[2]*sin($PsiOrbit*$rad)));
    my $OrbitX = 1000*$Dir[0];
    my $OrbitY = 1000*$Dir[1];
    my $OrbitZ = 1000*$Dir[2];


    printf $fh "

#\\
# parameters of orbit plot
#/
\$!VarSet |LonEarth|   = $Lon0
\$!VarSet |LonStA|     = $LonA
\$!VarSet |LonStB|     = $LonB
\$!VarSet |LatEarth|   = $Lat0
\$!VarSet |LatStA|     = $LatA
\$!VarSet |LatStB|     = $LatB
\$!VarSet |PsiOrbit|   = $PsiOrbit
\$!VarSet |ViewOrbitX| = $OrbitX
\$!VarSet |ViewOrbitY| = $OrbitY
\$!VarSet |ViewOrbitZ| = $OrbitZ
\$!VarSet |ThetaOrbit| = $ThetaOrbit
\$!VarSet |AlphaOrbit| = $AlphaOrbit";

    
    # 2D coordinates of positions of Earth and STEREOs on the figure;
    # Earth is fixed at the bottom position
    my $x; my $y;
    $y =-215;
    $x =   0;
    printf $fh "

# Earth position on the figure
\$!VarSet |XEarth| = $x
\$!VarSet |YEarth| = $y";

    $y =-215 * cos($rad*($LonA-$Lon0));
    $x = 215 * sin($rad*($LonA-$Lon0));
    printf $fh "

# STEREO A position on the figure
\$!VarSet |XStA| = $x
\$!VarSet |YStA| = $y";
	
    $y =-215 * cos($rad*($LonB-$Lon0));
    $x = 215 * sin($rad*($LonB-$Lon0));
    printf $fh "

# STEREO B position on the figure
\$!VarSet |XStB| = $x
\$!VarSet |YStB| = $y";

}
