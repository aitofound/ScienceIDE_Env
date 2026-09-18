#!/usr/bin/perl

# Wrapper script around Fortran compilers. See help message below.
# Author: Gabor Toth. (c) University of Michigan, 2025

use strict;
use Cwd qw(abs_path);
use FindBin qw($Script $Bin $RealBin $RealScript);
my $verbose;
my @file;

# Find Fortran file names among arguments and check for --wrapper flag
foreach (@ARGV){
    push(@file, $_) if /\w.*\.(f\d*|for)$/i; # add file to @file
    $verbose = 1 if s/\-\-wrapper//; # remove --wrapper from ARGV
}

# Print help information if called without arguments
if(not scalar @ARGV){
    print "
fortran_wrapper.pl by Gabor Toth (c) University of Michigan 2025.

Call this wrapper around some Fortran compiler to get an error message
for ambiguous syntax like 10.0**-2 * 3 that is evaluated
by ifort and ifx as          10.0**(-2*3) = 0.000001
by gfortran and nvfortran as (10.0**-2)*3 = 0.03
If the --wrapper flag is added, the script will show extra information.

Installation:
   Create symbolic links named the same as the compilers
   pointing at this script $RealBin/$RealScript
   and have those symbolic links in the path _before_ the
   actual compilers, so \'which $Script\' returns the link.

Examples of usage:
  # show this help message:
  gfortran
  # check files without compilation:
  fortran_wrapper.pl file1.f90 file2.f90 
  # check and compile file1.f90 with ifort:
  ifort -O3 file1.f90
  # check with verbose output and compile files with ifx
  ifx --wrapper -c -O2 file1.f90 file2.f90 

";
    exit 0;
}

my $compiler;
if($Script ne $RealScript){
    # Find the real compiler from the script name (which is a symbolic link)
    my $dir;
    foreach $dir (split(':', $ENV{PATH})){
	my $path = abs_path($dir);
	$compiler = "$path/$Script", last if $path ne $Bin and -x "$path/$Script";
    }
    die "ERROR: could not find real compiler $Script in the path\n" unless $compiler;
    print "Running script $Bin/$Script for compiler $compiler\n" if $verbose;
}

# Process all Fortran files
my $file;
foreach $file (@file){
    open(FILE, $file) or die "Could not open $file\n";
    print "processing file=$file\n" if $verbose;
    # Check file for errors
    my $iline;    # actual line number
    my $nline;    # line number of first line for continuation lines
    my $nerror;   # number of errors in this file
    my $nquote;   # number of strings in quotation marks
    my %quote;    # hash of quoted texts 
    my $line;     # processed continuation lines
    while(<FILE>){
	$iline++;
	next if /^\s*\!/; # skip comments
	s/^\s*&//; # remove leading & from continuation line

	# Check for trailing & indicating continuation line
	if(s/\&\s*(\!.*)?\n//) {
	    $nline = $iline unless $line; # store current line index
	    $line .= $_; # collect continuation lines into $line
	    next;
	}
	# Check if this is the final continuation line
	if($line){
	    $_ = $line . $_; # Put the full continued line into $_
	    $line = "";      # reset continuation line
	}else{
	    $nline = $iline; # use current line index
	}

	# Replace quoted strings with #0s* and store them into a hash
	while(s/(\')([^']*)\'|(\")([^\"]*)\"/sprintf("#%d%s", $nquote, "s" x length($2))/e){
	    $quote{"#$nquote"} = $1.$2.$1; # store quoted text
	    $nquote++;                     # increase quotation index
	}
	# remove trailing comments
	s/\s*\!.*$//;

	# Check for ** + and ** - operator
	if(/\*\*\s*[+-]/){
	    my $before = $`;
	    my $match  = $&;
	    my $after  = $';
	    # replace (..) with X in $after
	    $after =~ s/(\(.*?\))/sprintf("%s", "X" x length($1))/eg;
	    # check for VAR * or VAR /
	    if($after =~ /^(\s*[\.\w]+\s*)(\*|\/)/){
		my $beforeop = $1;
		my $operator = $2;
		$nerror++;
		warn "\nERROR $nerror: "
		    ."using signed exponent followed by $operator at line "
		    ."$nline in $file:\n";
		# put back quoted strings
		s/(\#\d)(s*)/$quote{$1}/ge;
		print $_;
		print "-" x length($before),"^",
		    "-" x (length($match) + length($beforeop) - 1),"^\n";
	    }elsif($verbose){
		warn "\nWARNING: signed exponent at line $nline in $file\n";
	    }

	}
	# reset quotation hash
	%quote = () if $nquote;
	$nquote = 0;
    }
    close(FILE);
    if($nerror){
	warn "
ifort and ifx evaluate the * and / operators BEFORE the exponentiation!
gfortan and nvfortran evaluate the * and / operators AFTER the exponentiation!
Add parentheses to clarify the expression and make your code portable!
Replace **+ with ** and **- with **(- ) to conform with Fortran standard!
";
	if($nerror > 1){
	    die "\n$file contains $nerror errors, so it cannot be compiled\n";
	}else{
	    die "\n$file contains an error, so it cannot be compiled\n";
	}
    }
}

if($compiler){
    # call the real compiler with all the arguments
    print "$compiler @ARGV\n" if $verbose;
    exec($compiler, @ARGV);
}else{
    # report
    print "No errors were found in @file\n";
}
exit 0;
