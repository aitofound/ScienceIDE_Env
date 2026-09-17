#!/usr/bin/perl

use strict;

#parse the command line: -f=[the file name pattern] -np=[the number of processes that was used to create the output files] 
my ($fname,$nthreads,$out);


foreach (@ARGV) {
  if (/^-f=(.*)/i) {
    $fname=$1;
  }

  if (/^-np=(.*)/i) {
    $nthreads=$1;
  }

  if (/^-h/i) {
    print "AssembleDistributedOutputFile.pl -f=[the file name pattern] -np=[the number of processes that was used to create the output files] \n";
    print "Example: to process the set of files line 'pic.H2O.s=0.out=1.dat.data.thread=0.tmp' the following command would be used\n";
    print "ConOutput.pl -fname=pic.H2O.s=0.out=1.dat\n";
    exit;
  }

  my ($line,$n,$f); 

  open (fOut,">",$fname);

  open (fIn,"<",$fname.".thread=0.variables");
  while ($line=<fIn>) {
    print fOut  "$line";
  }
  close fIn;


  for ($n=0;$n<$nthreads;$n++) {
    #Header
    $f=$fname.".thread=".$n.".header";

    print "$f\n";
    open (fIn,"<",$f);

    while ($line=<fIn>) {
      print fOut  "$line";
    }

    close fIn;

    #Data
    $f=$fname.".thread=".$n.".data";
   
    print "$f\n";
    open (fIn,"<",$f);

    while ($line=<fIn>) {
      print fOut  "$line";
    }

    close fIn; 

    #Connectivity list
    $f=$fname.".thread=".$n.".connectivity";
 
    print "$f\n";
    open (fIn,"<",$f);

    while ($line=<fIn>) {
      print fOut  "$line";
    }

    close fIn; 
  }

  close fOut;
}



