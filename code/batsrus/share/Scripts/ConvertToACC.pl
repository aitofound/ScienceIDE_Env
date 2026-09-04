#!/usr/bin/perl -s

my $Help     = ($h or $H or $help);
my $Verbose  = ($v or $verbose);
my $Declare  = ($d or $declare);
my $Argument = ($a or $argument);

# Allow in-place editing
$^I = "";

use strict;
use English;

&print_help if $Help or not @ARGV;

my @source = @ARGV;  # source files to be processed
my $source;          # source file being processed
my $output;
my @lines;           # lines in one source file
my $Line;            # current line (with continuation lines)
my $Module;          # current module name
my $Subroutine;      # current subroutine name
my %Vars;            # hash of module variables indexed by lc(varname)
my %SubVars;         # hash of subroutine variables by lc(subname:varname)
my %Funcs;           # hash of function information indexed by lc(funcname)

foreach $source (@source){

    # Skip files that are not actually compiled
    next if $source =~ /_(orig|empty)\.f90/;

    print "Working on source file $source\n" if $Verbose;

    # Read source file into an array of lines
    open(FILE, $source) or warn "Could not open source file $source\n";
    @lines = <FILE>;
    close(FILE);

    &find_global_var;

    if($Argument){
	&find_function_tree;
	&modify_file;
    }

    print "input source file:$source\n" if $Verbose;
    $source =~ s/\.f90/\_acc\.f90/;
    print "output source file:$source\n" if $Verbose;

    # Write modified file
    open(FILE, ">$source");
    print FILE @lines;
    close FILE;

}


exit 0;

#=============================================================================
sub find_global_var{

    my $IsInterface;	# true inside interfaces and type declarations

    my $iLine = -1;
    foreach my $line (@lines){
	$iLine++;

	# Copy into $_ for easy manipulation
	$_ = $line;

	next if /^\s*$/;	# Skip lines with only whitespaces

	next if /^\s*\!/ and not /^\s*(\!\$omp)/; # skip pure comment lines

	# Skip module procedure lines
	next if /^\s*module\s+procedure/i;

	# Process lines inside the modules only
	if(/^\s*module\s+(\w+)/i){
	    $Module = $1;
	    $Subroutine = "";
	    print "module name: $Module\n" if $Verbose;
	}

	next unless $Module;

	if(/^\s*(end\s+module)/i){

	    print "end module $Module\n" if $Verbose;

	    if($Verbose) {
		print "Print test info:\n";
		print "----------------\n";
		foreach my $key (sort keys %Vars){
		    print "key=$key,Var=",$Vars{$key}{'name'},
		    " ,Type=",$Vars{$key}{'type'},
		    " ,Array=",$Vars{$key}{'array'},
		    " ,threadprivate=",$Vars{$key}{'threadprivate'},"\n";
		}
		print "test info end\n";
		print "-------------\n";
	    }



	    return; # Is this needed? Useful? Multiple modules?

	}

	# Skip interfaces and type declarations
	if(/^\s*end\s+(interface|type)\b/i){
	    $IsInterface = 0;	# End of interface
	    next;
	}
	$IsInterface = 1 if /^\s*(interface|type)\b/i;
	next if $IsInterface;

	# Contained subroutine
	if(/^\s*subroutine\s+(\w+)/i){
	    $Subroutine = $1;
	    print "subroutine name: $Subroutine\n" if $Verbose;
	}

	# Store thread private variables that are supposed to be private
	# (This assumes the threadprivate declarations
	# always come after the actual declarations.)
	if(/^\s*\!\$omp\s+threadprivate\(([^\)]*)/i){
	    my $threadprivate = $1;
	    $threadprivate =~ s/ //g;

	    foreach my $var (split(/,/,$threadprivate)){
		$Vars{lc($var)}{"threadprivate"} = 1 if exists $Vars{lc($var)};
	    }
	    next;
	}

	s/\s*[\n\r]+//;		# cut off trailing space and \n\r
	s/^\s+//;		# remove leading spaces
	s/\s+/ /g;		# remove multiple spaces
	s/\s*\!.*//;		# strip comments

	$Line .= $_;		# collect continuation lines

	# remove & and read continuation line
	next if $Line =~ s/\s*\&$/ /;

	&process_line($iLine) if $Line =~ /::/;

	$Line = '';		# delete line
    }

}


#=============================================================================
sub process_line{

    my $iLine = shift;

    $_ = $Line;			# Use $_ for easier pattern matching

    return if /^\s*(save|private|public)/i; # skip save/private/public

    s/\s+//g;		   # Remove spaces (hyzhou: maybe unecessary?)

    /::/;

    my $Type = lc($PREMATCH);	# String preceding the matched string
    my $Vars = $POSTMATCH;	# String following the matched string

    # Skip pointer/parameters/derived types
    return if $ Type =~ /\b(pointer|parameter|type)\b/i;

    # Remove public/private/target attributes
    $Type =~ s/,(public|private|target)\b//i;

    # Remove and store intent if present
    my $Intent = "";
    $Intent = ", $1" if $Type =~ s/,(intent\(\w+\))//i;

    # Remove (/.../) and [...] strings
    $Vars =~ s/\(\/[^\/]*\/\)//g;
    $Vars =~ s/\[[^\/]*\]//g;

    # Set allocatable attribute
    my $allocatable;
    $allocatable = 1 if $Type =~ s/,allocatable//i;

    # Check for dimension()
    my $dimension;
    $dimension = $1 if $Type =~ s/,dimension(\(([^\(]+)\))//i;

    # Store individual (...) dimensions into the dim hash
    my %dim;
    while($Vars =~ s/(\w+)(\(([^\(\)]*\([^)]+\))*[^\(]*\))/$1/){
	$dim{$1} = $2;
    }

    my $declare = '';
    foreach (split(',',$Vars)){
	# Skip cell/face indexed variables
	# What about Cmax_Dt?
	next if /_[VD]?[CFGXYZ]/ and not $Declare;

	my $Name = $_;

	$Name =~ s/=.*//;	# get rid of initialization part( !!!)

	my $key = lc($Name);
	$key = "$Subroutine\-$key" if $Subroutine;

	# Store information
	$Vars{$key}{'name'}        = $Name;
	$Vars{$key}{'type'}        = $Type;
	$Vars{$key}{'intent'}      = $Intent;

	if($dimension){
	    $Vars{$key}{'array'}   = $dimension;
	}else{
	    $Vars{$key}{'array'}   = "$dim{$Name}";
	}
	$declare .= "  !\$acc declare create(".$Vars{$key}{'name'}.")\n"
	    if $Declare and not $Subroutine;
 
    }
    $lines[$iLine] .= $declare;

}

#=============================================================================
sub find_function_tree{

    my $FuncLevel = 0;
    my $NameFunc = "";
    my $namefunc = '';
    my $Args = "";
    my $ParentFunc = "";
    my $Level1Func = "";
    my $iStart = 0;	    # index of the first line for continuation

    while(my ($i, $line) = each @lines){

	$line =~ s/\s*[\n\r]+//;     # cut off trailing space and \n\r
	$line =~ s/^\s+//;	     # remove leading spaces
	$line =~ s/\s+/ /g;	     # remove multiple spaces
	$line =~ s/\s*\!.*//;	     # strip comments

	if($line =~ /call\s+(\w+)/){
	    my $called = $1;
	    $Funcs{$namefunc}{'call'}{$called} = 1;
	    # Only records the beginning line number!
	    push @{$Funcs{lc($called)}{'callpos'}}, $i;
	}

	$Line .= $line;		# collect continuation lines

	# remove & and read continuation line
	next if $Line =~ s/\s*\&$/ /;

	if($Line =~ /^\s*subroutine\s+([\w]+)(.*)/ ) {
	    $FuncLevel += 1;
	    $NameFunc = $1;
	    $namefunc = lc($NameFunc);
	    $Args     = $2;
	    # get rid of opening and closing parens
	    $Args =~ s/^\(//; $Args =~ s/\)$//;

	    # Store name of level 1 contained subroutine
	    $Level1Func = $NameFunc if $FuncLevel == 1;
	    $ParentFunc = ""        if $FuncLevel < 2;

	    # Assign parent for level 2 contained subroutine
	    $ParentFunc = $Level1Func if $FuncLevel == 2;
	    print "Level 2 function $1 of $NameFunc\n" if $FuncLevel == 2 and $Verbose;

	    # Store information
	    $Funcs{$namefunc}{"args"}  = $Args;
	    $Funcs{$namefunc}{"level"} = $FuncLevel;
	    $Funcs{$namefunc}{"parent"}= $ParentFunc if $ParentFunc;
	    $Funcs{$namefunc}{"pos"}   = $iStart;

	}elsif($Line =~ /end subroutine/) {
	    $FuncLevel -= 1;
	    $ParentFunc = "";
	}elsif($FuncLevel > 0){

	    # Record all the global var occurence in this function
	    while( $Line =~ /\b([a-zA-z]\w*)/g ){
		my $var = $1;
		$Funcs{$namefunc}{'vars'}{$var} = 1 
		    if ($Vars{lc($var)} or $Vars{$ParentFunc.'-'.lc($var)}) and
		    not $Vars{$NameFunc.'-'.lc($var)};
	    }
	}

	$iStart = $i+1;
	$Line = '';		# delete line
    }

    # Select function calls within module
    foreach my $key (keys %Funcs) {
	foreach my $f (keys %{$Funcs{$key}{'call'}}){
	    if(not exists $Funcs{$f}){
		delete($Funcs{$key}{'call'});
		delete($Funcs{$key}{'callpos'});
	    }
	}
    }

    if($Verbose){
	print "========================\n";
	foreach my $key (sort keys %Funcs) {
	    print "Func name: $key \n";
	    print "Func args:",$Funcs{$key}{"args"},"\n";
	    print "Func level:",$Funcs{$key}{"level"},"\n";
	    print "Func line:",$Funcs{$key}{"pos"},"\n";
	    print "Func call line:",@{$Funcs{$key}{'callpos'}},"\n"
		if $Funcs{$key}{'callpos'};
	    foreach my $f (sort keys %{$Funcs{$key}{'call'}}){
		print "Func call: $f\n";
	    }
	    foreach my $f (sort keys %{$Funcs{$key}{'vars'}}){
		print "Func used vars: $f\n";
	    }
	    print "========================\n";
	}
    }

    print "End of function parser.\n" if $Verbose;

}

#=============================================================================
sub modify_file{

    foreach my $key (sort keys %Funcs) {
	foreach my $callfunc (sort keys %{$Funcs{$key}{'call'}}){
	    foreach my $var (sort keys %{$Funcs{$callfunc}{'vars'}}){

		if($var eq "iFlux"){
		    print "!!! function $key, called func=$callfunc\n";
		}


		# Add to the calling function's arg list if the function
		# is called in the module and the variable is not 
		# declared in this function.

		if(exists $Funcs{$key}{'callpos'} and
		   not exists $Vars{"$key\-".lc($var)} and
		   not $Funcs{$key}{'args'} =~ /$var/i){
		    if(not grep( /^$var$/i, @{$Funcs{$key}{'addarg'}})){
			push @{$Funcs{$key}{'addarg'}}, $var;
		    }
		}

		# Add to the called func arg list
		if(not $Funcs{$callfunc}{'args'} =~ /$var/){
		    if(not grep( /^$var$/i, @{$Funcs{$callfunc}{'addarg'}})){
			push @{$Funcs{$callfunc}{'addarg'}}, $var;
		    }
		}
	    }
	}

    }

    if($Verbose){
	foreach my $key (sort keys %Funcs) {
	    print "--------------------\n";
	    print "Func name: $key\n";
	    if(exists $Funcs{$key}{'addarg'}){
		print "addarg: @{$Funcs{$key}{'addarg'}}\n";
	    }
	}
    }

    # loop over each func that has required arguments
    #   add to the func call
    #   add to the subroutine arg list
    #   declaration inside subroutine

    my $str;
    foreach my $key (sort keys %Funcs){
	if(exists $Funcs{$key}{'addarg'}){
	    if($Verbose){
		print "Func:",$key," in line ",$Funcs{$key}{'pos'},"\n";
		print $lines[$Funcs{$key}{'pos'}],"\n";
	    }
	    $lines[$Funcs{$key}{'pos'}] =~ /^(\s*)/;
	    my $whitespace = $1 . "  ";
	    # append addarg to the beginning
	    $str = join ",&\n\t",@{$Funcs{$key}{'addarg'}};

	    if($lines[$Funcs{$key}{'pos'}] =~ /\(/){
		$lines[$Funcs{$key}{'pos'}] =~ s/\(/\(\&\n$whitespace$str ,\&\n\t/;
	    } else{
		$lines[$Funcs{$key}{'pos'}] =~ s/\n/\(\&\n$whitespace$str\)\n/;
	    }

	    if(exists $Funcs{$key}{'callpos'}){
		print "call Func:",$key," in line ",
		@{$Funcs{$key}{'callpos'}},"\n" if $Verbose;
		# append addarg to call functions
		foreach my $fc (@{$Funcs{$key}{'callpos'}}){
		    if($lines[$fc] =~ /(.*\bcall\s+\w+\()/ ){
			$lines[$fc] = $1 . "\&\n$whitespace$str,&\n\t" . $POSTMATCH;
		    } else{
			$lines[$fc] =~ s/\n/\(\&\n$whitespace$str\)\n/;
		    }
		}
	    }

	    my $addDeclare = '';
	    my $parent = $Funcs{$key}{'parent'};

	    # add var declarations
	    foreach my $Var (@{$Funcs{$key}{'addarg'}}){
		my $var = lc($Var);
		$var = "$parent\-$var" if exists $Vars{"$parent\-$var"};
		$addDeclare .= $whitespace . $Vars{$var}{'type'} .
		    $Vars{$var}{'intent'} . ':: ' .
		    $Vars{$var}{'name'} . $Vars{$var}{'array'} . "\n";
		#splice @lines, $Funcs{$key}{'pos'}+$count, 0, $addDeclare;
	    }

	    # If there is keyword 'intent', find the last argument declaration
	    # line;
	    # if no keyword 'intent', find the first local declaration.

	    my $count = 0;

	    if($Funcs{$key}{'args'}){
		do{
		    $count += 1;
		}until($lines[$Funcs{$key}{'pos'}+$count] =~ /intent/ &&
		       not $lines[$Funcs{$key}{'pos'}+$count+1] =~ /intent/);
	    }else{
		while(not ($lines[$Funcs{$key}{'pos'}+$count+1] =~ /\!--/ or
			   $lines[$Funcs{$key}{'pos'}+$count+1] =~
			   /(real|integer|character)/)){
		    $count += 1;
		}
	    }

	    $lines[$Funcs{$key}{'pos'}+$count] .= "\n" . $addDeclare;
	}
    }

}


#=============================================================================
sub print_help{
    print
	'
Purpose: Enable OpenACC compilation by modifying one or more Fortran 90+ files.
FILENAME.f90 will be processed into  FILENAME_acc.f90

Usage:

  ConvertToACC.pl [-h] [-v] FILE1 [FILE2 ...]

  -h -help       Print help message and exit.
  -v -verbose    Verbose output.
  -a -argument   Pass "global" variables as arguments
  -d -declare    Declare module variables for the device.
  FILE1 FILE2    Files to be processed and modified.

Examples:

Add ACC declarations and arguments to ModFaceFlux.f90: 

  ConvertToACC.pl -v -a -d ModFaceFlux.f90

Add ACC declarations to F90 files in srcBATL

  ConvertToACC.pl -d srcBATL/*.f90

';

    exit;
}
