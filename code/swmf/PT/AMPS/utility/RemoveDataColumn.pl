#!/usr/bin/perl
#remove a column from a file 

use strict;
use warnings;
use POSIX qw(strftime);
use List::Util qw(first);
use IO::File;
use File::Copy;

my (@FileNameTable,@ColumnTable,@ColumnTableSorted,$line,$Offset,@l); 

#Keys: 
#-c [the column that neer to be removed]
#-f [the file that needs to be processed]

for (my $i=0;$i<=$#ARGV;$i++) {
  if ($ARGV[$i] eq "-f") {
    $i++;
    push(@FileNameTable,$ARGV[$i]);
  }
  elsif ($ARGV[$i] eq "-c") {
    $i++; 
    push(@ColumnTable,$ARGV[$i]);
  }
  else {
    die "Error: Unknown option"; 
  } 
}

@ColumnTableSorted=sort {$b <=> $a } @ColumnTable;

print("Remove Columns: @ColumnTableSorted\n");
print("Process files:\n");

for (my $i=0;$i<=$#FileNameTable;$i++) {
  print("$i: $FileNameTable[$i]\n");
}

for (my $ifile=0;$ifile<=$#FileNameTable;$ifile++) {
  open (fIn,"<$FileNameTable[$ifile]");
  open (fOut,">$FileNameTable[$ifile].ColumnReduced");

  while ($line=<fIn>) {
    chomp($line);
    @l=split(' ', $line);

    for (my $icolumn=0;$icolumn<=$#ColumnTable;$icolumn++) { 
      $Offset=$ColumnTable[$icolumn]; 

      if ($Offset <= $#l) {
        splice(@l,"$Offset",1);
      }
    }

    print fOut "@l\n";
  }

  close (fIn);
  close (fOut);
}
