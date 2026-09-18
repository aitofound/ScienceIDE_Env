#!/usr/bin/perl -s

# Calculate run speed for various tests.

my $Tests = $t;

use strict;

if(not @ARGV){
    print "
Get the run time of various tests from multiple test_swmf.log files.
Can be used to check how speed changes over time.

Usage:
   test_speed [-t=TESTS] DIR1 [DIR2 ...]

-t=TESTS      list of tests for which timings are required. By default all tests are evaluated.

Example:
   test_speed -t=earth,awsom_gpu SWMF_TEST_RESULTS/2022/01/2[0-2]/pleiades/
";
    exit 0;
}

my %Test;
my %Speed;
my $test;
my $file;
my $dir;

foreach $dir (@ARGV){
    $file = "$dir/test_swmf.log";
    if(not open(FILE, $file)){
	warn "$file is missing\n";
	next;
    }
    
    while(<FILE>){
	if(/make test(\w+)_run\b/){
	    $test = $1;
	    if($Tests){
		next unless $Tests =~ /\b$test\b/;
	    }
	    # Found running the test
	    $Test{$test}++;
	}
	# Extract the run time from a line like
	# "PostProc.pl: TIMINGS from runlog (init, run) 1.81 1.05"
	$Speed{$dir}{$test} = $1 if /TIMINGS/ and /([\d\.]+)$/;

	$Speed{$dir}{$test} = $2 if
	    /.*(BATSRUS|SWMF)\s+(\d+\.\d+).*/ or
	    /.*(BATSRUS|SWMF)\s*1\s*1\s+(.*?)\s+/;
    }
}

foreach $test (sort keys %Test){
    foreach $dir (@ARGV){
	printf "test%-20s %s: %8.2f\n", $test, $dir, $Speed{$dir}{$test};
    }
}

