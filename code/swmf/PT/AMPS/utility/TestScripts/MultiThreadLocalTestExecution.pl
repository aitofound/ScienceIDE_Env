#!/usr/bin/perl -s

# count the number of tests 
my $nTotalTests=0;
my ($line,$TestSetSize,$CurrentTestSetSize,$iTestSet);

if (-e Makefile.test.bak) {
  `mv Makefile.test.bak Makefile.test`;
}

open(fIn,"<Makefile.test");
open(fOut,">Makefile.test.bak");

while ($line=<fIn>) {
  print fOut "$line"; 
  if ($line=~m/findstring rundir... done/) {
    $nTotalTests++;
  }
}
   
close (fIn);
close (fOut);

if ($nthreads==0) {
  die "The number of threads is not defined (should be defined as -nthreads=\[\] in the argiment line\n"; 
}

print "The total number of test: $nTotalTests\n";
print "Execution thread number: $nthreads\n"; 

$TestSetSize=int($nTotalTests/$nthreads);
print "Test Set Size: $TestSetSize\n";

$iTestSet=1;
$CurrentTestSetSize=0;

open(fIn,"<Makefile.test.bak");
open(fOut,">Makefile.test");

my ($l0,$l1);

$l0=<fIn>; 

while ($l1=<fIn>) {
  if ($l1=~m/^test_run:/) {
    $l1="test_run_thread1:\n";
  }

  if ($l1=~m/findstring rundir... done/) {
    $CurrentTestSetSize++;

    if (($CurrentTestSetSize==$TestSetSize)&&($iTestSet<$nthreads)) {
      $iTestSet++;
      $CurrentTestSetSize=0;

      $l0="\ntest_run_thread$iTestSet:\n".$l0;
   }
  }

  print fOut "$l0";
  $l0=$l1; 
}

print fOut "$l0";

close (fIn);
close (fOut);
