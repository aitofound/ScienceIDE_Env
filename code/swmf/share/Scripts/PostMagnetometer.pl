#!/usr/bin/perl -s
#  Copyright (C) 2002 Regents of the University of Michigan, portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

my $help = ($h or $H or $help);

use strict;

@ARGV = glob("mag*.mag") unless @ARGV;

$help = 1 if not @ARGV; 

&print_help if $help;

my $ERROR = "ERROR in PostMangetometer.pl";

my @stations;  # array of station names (all capitals)
my %data;      # hash of magnetometer data indexed by station and date
my %read;      # hash to check which file types have been read
my %check;     # hash to check multiple occurance of dates in one type

# list of variables in the output files
my $variablesout  = "Year Month Day Hour Min Sec B_NorthGeomag B_EastGeomag B_DownGeomag";

my $variablesgm   = " dBn dBe dBd dBnMhd dBeMhd dBdMhd dBnFac dBeFac dBdFac dBnHal dBeHal dBdHal dBnPed dBePed dBdPed";

my $file; 
my $type;
foreach $file (sort @ARGV){
    print "reading $file\n";
    open FILE, $file or die "$ERROR: could not open file $file\n";
    &process_swmf_file;
    close FILE;
}

&write_output;

exit 0;

##########################################################################
sub process_swmf_file{

# GM header
#       12 magnetometers:  YKC MEA NEW FRN IQA PBQ OTT FRD HRN ABK WNG FUR
#nstep year mo dy hr mn sc msc station X Y Z dBn dBe dBd facdBn facdBe facdBd
#
# IE header
#   12 magnetometers:  YKC MEA NEW FRN IQA PBQ OTT FRD HRN ABK WNG FUR
#nsolve year mo dy hr mn sc msc station X Y Z JhdBn JhdBe JhdBd JpBn JpBe JpBd

    # Read header and extract station names
    my $header = <FILE>;

    $header =~ /magnetometers:/ 
	or die "$ERROR for $file: invalid header=$header\n";

    my $stations = $';
    @stations = split(' ',$stations);

    my $variables = <FILE>;
    die "$ERROR for $file: invalid variables=$variables\n" unless
	$variables =~ /nstep year mo dy hr mn sc msc station X Y Z$variablesgm/;

    # Read data
    while(<FILE>){
	# split line into columns
	my @items = split(' ',$_);

	#die "$ERROR in $file at line $.: incorrect number of items $#items in $_"
	#    unless $#items == 26;

	# Extract date and station number/name
        $items[6] =~ s/\d$/0/;  # replace second digit of seconds with a 0
	my $date = join(' ',@items[1..6]);
	my $imag = $items[8]-1;
	my $station = $stations[$imag];

	# Only record the first occurance
	next if $check{$station}{$date}++;

	# Store magnetic field
	$data{$station}{$date} .= ' ' . join(' ',@items[12..14]);

	# There is some data for this station
	$read{$station} = 1;
    }
    #print "read stations=",%read,"\n";
}
#############################################################################
sub write_output{

    # write out perturbations for each station separately
    # calculate total (GM+IE) perturbation
    # calculate RMS error and correlation with
    # respect to data if present

    my $station;
    foreach $station (sort @stations){

	my $variables = $variablesout;

	if(not $read{$station}){
	    $variables =~ s/$variablesgm//;
	    print "No data for station $station\n";
	    next;
	}

	# Construct header line
	my $header = "# SWMF run: SWMF_SWPC
# created by PostMagnetometer.pl
# North, East and vertical components of magnetic field in nT
# computed from magnetosphere and ionosphere currents
# Station: $station";

	# Process data and collect output to be written into file
	my $output; # string storing output

	my $date;
	foreach $date (sort keys %{ $data{$station} }){
	    my $data = $data{$station}{$date};
	    my @date = split(' ',$date);
	    my @data = split(' ',$data);

	    # Print date, add a "msc" column with 0s, sums
	    $output .= sprintf("%4d%5d%5d%5d%5d%5d%13.3f%13.3f%13.3f\n", @date, @data);

	}

	my $fileout = "$station.txt";
	print "writing $fileout\n";

	open FILE, ">$fileout";

	print FILE "$header\n";
	print FILE "$variables\n";
	print FILE $output;

	close FILE;
    }

}
##############################################################################
sub print_help{
    
    print "
Purpose: combine GM, IE and measured magnetic perturbations. Calculate errors.
The input files are parsed and the output is split into separate files for
each magnetometer. Only data points that occur in all 3 files are used. 

Usage:

PostMagnetometer.pl GM*.mag IE*.mag *.final

";
    exit 0;
}
