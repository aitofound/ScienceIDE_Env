#!/usr/bin/perl -s

my $Help     = ($h or $H or $help);
my $Undo     = ($u or $undo);
my $OutCode  = ($o or $output or "check_variables.f90");
my $Verbose  = ($v or $verbose);

# Allow in-place editing
$^I = "";

use strict;

&print_help if $Help or not @ARGV;

my @source = @ARGV;  # source files to be processed
my $source;          # source file being processed
my @lines;           # lines in one source file
my $Line;            # current line (with continuation lines)
my $Module;          # current module name
my %Vars;            # hash of variable information indexed by lc(varname)
my $Insert;          # source code inserted
my $InsertStart = "  ! START_CHECK_VAR ---------------";
my $InsertEnd   = "  ! END_CHECK_VAR -----------------";
my $FileChanged;     # true if file changed

# variables for ModCheckVar
my $InitUse;         # use init_check_* routines
my $InitCall;        # call init_check_* routines
my $CheckUse;        # use do_check_* routines
my $CheckCall;       # call do_check_* routines

foreach $source (@source){

    # Skip files that are not actually compiled
    next if $source =~ /_(orig|empty)\.f90/;

    print "Working on source file $source\n" if $Verbose;
    
    # read source file into an array of lines
    open(FILE, $source) or warn "Could not open source file $source\n";
    @lines = <FILE>;
    close(FILE);

    $FileChanged = 0;

    &process_file;

    # If file did not change, there is nothing to do
    next unless $FileChanged;

    # Write modified file
    open(FILE, ">$source");
    print FILE @lines;
    close FILE;

}

### remove lines related to some modules from check_variable.f90
### if($Undo){

# Create output code
if($InitUse){

    print "Creating $OutCode\n";
    
    open(FILE, ">$OutCode");
    print FILE "
subroutine init_check_variables

$InitUse

  implicit none
  !-------------------------------------------------------------------------
$InitCall

end subroutine init_check_variables
!===========================================================================
subroutine do_check_variables

$CheckUse

  implicit none
  !-------------------------------------------------------------------------
$CheckCall

end subroutine do_check_variables
";
    close FILE;
}
    
exit 0;
#=============================================================================
sub process_file{

    my $IsDeclaration; # true after "implicit none" statement
    my $IsInterface;   # true inside interfaces and type declarations
    my $OldInsert;     # true inside the old inserted code

    foreach my $line (@lines){

	# Copy into $_ for easy manipulation
	$_ = $line;

	if(/$InsertStart/){
	    $OldInsert = 1;
	    $FileChanged = 1;
	}
	$line = '' if $OldInsert;
	$OldInsert = 0 if /$InsertEnd/;
	next if $OldInsert or /$InsertEnd/;

	# There is nothing else to do for "Undo" operation
	next if $Undo;
	
	# skip module procedure lines (looks like module start)
	next if /^\s*module\s+procedure/i;
	
	# process lines inside the modules only
	if(/^\s*module\s+(\w+)/i){
	    $Module = $1;
	    $IsDeclaration = 0;
	    print "module name is $Module\n" if $Verbose;
	}
	next unless $Module;
	
	if(/^\s*(contains|end\s+module)/i){
	    print "end declaration of module $Module\n" if $Verbose;

	    &create_insert($line);
	    if($Insert){
		$FileChanged = 1;  # file to be saved

		print $Insert if $Verbose;
	    
		if(/^\s*contains/){
		    $line = $Insert;
		}else{
		    $line = "$Insert$_";
		}
	    }

	    # Reset global variables
	    $Module = '';
	    $IsDeclaration = 0;
	    $Insert = '';
	    %Vars = ();
	    next;

	}

	next if /^\s*$/;    # skip lines with only whitespaces

	# Store thread private variables
	if(/^\s*\!\$omp\s+threadprivate\(([^\)]*)/i){
	    my $threadprivate = $1;
	    $threadprivate =~ s/ //g;
	    foreach my $var (split(/,/,$threadprivate)){
		$Vars{lc($var)}{"threadprivate"} = 1;
	    }
	}

	next if /^\s*\!/; # skip pure comment lines

	# skip interfaces and type declarations
	if(/^\s*end\s+(interface|type)\b/i){
	    $IsInterface = 0; # End of interface
            next;
	}
	$IsInterface = 1 if /^\s*(interface|type)\b/i;
	next if $IsInterface;
	
	# Find first variable declarations
	$IsDeclaration = 1 if /^[^!]*::/;
	next unless $IsDeclaration;
	
	s/\s*[\n\r]+//;           # cut off trailing space and \n\r
	s/^\s+//;                 # remove leading spaces
	s/\s+/ /g;                # remove multiple spaces
	s/\s*\!.*//;              # strip comments
	
	$Line .= $_;              # collect continuation lines

	next if
	    $Line =~ s/\s*\&$/ /; # remove & and read continuation line 
	
	&process_line;

	$Line = '';               # delete line
    }
    
}
#=============================================================================
sub process_line{

    $_ = $Line;  # Use $_ for easier pattern matching

    return if /^\s*(save|private|public)/i; # skip save/private/public

    s/\s+//g; # remove spaces
    
    if(not /::/){
	print "  Missing :: in $Module: $Line\n";
	return;
    }
    
    my $Type = lc($`);
    my $Vars = $';

    # skip pointers/parameters/derived types ???
    return if $Type =~ /\b(pointer|parameter|type)\b/i; 

    # remove public/private/target attributes
    $Type =~ s/,(public|private|target)\b//i; 

    # Remove (/.../) strings
    $Vars =~ s/\(\/[^\/]*\/\)//g;

    # Set allocatable attribute
    my $allocatable;
    $allocatable = 1 if $Type =~ s/,allocatable//i;

    # Check for dimension() 
    my $dimension;
    $dimension = $1 if $Type =~ s/,dimension(\(([^\(]+)\))//i;

    # store individual (...) dimensions into the dim hash
    my %dim;
    while($Vars =~ s/(\w+)(\(([^\(\)]*\([^)]+\))*[^\(]*\))/$1/){
	$dim{$1} = $2;
    }
        
    print "$_\n" if $Verbose;
    foreach (split(',',$Vars)){

	# skip block indexed variables
	next if /_[A-Z]*[AB]/ or /_BLK/i;  

	my $Name = $_;
	$Name =~ s/=.*//;         # get rid of initialization part
	
	my $key = lc($Name);

	# store information
	$Vars{$key}{"name"}        = $Name;
	$Vars{$key}{"type"}        = $Type;
	if($dimension){
	    $Vars{$key}{"array"}   = $dimension;
	}else{
	    $Vars{$key}{"array"}   = "$dim{$Name}";
	}
	$Vars{$key}{"allocatable"} = 1 if $allocatable;

	print "Var=$Name Type=$Type Array=",$Vars{$key}{'array'},
	    " Allocatable=$allocatable\n"
	    if $Verbose;

    }
}
#=============================================================================
sub create_insert{

    my $line = shift; # get argument
    
    my $DeclareVars;
    my $InitVars;
    my $CheckVars;

    foreach my $key (sort keys %Vars){
	# Skip thread private variables
	next if $Vars{$key}{"threadprivate"};

	# extract fields into scalars
	my $Name        = $Vars{$key}{"name"};
	my $Type        = $Vars{$key}{"type"};
	my $allocatable = $Vars{$key}{"allocatable"};
	my $array       = $Vars{$key}{"array"};

	$Type .= ", allocatable" if $allocatable;
	
	# unique control variable name
	my $NameCheck = $Name . "__";

        # Declare the check variable
	$DeclareVars .= "  $Type :: $NameCheck$array\n";

        # Initialize the check variable
	if($allocatable){
	    $InitVars .= "    if(allocated($Name)) $NameCheck = $Name\n";
	}else{
	    $InitVars .= "    $NameCheck = $Name\n";
	}

	# Check the variable
	my $indent = "";
	if($allocatable){
	    $CheckVars .= "     if(allocated($Name))then\n";
	    $indent     = "   ";
	}
	my $operator = "/=";
        $operator = ".neqv." if $Type =~ /logical/;
	if($array){
	    $CheckVars .= "$indent     if(any($NameCheck $operator $Name)) &\n"
	}else{
	    $CheckVars .= "$indent     if($NameCheck $operator $Name) &\n"
	}
	$CheckVars .= "$indent          write(*,*)'$Module::$Name changed'\n";
	$CheckVars .= "     end if\n" if $allocatable;
    }

    # Nothing to do if there are not variables to check
    return unless $DeclareVars;

    # Add code to check_variables.f90 except for some files
    $InitUse   .= "  use $Module, ONLY: init_check_$Module\n";
    $CheckUse  .= "  use $Module, ONLY: do_check_$Module\n";
    $InitCall  .= "  call init_check_$Module\n";
    $CheckCall .= "  call do_check_$Module\n";

    # Set "contains" line with or without InsertEnd and InsertStart markers
    my $Contains = "contains";
    $Contains = "$InsertEnd\n$Contains\n$InsertStart" if $line=~/^\s*contains/;

    # Create code in this module
    $Insert = "$InsertStart

  public:: init_check_$Module, do_check_$Module

$DeclareVars

$Contains
  !==========================================================================
  subroutine init_check_$Module

$InitVars
  end subroutine init_check_$Module
  !==========================================================================
  subroutine do_check_$Module

$CheckVars
  end subroutine do_check_$Module
$InsertEnd
";
    

}

#=============================================================================
sub print_help{
    print
'Purpose: Create code to check if variables change or not, 
         which can help finding issues with multi-threaded code.

Usage:

    CheckVarChange.pl [-h] [-v] [-u] [-o=NAME] FILE1 [FILE2 ...]

    -h -help       Print help message and exit.
    -v -verbose    Verbose output.
    -o=NAME        Set the name of the main output code. 
                   Default is check_variables.f90
    -u -undo       Undo/remove changes.
    FILE1 FILE2    Files to be processed and modified.

Examples:

   Add variable checking to ModAMR.f90 with verbose information shown

CheckVarChange.pl -v ModAMR.f90

   Add variable checking to all src/Mod*.f90 and srcBATL/BATL*.f90 modules

CheckVarChange.pl -o=src/check_variables.f90 src/Mod*.f90 srcBATL/BATL*.f90

   Remove variable checking code from ModAMR.f90

CheckVarChange.pl -u ModAMR.f90

   Remove variable checking from all Mod*.f90 modules

CheckVarChange.pl -u Mod*.f90

';

    exit;
}
