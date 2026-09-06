#!/usr/bin/perl -s

# Script to process multiple simulations and extract the time series
# of magnetic perturbation at a number of magnetometer stations
# using the INTERPOLATE.exe code.
# The input mag_grid*.out.tar.xz files are untarred and concanated 
# into mag_grid*.outs files.
# The output XYZ.txt files are tarred up and compressed with xz.

use strict;

my @event = glob("201*");

print "Number of events: ",scalar @event,"\n";

my $event;
foreach $event (@event){
    my $eventdir = "stations/$event";
    next if -f "$eventdir/ABK.txt";
    `mkdir -p $eventdir` unless -d $eventdir;
    
    my $outfile;
    my @outfile = glob("$eventdir/mag_grid*.outs");
    if(@outfile){
	$outfile = @outfile[0];
    }else{
	my @tarfile = glob("$event/run/GM/IO2/mag_grid*.tar.xz");
	my $ntarfile = scalar @tarfile;
	if($ntarfile != 1){
	    print "number of tar files in $event: $ntarfile\n";
	    next;
	}
	my $tarfile = @tarfile[0];
	my $outfile = $tarfile;
	$outfile =~ s/.*(mag_grid)/$eventdir\/$1/; 
	$outfile =~ s/\.tar\.xz$/s/;
	my $command = "tar -xOf $tarfile > $outfile";
	print "$command\n";
	`$command`;
    }
    my $paramfile = "$eventdir/INTERPOLATE.in";
    open PARAM, ">$paramfile" or die "could not open $paramfile\n";
    print PARAM "$outfile\nreal4\nstations/supermag.dat\n$eventdir/";
    close PARAM;
    my $command = "stations/INTERPOLATE.exe < $paramfile; " 
	. "cd stations; tar -cJf $event.tar.xz $event/*.txt";
    print "$command\n";
    `$command`;
}

exit 0;
