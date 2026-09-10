#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

# Allow in-place editing                                                        
$^I = "";

# Add local directory to search                                                 
push @INC, ".";

use strict;

our $Component       = 'PT';
our $Code            = 'AMPS';
our $MakefileDefOrig = 'Makefile.def.amps';
our @Arguments       = @ARGV;

my $Application;
my $MpiLocation;
my $Mpirun="mpirun";
my $TestRunProcessorNumber=4;

# name for the nightly tests
our $TestName;
our @Compilers;


my $config     = "share/Scripts/Config.pl";
#check util and share
my $GITCLONE = "git clone"; my $GITDIR = "git\@github.com:SWMFsoftware";

if (-f $config or -f "../../$config"){
}else{
    `$GITCLONE $GITDIR/share.git; $GITCLONE $GITDIR/util.git`;
}


#create Makefile.local
if (! -e "Makefile.local") {
  `touch Makefile.local`;
}

# build AMPS' Makefile.test
foreach (@Arguments) { 
    if(/^-install/) {
      `g++ -std=c++14 utility/TestScripts/table_parser.cpp -o table_parser`;      
      `cp MakefileTest/Table .`;
      `./table_parser 2`;
      `rm Table`; 

      #create .general.conf if not exists
      `touch .general.conf` unless (-e ".general.conf");
    }
}


# Run the shared Config.pl script
if(-f $config){
  require $config;
}else{
  require "../../$config";
}

our %Remaining;   # Arguments not handled by share/Scripts/Config.pl

#sort the argument line to push the 'application' argument first
my $cntArgument=0;

foreach (@Arguments) {
  if (/^-application=(.*)/i) {
    if ($cntArgument!=0) {
      my $t;
      
      $t=$Arguments[0];
      $Arguments[0]=$Arguments[$cntArgument];
      $Arguments[$cntArgument]=$t;
      
      last;
    }
  }
  
  $cntArgument++;
}

#process the 'help' argument of the script
foreach (@Arguments) { 
   if ((/^-h$/)||(/^-help/)) { #print help message
     print "Config.pl can be used for installing and setting of AMPS\n";
     print "AMPS' settings can be defined in the SWMF's Config.pl: Config.pl -o=PT:spice-path=path,spice-kernels=path,ices-path=path,application=casename\n\n";
     print "Usage: Config.pl [-help] [-show] [-spice-path] [-spice-kernels] [-ices-path] [-cplr-data-path] [-application]\n";
     print "\nInformation:\n";
     print "-h -help\t\t\tshow help message.\n";
     print "-s -show\t\t\tshow current settings.\n";
     print "-append\t\t\t\tadd new settings variables into .amsp.conf instead of re-defining their values.\n";
     print "-spice-path=PATH\t\tpath to the location of the SPICE installation.\n";
     print "-spice-kernels=PATH\t\tpath to the location of the SPICE kernels.\n";
     print "-ices-path=PATH\t\t\tpath to the location of ICES\n";
     print "-application=case\t\tthe name of the model case to use.\n";
     print "-boost-path=PATH\t\tthe path to the boost library.\n";
     print "-kameleon-path=PATH\t\tthe path for the Kameleon library.\n";
     print "-cplr-data-path=PATH\t\tthe path to the data files used to import other model results by PIC::CPLR::DATAFILE.\n";
     print "-model-data-path=PATH\t\tthe path to the user-defined input data files (the files are read directly by the user).\n";
     print "-output-path=PATH\t\tthe directory where AMPS' output files will be saved.\n";
     print "-batl-path=PATH\t\t\tthe path to the BATL directory\n";
     print "-swmf-path=PATH\t\t\tthe path to the SWMF directory\n";
     print "-set-test=(NAME)\/comp\t\tinstall nightly tests (e.g. comp=gnu,intel|pgi|all; NAME parameter is optional)\n";
     print "-rm-test\/comp\t\t\tremove nightly tests\n";
     print "-amps-test=[on,off]\t\ttells the code that a nightly test is executed\n";
     print "-test-blocks=n_execution_blocks\tsplits the entire list of tests in n_execution_blocks for concurrent execution. All SWMF related tests are combibed in a single target\n"; 
     print "-openmp=[on,off]\t\twhen \"on\" use OpenMP and MPI libraries for compiling AMPS\n";
     print "-link-option=-opt1:-opt2\tadd options \"-opt1 -opt2\" to linker\n";
     print "-compiler-option=opt1:opt2\tadd option \'opt\' into the compiler argument line\n";
     print "-f-link-option=-opt1:-opt2\tadd options \"-opt1 -opt2\" to linker whe fortran compiler is used as a linker \n";
     print "-cpp-link-option=-opt1:-opt2\tadd options \"-opt1 -opt2\" to linker whe c++ compiler is used as a linker \n";
     print "-cpp-compiler=opt\t\treplace C++ compiler name defined by the variable COMPILE.mpicxx in Makefile.conf\n";
     print "-cpplib-rm=opt\t\t\tremove libraty flag from the list defined by variable CPPLIB in Makefile.conf\n";
     print "-avx=[256,512,off]\t\tsettings for using AVX instructions\n";
     print "-mp=[on,off]\t\t\tallow memory prefetch\n";
     print "-cuda\t\t\t\tcompile AMPS as a CUDA code\n"; 
     print "-no-signals\t\t\tsupress interseption of the operating system signals\n";
     print "-input=[the name of the input file that is used at start after the code is compiled]\n";

     print "-check_macro=[on,off]\t\tturns on/off checking the macro used to control elecutions of AMPS. In case -test_macro=on, additional compiler directives will be added to the source code before compilation to ensure that all macros used are defined. Config.pl -check_macro=[...] will recreate the build directory to remove any previous macro checks if they exist.\n\n";
     print "-prefix=[on,off]\t\tcontrols the substitution of \$PREFIX with a specific prefix in the code.\n"; 
     
     print "-no-avx-matmul\t\t\tdisable AVX in the matrix multiplication functions\n";
     print "-no-avx-mover\t\t\tdisable AVX in the particle movers\n";

     print "-state-vector=[aligned,packed] \t\tthe mode of packing basic data in particle's state vector; aligned -- the length of particle's species id vector is sizeof(long int) and all fields of the state vector are aligned by 8; packed --  the length of the particle's species id is 1 byte, no alignment of the state vector fields\n\n";  

     print "-align=[on,off]\t\t\talign (8) state vectors and arrays when possible and improve the efficientcy\n";
     print "-align-base=base \t\talign change the base used for the alighnemnt of the particle state vector. The defail valus is 8 that correcpond to \"long int\" and \"double\". The case need to be changes if the state would contain varaibles with hight length, e.g. _mm256 \n";
 

     print "-concurrent_debug\n"; 
     print "-logger\n";

     print "-fexit=[exit,mpi_abort]\t\tselect function that will be used to terminate code in case of an error. In some systems mpi_abort() does not terminate the code, but in other systems exit() does not terminate the code\n"; 
     exit;
   }
   
   
   if ((/^-s$/i)||(/^-show/)) { #print AMPS settings
     print "\nAMPS' Setting options:\n";
      
     if (-e ".amps.conf") {
       my @f;
       
       open (FSETTINGS,"<.amps.conf");
       @f=<FSETTINGS>;
       close(FSETTINGS);
       
       print @f;
       print "\n";
     }
     else {
       print "No custom settings\n";
     }
     
     exit;
   } 
}

#process the configuration settings
foreach (@Arguments) {
  if (/^-application=(.*)/i) {
    my $application = lc($1);
  
    #default values
    if (! -e ".amps.conf") {
      add_line_amps_conf("TESTMODE=off");
      add_line_amps_conf("BATL=nobatl");

      add_line_amps_conf("KAMELEON=nokameleon");
    }

    my $application_full_name=$application;

     #remove path from the name of the input file is such exists
     my @list;

     $application=~s/\// /g;
     @list=split(' ',$application);
     $application=$list[$#list];

     if (!check_amps_conf("APPLICATION=$application")) {
       add_line_amps_conf("InputFileAMPS=$application.input");   
       add_line_amps_conf("APPLICATION=$application");

       `cp -f input/$application_full_name.* input/species.input .`;

       #set defailt compilation module flags in Makefile.local
       `sed '/COMPILE_/d' Makefile.local > Makefile.local.new`;
       `rm Makefile.local`;
       `mv Makefile.local.new Makefile.local`;

       `echo "InputFileAMPS=$application.input" >> Makefile.local`;
     }

    next
  };

  if (/^-input=(.*)$/i)        {
    add_line_amps_conf("PostCompileInputFileAMPS=$1");
    next;} 

  if (/^-mpi=(.*)$/i)        {$MpiLocation=$1;                next}; 
  if (/^-np=(.*)$/i)         {$TestRunProcessorNumber=$1;     next};
  if (/^-spice-path=(.*)$/i)      {
      if (!check_amps_conf("SPICE=$1")) {
        add_line_amps_conf("SPICE=$1");
        `echo SPICE=$1 >> Makefile.local`;
      }

      next}; 
  if (/^-spice-kernels=(.*)$/i)    {
      add_line_amps_conf("SPICEKERNELS=$1");
      next};
  if (/^-ices-path=(.*)$/i)       {
      add_line_amps_conf("ICESLOCATION=$1");
      next};
  
  if (/^-boost-path=(.*)$/i)       {
      add_line_amps_conf("BOOST=$1");
      next};
  if (/^-kameleon-path=(.*)$/i)       {
      if (!check_amps_conf("KAMELEON=$1")) {
        add_line_amps_conf("KAMELEON=$1");
        `echo KAMELEON=$1 >> Makefile.local`;
      }

      next};
      
  if (/^-logger/i) {
    add_line_general_conf("#undef _PIC_LOGGER_MODE_ \n#define _PIC_LOGGER_MODE_ _PIC_MODE_ON_\n");
    next;
  } 

  if (/^-concurrent_debug/i) {
    `g++ -g utility/compare_runs.cpp -o compare_runs -lpthread -lrt`; 

    my $fh;
    open($fh,'<',"Makefile");
    my @lines = <$fh>;
    close($fh);

    open($fh,'>',"Makefile");

    foreach my $line (@lines) {
      if($line =~ m/^EXTRALINKEROPTIONS_CPP=/) {
        $line = "EXTRALINKEROPTIONS_CPP=-lrt\n";
      }

      print $fh $line;
    }

    close($fh);

    add_line_general_conf("#undef _PIC__DEBUG_CONCURRENT_RUNS_ \n#define _PIC__DEBUG_CONCURRENT_RUNS_ _PIC_MODE_ON_\n");
    next;
  }

  if (/^-mp=(.*)$/i) {
    my $t;
    $t=uc($1);

    add_line_amps_conf("MemoryPrefetch=$t");
    next;
  }

  if (/^-openmp=(.*)$/i) {
    my $t;
    $t=lc($1);
    add_line_amps_conf("OPENMP=$1");
       
    if ($t eq "on") {
      if (! check_macro_general_conf("_COMPILATION_MODE_","_COMPILATION_MODE__HYBRID_")) {
        add_line_general_conf("#undef _COMPILATION_MODE_ \n#define _COMPILATION_MODE_ _COMPILATION_MODE__HYBRID_\n");
        `echo OPENMP=on >> Makefile.local`;
      }

      next;
    }
    elsif ($t eq "off") {
      if (! check_macro_general_conf("_COMPILATION_MODE_","_COMPILATION_MODE__MPI_")) {
        add_line_general_conf("#undef _COMPILATION_MODE_ \n#define _COMPILATION_MODE_ _COMPILATION_MODE__MPI_\n");
        `echo OPENMP=off >> Makefile.local`;
      }
      next;
    }
    else {
      die "Option is unrecognized: -openmpfgf=($1)";
    }
  }   

  if (/^-check_macro=(.*)$/i) {
    my $t;
    $t=lc($1); 

    if ( -e ".last_input_info") {
      `rm .last_input_info`;
    }

    if ($t eq "on") { 
      `echo CHECKMACRO=on >> Makefile.local`;
    }
    else {
      `echo CHECKMACRO=off >> Makefile.local`;
    }

    next;
  }

  if (/^-prefix=(.*)$/i) {
    my $t;
    $t=lc($1);

    if ( -e ".last_input_info") {
      `rm .last_input_info`;
    }

    if ($t eq "on") {
       add_line_amps_conf("PREFIXMODE=on");
    }
    else {
       add_line_amps_conf("PREFIXMODE=off"); 
    }

    next;
  }
      
  if (/^-fexit=(.*)$/i) {
    my $t;
    $t=lc($1);
 #  add_line_amps_conf("OPENMP=$1");

    if ($t eq "exit") {
      if (! check_macro_general_conf("_GENERIC_EXIT_FUNCTION_MODE_","_GENERIC_EXIT_FUNCTION__EXIT_")) {
        add_line_general_conf("#undef _GENERIC_EXIT_FUNCTION_MODE_ \n#define _GENERIC_EXIT_FUNCTION_MODE_ _GENERIC_EXIT_FUNCTION__EXIT_\n");
      }

      next;
    }
    elsif ($t eq "mpi_abort") {
      if (! check_macro_general_conf("_GENERIC_EXIT_FUNCTION_MODE_","_GENERIC_EXIT_FUNCTION__MPI_ABORT_")) {
        add_line_general_conf("#undef _GENERIC_EXIT_FUNCTION_MODE_ \n#define _GENERIC_EXIT_FUNCTION_MODE_ _GENERIC_EXIT_FUNCTION__MPI_ABORT_\n");
      }

      next;
    }
    else {
      die "Option is unrecognized: -openmpfgf=($1)";
    }
  }     

  
  if (/^-batl-path=(.*)$/i)       {
      add_line_amps_conf("BATL=$1");
      next};
  if (/^-swmf-path=(.*)$/i)       {
      add_line_amps_conf("SWMF=$1");
      next};  
  
  if (/^-cplr-data-path=(.*)$/i)       {
      add_line_amps_conf("CPLRDATA=$1");
      next};  #path to the data files used in the PIC::CPLR::DATAFILE file readers
  if (/^-model-data-path=(.*)$/i)  {
      add_line_amps_conf("MODELINPUTDATA=$1");
      next}; #the path to the data file used by the user direactly (not through the PIC::CPLR::DATAFILE readers)

  if (/^-output-path=(.*)$/i) {
      add_line_amps_conf("OUTPUT=$1");
      next};  #the directory where AMPS' output files will be located 

  #compile for the nightly test
  if (/^-amps-test=(.*)$/i) {
    my $t;

    $t=lc($1);

    if (!check_amps_conf("TESTMODE=$t")) {
      add_line_amps_conf("TESTMODE=$t");
      `echo TESTMODE=$t >> Makefile.local`;
    }

    next;
  }; 

  #regenerate Makefile.input with a number of the test execution blocks proveded to ./Config.pl 
  if (/^-test-blocks=(.*)$/i) {
    `g++ utility/TestScripts/table_parser.cpp -o table_parser`;
    `cp MakefileTest/Table .`;
    `table_parser $1`;
    `rm Table`;
    next;
  }

  #set the APPEND flog to append .amps.conf instead of re-defining of AMPS' settings
  if (/^-append$/i) { 
    `echo APPEND >> .amps.conf`;
    next;
  }

  # set nightly test:
  #  -set-name=NAME/comp - tests' name NAME cannot be empty
  #  -set-name/comp      - tests' name will be `hostname -s`
  if (/^-set-test=(.*)$/i) {
    die "ERROR: test name is empty!" unless ($1);
    my $CompilersRaw;
   
    ($TestName, $CompilersRaw) = split("\/",$1,2);
    die "ERROR: no compiler is indicated for tests!" unless ($CompilersRaw);
    @Compilers = split (',',lc $CompilersRaw);
    require "./utility/TestScripts/SetNightlyTest.pl";
    next;
  } 

  if (/^-set-test\/(.*)$/i) {
    die "ERROR: no compiler is indicated for tests!" unless ($1);
    $TestName=''; 
    @Compilers = split (',',lc $1);
    require "./utility/TestScripts/SetNightlyTest.pl";     
    next;
  } 

  if (/^-rm-test$/i) {
    require "./utility/TestScripts/RemoveNightlyTest.pl";     
    next;
  } 

  if (/^-cuda$/i) {
    add_line_general_conf("#undef _TARGET_GLOBAL_ \n#define _TARGET_GLOBAL_ __global__");
    add_line_general_conf("#undef _TARGET_HOST_ \n#define _TARGET_HOST_ __host__");
    add_line_general_conf("#undef _TARGET_DEVICE_ \n#define _TARGET_DEVICE_ __device__");
    add_line_general_conf("#undef _CUDA_MODE_ \n#define _CUDA_MODE_ _ON_");
    add_line_general_conf("#undef _CUDA_MANAGED_\n#define _CUDA_MANAGED_ __managed__"); 
    add_line_general_conf("#undef _CUDA_SHARED_\n#define _CUDA_SHARED_ __shared__");

    #add stuff in Makefile.conf
    open (MAKEFILE,">>Makefile.conf") || die "Cannot open Makefile.local\n";
    print MAKEFILE ".SUFFIXES: .cu\n.cu.o:\n\tnvcc \${FLAGCC} \$< -o \$@";
    close (MAKEFILE);

    next;
  }

  if (/^-no-signals/i) {
    add_line_general_conf("#undef _INTERSEPT_OS_SIGNALS_ \n#define _INTERSEPT_OS_SIGNALS_ _OFF_"); 
    next;
  }

  if (/^-no-avx-matmul/i) {
    add_line_general_conf("#undef _AVX_MATMUL_ \n#define _AVX_MATMUL_ _OFF_");
    next;
  } 

  if (/^-no-avx-mover/i) {
    add_line_general_conf("#undef _AVX_PARTICLE_MOVER_ \n#define _AVX_PARTICLE_MOVER_ _OFF_");
    next;
  }

  if (/^-state-vector=(.*)$/i) {
    my $t;
    $t=lc($1);

    if ($t eq "aligned") {
      add_line_amps_conf("STATE_VECTOR=ALIGNED");
    }
    elsif ($t eq "packed") { 
      add_line_amps_conf("STATE_VECTOR=PACKED");
    }
    else {
      die "Unknoen option of -state-vector";
    }

    next;
  }

  if (/^-align=(.*)$/i) {
    my $t;
    $t=lc($1);

    if ($t eq "on") {
      add_line_amps_conf("ALIGN_STATE_VECTORS=ON");
      add_line_amps_conf("ALIGN_STATE_VECTORS_BASE=8"); 
    }
    else {
      add_line_amps_conf("ALIGN_STATE_VECTORS=OFF");
    }

    next;
  }


  if (/^-align-base=(.*)$/i) {
    my $t;
    $t=lc($1);

    add_line_amps_conf("ALIGN_STATE_VECTORS=ON");
    add_line_amps_conf("ALIGN_STATE_VECTORS_BASE=".$t);
    next;
  }

  if (/^-avx=(.*)$/) {
    my $t;

    $t=lc($1);
        
    if ($t eq "256") {
      if (! check_macro_general_conf("_AVX_INSTRUCTIONS_USAGE_MODE_", "_AVX_INSTRUCTIONS_USAGE_MODE__256_")) {  
        add_line_general_conf("#undef _AVX_INSTRUCTIONS_USAGE_MODE_ \n#define _AVX_INSTRUCTIONS_USAGE_MODE_ _AVX_INSTRUCTIONS_USAGE_MODE__256_");
        add_line_general_conf("#undef _AVX_INSTRUCTIONS_USAGE_MODE__ON_ \n#define _AVX_INSTRUCTIONS_USAGE_MODE__ON_ _AVX_INSTRUCTIONS_USAGE_MODE__256_");

        `echo AVXMODE=on >> Makefile.local`;
        `echo AVXTYPE=256 >> Makefile.local`;
      }
    }
    elsif ($t eq "512") {
      if (! check_macro_general_conf("_AVX_INSTRUCTIONS_USAGE_MODE_","_AVX_INSTRUCTIONS_USAGE_MODE__512_")) {
        add_line_general_conf("#undef _AVX_INSTRUCTIONS_USAGE_MODE_ \n#define _AVX_INSTRUCTIONS_USAGE_MODE_ _AVX_INSTRUCTIONS_USAGE_MODE__512_");
        add_line_general_conf("#undef _AVX_INSTRUCTIONS_USAGE_MODE__ON_ \n#define _AVX_INSTRUCTIONS_USAGE_MODE__ON_ _AVX_INSTRUCTIONS_USAGE_MODE__512_"); 

        `echo AVXMODE=on >> Makefile.local`;
        `echo AVXTYPE=512 >> Makefile.local`;
      }
    }
    elsif ($t eq "off") {
      if (! check_macro_general_conf("_AVX_INSTRUCTIONS_USAGE_MODE_","_AVX_INSTRUCTIONS_USAGE_MODE__OFF_")) {
        add_line_general_conf("#undef _AVX_INSTRUCTIONS_USAGE_MODE_ \n#define _AVX_INSTRUCTIONS_USAGE_MODE_ _AVX_INSTRUCTIONS_USAGE_MODE__OFF_");
        `echo AVXMODE=off >> Makefile.local`;
      }
    }
    else {
      die "Option is not recognized: -avx=($1)";
    }
    
    next;
  }

  if (/^-link-option=(.*)$/) {
    my $options=$1; 
    $options =~ s/:/ /g;
    
    my $fh;
    open($fh,'<',"Makefile"); 
    my @lines = <$fh>; 
    close($fh);
    
    open($fh,'>',"Makefile");
      
    foreach my $line (@lines) {
      if($line =~ m/^EXTRALINKEROPTIONS=/) {
        $line = "EXTRALINKEROPTIONS=$options\n";
      }
      
      print $fh $line;
    }
      
    close($fh);
    next;
  }
  
  if (/^-f-link-option=(.*)$/) {
    my $options=$1; 
    $options =~ s/:/ /g;
    
    my $fh;
    open($fh,'<',"Makefile"); 
    my @lines = <$fh>; 
    close($fh);
    
    open($fh,'>',"Makefile");
      
    foreach my $line (@lines) {
      if ($line =~ m/^EXTRALINKEROPTIONS_F=/) {
        $line = "EXTRALINKEROPTIONS_F=$options\n";
      }
	      
      print $fh $line;
    }
      
    close($fh);
    next;
  }
  
  if (/^-cpp-link-option=(.*)$/) {
    my $options=$1; 
    $options =~ s/:/ /g;
    
    my $fh;
    open($fh,'<',"Makefile"); 
    my @lines = <$fh>; 
    close($fh);
    
    open($fh,'>',"Makefile");
      
    foreach my $line (@lines) {
      if ($line =~ m/^EXTRALINKEROPTIONS_CPP=/) {
         $line = "EXTRALINKEROPTIONS_CPP=$options\n";
      }
	      
      print $fh $line;
    }
      
    close($fh);
    next;
  }
  
  if (/^-compiler-option=(.*)$/) {
    my $options=$1; 

    $options =~ s/:/ /g;
    `echo EXTRACOMPILEROPTIONS+=$options >> Makefile.local`;
    next;
  }
  
  if (/^-cpp-compiler=(.*)$/) {
    my $options=$1; 

    #replace COMPILE.mpicxx variable in Makefile.conf    
    `mv Makefile.conf Makefile.conf.bak`;
    
    open(fMakefileIn,"<Makefile.conf.bak") || die "Cannot open Makefile.conf.bak";
    open(fMakefileOut,">Makefile.conf") || die "Cannot open Makefile.conf";
    
    my $line;
    while ($line=<fMakefileIn>) {
      if ($line=~m/COMPILE.mpicxx=/) {
        print fMakefileOut "COMPILE.mpicxx = $options\n";
      }
      else {
        print fMakefileOut "$line";
      }
    }
    
    close(fMakefileIn);
    close(fMakefileOut);
    next;
  }  

  if (/^-cpplib-rm=(.*)$/) {
    my $options=$1;

    #remove library 'options' from the list defined by variable CPPLIB in Makefile.conf 
    `mv Makefile.conf Makefile.conf.bak`;

    open(fMakefileIn,"<Makefile.conf.bak") || die "Cannot open Makefile.conf.bak";
    open(fMakefileOut,">Makefile.conf") || die "Cannot open Makefile.conf";

    my $Opt;
    my @OptList;

    $options=~s/:/ /g;
    @OptList=split(' ',$options);

    my $line;
    while ($line=<fMakefileIn>) {
      if ($line=~m/CPPLIB =/) {
        #remove the library defiend by options
        foreach $Opt (@OptList) { 
          $line=~s/$options/ /;
        }
      }

      print fMakefileOut "$line";
    }

    close(fMakefileIn);
    close(fMakefileOut);
    next;
  }

  
  warn "WARNING: Unknown flag $_\n" if $Remaining{$_};
}


exit 0;

#=============================== Add a line to .general.conf
sub add_line_general_conf {
  #create .general.conf if not exists
  `touch .general.conf` unless (-e ".general.conf");
  
  open (SETTINGS,">>",".general.conf") || die "Cannot open .general.conf\n";
  print SETTINGS "$_[0]\n";
  close SETTINGS;
}

sub check_macro_general_conf {
  my $found=undef;
  my $Macro=$_[0];
  my $Value=$_[1];
  my $line;

  open (SETTINGS,"<",".general.conf") || return undef; 

  while ($line=<SETTINGS>) {
    if ($line =~ m/\b$Macro\b/i) {  
      if ($line =~ m/\b$Value\b/i) {
        $found="true";
      }
      else {
        $found=undef;
      }
    }
  }

  close SETTINGS;
  return $found;
}


#=============================== Verify that the parametes is already in .amps.conf
sub check_amps_conf {
  my ($Param, $Value) = split(/=/,$_[0]);
  my $found=undef;
  my $line;

  open (SETTINGS,"<",".amps.conf") || return undef;

  while ($line=<SETTINGS>) {
    chomp($line);

    if ($line =~ m/\b$Param\b/) {
      if ($line =~ m/"$Param=$Value"$/) {
        $found="true";
      }
      else {
        $found=undef;
      }
    }
  }

  close SETTINGS;
  return $found;
} 
 

#=============================== Add a line to .amps.conf
# USAGE:
# add_line_amps_conf($Newline)
#  $Newline has a form 'PARAMETER=value'
# if PARAMETER has been defined before, it is OVERWRITTEN with the new value
sub add_line_amps_conf{
    #check whether the parameter is already set 
    if (check_amps_conf($_[0])) {
      return;
    } 

    # create Makefile.local if it doesn't exist
    `touch .amps.conf` unless (-e ".amps.conf");
    
    # check that there are exactly 2 arguments
    my $nArg = @_;
    die "ERROR: add_line_amps_conf takes exactly 1 argument" 
	unless ($nArg==1);

    # get the name of parameter being defined
    my $Parameter = '';
    if($_[0] =~ m/(.*?)=(.*)/){
	$Parameter = $1;
    }
    else{die "Trying to add invalid line to .amps.conf: $_[0]\n";}

    # trim the parameter name as well as newline symbol at the end
    $Parameter =~ s/^\s+//; $Parameter =~ s/\s+\n*$//;

    # read the current content
    open (SETTINGS,"<",".amps.conf") || 
	die "Cannot open .amps.conf\n";
    my @settingslines=<SETTINGS>;
    close(SETTINGS);

    #check if the APPEND keywork is present in the settings
    my $AppendFlag=0;

    foreach (@settingslines) {
      if ($_ =~ /APPEND/){
        $AppendFlag=1;
        last;
      }
    }

    # check if this parameter has already been defined
    my $IsPresent = 0;

    if ($AppendFlag == 0) {
      foreach (@settingslines) {
        if ($_ =~ /$Parameter\s*=.*/){
          $_ = $_[0]; chomp($_); $_ = "$_\n";
          $IsPresent = 1;
          last;
        }
      }
    }
    
    # if the parameter hasn't been defined before, add its definition
    unless ($IsPresent) {
	# temporary variable for storing a line to be printed
	my $tmp; $tmp = $_[0]; chomp($tmp);# $tmp = "$tmp\n";
	push(@settingslines,"$tmp\n") 
    }

    
    # write changed content of .amps.conf
    open (SETTINGS,">",".amps.conf");   
    print SETTINGS  @settingslines;
    close (SETTINGS);
}
