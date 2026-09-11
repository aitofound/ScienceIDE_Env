#!/usr/bin/perl
use strict;
use warnings;
use Fcntl qw(:flock SEEK_END);


my $nTotalThreads=0;
my @Mask;

`rm -rf all-res`;

foreach (@ARGV) {
  if (/^-np=(.*)/i) {$nTotalThreads=$1; next};

  if (/^-mask=(.*)/i) {push(@Mask,$1); next};
}

`mkdir all-res`;  

for (my $i=0;$i<$nTotalThreads;$i++) {
  `mkdir all-res/$i`; 

  foreach my $m (@Mask) { 
    `cp amps-out.thread=$i/*$m all-res/$i`;  
  }
}

`tar -zcvf all-res.tar all-res`;
`rm -rf all-res`;
