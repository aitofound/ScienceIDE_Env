#!/usr/bin/perl

push @INC, ".";
use strict;

#parse the command line: -fname=[the file name pattern] 
my ($fname,$nthreads,$out);


foreach (@ARGV) {
  if (/^-fname=(.*)/i) {
    $fname=$1;
  }
 
  if (/^-h/i) {
    print "ConOutput.pl -fname=[the file name pattern] \n";
    print "Example: to process the set of files line 'pic.H2O.s=0.out=1.dat.data.tread=0.tmp' the following command would be used\n";
    print "ConOutput.pl -fname=pic.H2O.s=0.out=1.dat\n"; 
    exit;
  }
}


#read the number of cells and corners in the resulted output file
my @mesh_size_files;
my @mesh_data_files;
my @mesh_connectivity_files;
my @corners_number;
my @cells_number;
my $line;

opendir(DIR, "."); 
my @all_files=sort(readdir(DIR));
my $file;

foreach $file (@all_files) {
  if ($file =~ m/$fname\.mesh-size.*\.tmp/) {
    print "file=$file\n";

    push @mesh_size_files, $file
  }

  if ($file =~ m/$fname\.data.*\.tmp/) {
    print "file=$file\n";

    push @mesh_data_files, $file
  }

  if ($file =~ m/$fname\.connectivity.*\.tmp/) {
    print "file=$file\n";

    push @mesh_connectivity_files, $file
  }
}

my $nTotalCells=0;
my $nTotalCorners=0;

for (my $i=0;$i<scalar @mesh_size_files;$i++) {
  open (fIn,"<",$mesh_size_files[$i]); 

  $line=<fIn>;
  $line=~s/Corners=//;
  push @corners_number,$line;
  $nTotalCorners+=$line;

  $line=<fIn>;
  $line=~s/Cells=//g; 
  push @cells_number,$line;
  $nTotalCells+=$line;

  close fIn;
}

#create the resulting output file and copy the header
open (fOut,">","$fname\.all\.dat");

open (fIn,"<","$fname.\header.tmp");
$line=<fIn>;
print fOut  "$line\n";
print fOut "ZONE N=$nTotalCorners, E=$nTotalCells, DATAPACKING=POINT, ZONETYPE=FEBRICK\n";
close fIn;



#copy the data content 
for (my $i=0;$i<scalar @mesh_data_files;$i++) {
  open (fIn,"<",$mesh_data_files[$i]);

  while ($line=<fIn>) {
    print fOut  "$line";
  }

  close fIn; 
}

#combine the connectivity list  
my $offset=0;
my ($s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7);

for (my $i=0;$i<scalar @mesh_connectivity_files;$i++) {
  open (fIn,"<",$mesh_connectivity_files[$i]);

  while ($line=<fIn>) {
    ($s0,$s1,$s2,$s3,$s4,$s5,$s6,$s7)=split(' ',$line);

    $s0+=1+$offset;
    $s1+=1+$offset;
    $s2+=1+$offset;
    $s3+=1+$offset; 
    $s4+=1+$offset; 
    $s5+=1+$offset;
    $s6+=1+$offset;
    $s7+=1+$offset; 

    print fOut  "$s0 $s1 $s2 $s3 $s4 $s5 $s6 $s7\n";
  } 

  $offset+=$corners_number[$i]; 
}

close fOut;
