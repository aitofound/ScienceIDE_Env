#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

use strict;
use warnings;
use List::Util "max";


my $DESCRIPTION = 
  "Script converts ASCII data files with observational data as fetched from\n".
  "public web resources to files that can be imported to Tecplot\n";
my $SOURCES = 
  "List of web resources:\n".
  " GOES\t\thttps://www.ngdc.noaa.gov/stp/satellite/goes/dataaccess.html\n".
  " SOHO/EPHIN\thttp://www2.physik.uni-kiel.de/SOHO/phpeph/EPHIN.htm\n".
  " STEREO/LET\thttp://www.srl.caltech.edu/STEREO/Public/LET_public.html\n".
  " STEREO/HET\thttp://www.srl.caltech.edu/STEREO/Public/HET_public.html\n";
my $USAGE = 
    "Usage:\n".
    " ./ProcessData.pl [-help] [-silent] [-sources] [-fmt[=FORMAT_NAME]] [FILENAME]\n\n".
    "If called without arguments, an error message is displayed\n\n".
    "Options:\n\n".
    " -h -help\tdisplay help message\n".
    " -silent\tsuppress standard output\n".
    " -sources\tdisplay data sources\n".
    " -fmt\t\tuse/show data format(s)\n\n".
    "Examples:\n\n".
    "Convert data file (one at a time) of indicated format:\n".
    " ./ProcessData.pl -fmt=format_name filename\n\n".
    "Display help:\n".
    " ./ProcessData.pl -help\n\n".
    "Display formats:\n".
    " ./ProcessData.pl -fmt\n\n".
    "Display data sources:\n".
    " ./ProcessData.pl -sources\n";

my $FORMATS = 
"List of implemented formats with names accepted by the script:\n".
" GOES\t\tgoes\n SOHO/EPHIN\tsoho-ephin\n".
" STEREO/LET\tstereo-let\n STEREO/HET\tstereo-het\n";
   
# the script process only one file at a time;
# if there is no argument -> die
my $nArg = @ARGV+0;
die "ERROR: No input file provided\n".('-'x 80)."\n$USAGE" unless ($nArg>0);

# format name holder
my $fmt = '';

# datafile name holder
my $filename = '';

# whether to suppress standard output
my $IsSilent = '';

# intergral flux cut-off levels in MeV
my @FluxChannels = (5, 10, 30, 50, 60, 100);

# storage for variables indices corresponding to flux channels
my %FluxVars;
@FluxVars{@FluxChannels} = ();

# energy ranges for fluxes
my %FluxRange;
@FluxRange{@FluxChannels} = ();

my %FluxMin;
@FluxMin{@FluxChannels} = ();

my %FluxMax;
@FluxMax{@FluxChannels} = ();


# print help if required
foreach my $Arg (@ARGV){
    if($Arg =~ m/^(-h|-help)$/){
	print "Displaying help message; other arguments are ignored\n".
	    ('-'x 80)."\n"
	    if($nArg > 1);
	print "$DESCRIPTION\n$USAGE\n$FORMATS\n$SOURCES";
	exit;
    }
    elsif($Arg =~ m/^-silent$/){
	$IsSilent = 1;
    }
    elsif($Arg =~ m/^-sources$/){
	print "Displaying data sources; other arguments are ignored\n".
	    ('-'x 80)."\n"
	    if($nArg > 1);
	print "$SOURCES";
	exit;
    }
    elsif($Arg =~ m/^-fmt$/){
	print "Displaying formats; other arguments are ignored\n".
	    ('-'x 80)."\n"
	    if($nArg > 1);
	print "$FORMATS";
	exit;
    }
    elsif($Arg =~ m/^-fmt=(.*)$/){
	die "ERROR: Trying to redefine format name\n".('-'x 80).
	    "\n$USAGE" if($fmt);
	$fmt = $1;
        # check that the format is valid
	die "ERROR: Unknown data format '$fmt'\n".('-'x 80)."\n$FORMATS"
	    unless($fmt=~ m/^(soho-ephin|goes|stereo-let|stereo-het)$/);
    }
    elsif($Arg =~ m/(.*)/){
	die "ERROR: Too many arguments\n".('-'x 80)."\n$USAGE" if($filename);
	$filename = $1;
	die "ERROR: File '$filename' not found\n".('-'x 80)."\n$USAGE"
	    unless(-e $filename);
    }
    else {
	die "ERROR: Unknown option $Arg\n".('-'x 80)."\n$USAGE";
    }
}

# check that the filename has been provided
die "ERROR: No input file provided\n".('-'x 80)."\n$USAGE" unless($filename);

# check that the file format has been provided
die "ERROR: No format provided\n".('-'x 80)."\n$USAGE" unless($fmt);

# read the files contents
my $fh;
my @linesIn = ();
open($fh,'<',$filename);
@linesIn = <$fh>;
close($fh);

# prepare output
my @linesOut = ();

# some important information is contained in the header; parse it
my $FoundData  = '';
my $DoneHeader = '';
my $VarNames   = '';
my $Time0='';
my $Time;
my $nEntry=1;


my %FormatTitle = (
    'goes'       => 'GOES',
    'soho-ephin' => 'SOHO/EPHIN',
    'stereo-let' => 'STEREO/LET',
    'stereo-het' => 'STEREO/HET'
);

# marker of the line that precedes data
my %DataMarker = (
    'goes'       => 'time_tag,',
    'soho-ephin' => 'Julian date\s+',
    'stereo-let' => 'BEGIN DATA',
    'stereo-het' => '#End'
);

# marker of the line that contains (some) header info
my %HeaderMarker = (
    'goes'       => 'time_tag,',
    'soho-ephin' => 'Julian date',
    'stereo-let' => 'Column\s+\d+:',
    'stereo-het' => '[TF]\d'
);

# hash with month names
my %Month = (
    "Jan" => 1, "Feb" => 2, "Mar" => 3,
    "Apr" => 4, "May" => 5, "Jun" => 6,
    "Jul" => 7, "Aug" => 8, "Sep" => 9,
    "Oct" =>10, "Nov" =>11, "Dec" =>12);

foreach my $line (@linesIn){
    # Process header lines
    if($line =~ m/^$HeaderMarker{$fmt}/){
	if($fmt eq 'goes'){
	    $VarNames = $line;
	    $VarNames =~ s/,/","/g;
	    $VarNames =~ s/\R//g;
	    $VarNames =~ s/time_tag/Julian date/;

	    # determine correspondence between channels and variables in data
	    foreach my $C (@FluxChannels) {
		if($C < 8.7){
		    push @{$FluxVars{$C}}, i_var('P2W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 4.2; 
		    push @{$FluxMax{$C}}, 8.7;
		}
		if($C < 14.5){
		    push @{$FluxVars{$C}}, i_var('P3W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 8.7; 
		    push @{$FluxMax{$C}}, 14.5;
		}
		if($C < 40){
		    push @{$FluxVars{$C}}, i_var('P4W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 15; 
		    push @{$FluxMax{$C}}, 40;

		}
		if($C < 82){
		    push @{$FluxVars{$C}}, i_var('P5W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 38; 
		    push @{$FluxMax{$C}}, 82;
		}
		if($C < 200){
		    push @{$FluxVars{$C}}, i_var('P6W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 84; 
		    push @{$FluxMax{$C}}, 200;
		}
		if($C < 900){
		    push @{$FluxVars{$C}}, i_var('P7W_UNCOR_FLUX', $VarNames);
		    push @{$FluxMin{$C}}, 110; 
		    push @{$FluxMax{$C}}, 900;
		}
	    }
	}
	elsif($fmt eq 'soho-ephin'){
	    $VarNames = $line;
	    $VarNames =~ s/\s{3,}/","/g;
	    $VarNames =~ s/\R//g;

	    # determine correspondence between channels and variables in data
	    # description of channel energy levels 
	    # http://www.ieap.uni-kiel.de/et/ag-heber/costep/ephin_sensor.php
	    foreach my $C (@FluxChannels) {
		if($C < 7.8){
		    push @{$FluxVars{$C}}, i_var('P4', $VarNames);
		    push @{$FluxMin{$C}}, 4.3; 
		    push @{$FluxMax{$C}}, 7.8;
		}
		if($C < 25.0){
		    push @{$FluxVars{$C}}, i_var('P8', $VarNames);
		    push @{$FluxMin{$C}}, 7.8; 
		    push @{$FluxMax{$C}}, 25.0;
		}
		if($C < 40.9){
		    push @{$FluxVars{$C}}, i_var('P25', $VarNames);
		    push @{$FluxMin{$C}}, 25.0; 
		    push @{$FluxMax{$C}}, 40.9;
		}
		if($C < 53.0){
		    push @{$FluxVars{$C}}, i_var('P41', $VarNames);
		    push @{$FluxMin{$C}}, 40.9; 
		    push @{$FluxMax{$C}}, 53.0;
		}
	    }
	}
	elsif($fmt eq 'stereo-let'){
	    # regex pattern for a decimal number
	    my $Number = '[0-9]*\.?[0-9]+';

	    $VarNames = '"Julian date"' unless($VarNames);
            # header line format Column N: VarName
	    if($line =~ m/^$HeaderMarker{$fmt}\s+(($Number)\s+-\s+($Number).*)$/){
		$VarNames.=",\"$1\"";
		$nEntry++;

		foreach my $C (@FluxChannels){
		    if($C < $3){
			push @{$FluxVars{$C}}, i_var("$1", $VarNames);
			push @{$FluxMin{$C}}, $2; 
			push @{$FluxMax{$C}}, $3;
		    }
		}
	    }
	}
	elsif($fmt eq 'stereo-het'){
	    # regex pattern for a decimal number
	    my $Number = '[0-9]*\.?[0-9]+';
            # header line format:
	    # Tx: time string, x=1 for single time string, 
	    #                  x=2 for strings for both begin and end
	    #                      of collecting (averaging) period
	    # Fx: float data,  x=1 for data value only,
	    #                  x=2 for data value and its uncertainty
	    if($line =~ m/\s*T(.)/){
		if   ($1 == 1){
		    $VarNames = '"Julian date"';
		}
		elsif($1 == 2){
		    $VarNames = '"Julian date","Julian date end"';
		}
		else{
		    die "Don't know how to process line:\n\t'$line'\n"
		}
	    }
	    if($line =~ m/^\s*F(.).*\s+($Number)\s+($Number)\s+(.)\s*$/){
		if   ($1 == 1){
		    $VarNames.= ",\"Intensity $4 $2-$3 MeV\"";
		}
		elsif($1 == 2){
		    $VarNames.= ",\"Intensity $4 $2-$3 MeV\",\"Sigma $4 $2-$3 MeV\"";
		}
		else {
		    die "Don't know how to process line:\n\t'$line'\n"
		}
		# determine correspondence between channels and variables
		if($4 eq'H') {
		    foreach my $C (@FluxChannels){
			if($C < $3){
			    push @{$FluxVars{$C}}, i_var("Intensity H $2-$3 MeV", $VarNames);
			    push @{$FluxMin{$C}}, $2; 
			    push @{$FluxMax{$C}}, $3;
			}
		    }
		}
	    }
	}
    }

    # check whether reached data location
    if($line =~ m/^$DataMarker{$fmt}/) {
	$FoundData = 1;
	$VarNames = '"'.$VarNames.'"'unless($VarNames =~ m/^".*"$/);
	next;
    }

    next unless($FoundData);
    
    #\
    # Process data lines --------------------------------------------------
    #/
    if($fmt eq 'goes') {
	# process time_tag:
	# convert dates to Julian dates (in days);
	# save the beginning of the period in the file as auxilary data,
	# save the dates as amount of julian days since this date
	$line =~ m/\s*(\d{4})-(\d{2})-(\d{2}) (\d{2}):(\d{2}):(\d{2})/;
	die "Failed to process time_tag at line:\n$line\n"
	    unless($1 && $2 && $3 && $4 && $5 && $6);
	
	$Time0 = get_julian_date($1,$2,$3,$4,$5,$6) unless ($Time0);
	$Time  = get_julian_date($1,$2,$3,$4,$5,$6) - $Time0;
	# substitute time_tag for time in seconds
	$line =~ s/^\s*.*?,/$Time,/;
	
        # now data are separated by commas, change for spaces
	$line =~ s/,/ /g;
    }
    elsif($fmt eq 'soho-ephin'){
    	# process time string:
	# dates  are given in Julian dates (in days);
	# save the beginning of the period in the file as auxilary data,
	# save the dates as amount of julian days since this date
	$line =~ m/\s*([0-9]*\.?[0-9]+)/;
	$Time0= $1 unless($Time0);
	$Time = $1-$Time0;
	$line =~ s/\s*[0-9]*\.?[0-9]+/$Time/;
    }
    elsif($fmt eq 'stereo-let'){
	# convert dates to Julian dates (in days);
	# 1st entry is yearm 2nd entry is DOY (fractional)
	# save the beginning of the period in the file as auxilary data,
	# save the dates as amount of julian days since this date
	my $TimePattern = '\d{4} \w{3}\s{1,2}\d{1,2} \d{2}\d{2}';
	my $TimeGrouped = '(\d{4}) (\w{3})\s{1,2}(\d{1,2}) (\d{2})(\d{2})';
	
	$line =~ m/^\s*(\d{4})\s+(\d+\.\d+)/;
	die "Failed to process time tag at line:\n$line\n"
	    unless($1 && $2);
	
	$Time0= get_julian_date($1, 1, 1, 0, 0, 0.0) - 1.0 + $2 unless($Time0);
	$Time = get_julian_date($1, 1, 1, 0, 0, 0.0) - 1.0 + $2 - $Time0;
	$line = "$Time $line";
	
	# remove trailing data
	$line =~ s/^(\s*(\S+\s+){$nEntry}).*/$1/;
	
	# substitute missing/bad data with zeros
	$line =~ s/-9999\.9/0\.0/g;
	$line =~ s/-9\.9999/0\.0/g;
	$line =~ s/-9999/0/g;
	
    }
    elsif($fmt eq 'stereo-het'){
    	# lines start with 0, remove
	$line =~ s/^0\s*//;
	
	# process time string:
	# convert dates to Julian dates (in days);
	# save the beginning of the period in the file as auxilary data,
	# save the dates as amount of julian days since this date
	my $TimePattern = '\d{4} \w{3}\s{1,2}\d{1,2} \d{2}\d{2}';
	my $TimeGrouped = '(\d{4}) (\w{3})\s{1,2}(\d{1,2}) (\d{2})(\d{2})';

	$line =~ m/$TimeGrouped/;
	die "Failed to process time tag at line:\n$line\n"
	    unless($1 && $2 && $3 && $4 && $5);
	$Time0= get_julian_date($1, $Month{$2}, $3, $4, $5, 0.0)unless($Time0);
	$Time = get_julian_date($1, $Month{$2}, $3, $4, $5, 0.0) - $Time0;
	$line =~ s/^\s*$TimePattern/$Time/;
	
	# process the second time string (if present)
	if($line =~ m/$TimeGrouped/){
	    die "Failed to process 2nd time tag at line:\n$line\n"
		unless($1 && $2 && $3 && $4 && $5);
	    $Time = get_julian_date($1, $Month{$2}, $3, $4, $5, 0.0) - $Time0;
	    $line =~ s/$TimePattern/$Time/;
	}
    }

    $line =~ s/\R//g;
    $line = "$line\n";

    push @linesOut, $line;
}

#\
# compute integral fluxes in requested channels
#/
foreach my $C (@FluxChannels){

    next unless($FluxMax{$C});

    # append name to the list of variables
    $VarNames .= ',"flux_'.sprintf("%05d",$C).'_[p.f.u.]"';
    
    # account for overlapping flux channels in data
    # NOTE: maxima and minima are assumed to be sorted
    for (my $i=0; $i < @{$FluxMax{$C}} - 1; $i++){
	if($FluxMax{$C}[$i] > $FluxMin{$C}[$i+1]){
	    $FluxMax{$C}[$i]   = 0.5*($FluxMax{$C}[$i]+$FluxMin{$C}[$i+1]);
	    $FluxMin{$C}[$i+1] = $FluxMax{$C}[$i];
	}
    }
    
    # compute the energy ranges corresponding to flux variables
    for (my $i=0; $i < @{$FluxMax{$C}}; $i++){
	# current channel serves as the lower intergration limit
	$FluxMin{$C}[$i] = max($FluxMin{$C}[$i], $C);
	# compute the final range
	push @{$FluxRange{$C}}, ($FluxMax{$C}[$i] - $FluxMin{$C}[$i]);
    }
}

# using the correspondence determined above, compute fluxes
foreach my $line (@linesOut){
    foreach my $C (@FluxChannels){
	
	next unless($FluxRange{$C});
	
	my $Flux = 0;
	for(my $i=0; $i < @{$FluxVars{$C}}; $i++){
	    $Flux += get_i_var($FluxVars{$C}[$i], $line) * $FluxRange{$C}[$i];
	}
	# append the result to the end
	$line =~ s/(.*)/$1 $Flux/;
    }
}


# count and save number of rows
my $nRow = @linesOut;

# check for failure to find data
die "ERROR:\tFailed to process file '$filename',\n".
    "\tformat '$fmt' may not be appropriate\n".('-'x80).
    "\n$FORMATS"
    unless($nRow);

# print to output file
my $oldname = $filename;
$filename =~ s/dat$/tec.dat/;
$filename =~ s/(txt|csv)$/dat/;
$filename.=  '.dat' unless($filename =~ m/dat$/);

open($fh,'>',$filename);
print $fh "TITLE=\"$FormatTitle{$fmt} data\"\n";
print $fh "VARIABLES = $VarNames\n".
    "ZONE T=\"STRUCTURED GRID\", I= $nRow, J= 1, K= 1, F=POINT\n".
    "AUXDATA StartTimeJulian=\"$Time0\"\n";
print $fh @linesOut;
close($fh);

print "Converted $FormatTitle{$fmt}:\n $oldname\n\ ->\n $filename\n"
    unless($IsSilent);

#---------------------------------------------------------------------

sub get_julian_date{ 
    # Arguments: year, month, day, hour, minunte, second
    # valid ONLY for dates after March 1, 1900 to 2099
    my $Y = $_[0];
    my $M = $_[1];
    my $D = $_[2];
    my $h = $_[3];
    my $m = $_[4];
    my $s = $_[5];
    return 367*$Y - int(7 * ( $Y + int( ($M + 9) / 12 ) ) / 4) + 
	int( (275 * $M) / 9 ) + $D + 1721013.5 + 
	($h + $m/60 + $s/3600)/24;
}

#---------------------------------------------------------------------
sub i_var{# args: $Var, $Vars
    # variables in the list MUST be comma separated
    # and each may be in double quotes, 
    # e.g. $Vars = '"Var1" "Var2" "Var3"'

    # result
    my $res = -1;

    # returns a zero-based index of $Var in the list $Vars
    my $Var = $_[0];
    my $Vars= $_[1];
    if($Vars =~ m/(([^,]*\s*,\s*)*)"?\Q$Var\E"?\s*(,|$)/){
	# list of variables preceding $Var
	my $Aux = $1;
	$res = ~~($Aux =~ s/,/,/g);
    }
    return $res;
}

#---------------------------------------------------------------------
sub get_i_var{# args: $iVar, $Vars
    # returns variable with zero-based index in the string $Vars
    die "Failed to get a variable value from string $_[1]"
	unless($_[1] =~ m/([0-9.eE+-]+\s+){$_[0]}([0-9.eE+-]+)/);
    return $2;
}
		
