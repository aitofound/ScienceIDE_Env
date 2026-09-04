#!/usr/bin/perl -s
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

my $Help          = ($h or $H or $help);
my $Verbose       = ($v or $verbose);
my $Gzip          = ($g or $gzip);
my $Repeat        = ($r or $repeat);
my $Stop          = ($s or $stop or 2);
my $Concat        = (($c or $cat) and not $Repeat);
my $MovieFlag; # for pIDL
$MovieFlag        = '-m' if ($m or $movie);
$MovieFlag        = '-M' if ($M or $MOVIE);
$MovieFlag        = '-t' if ($t or $tar);
$MovieFlag        = '-T' if ($T or $TAR);
my $KeepMovieOnly = ($M or $MOVIE);
my $MakeTar       = ($t or $tar or $T or $TAR);
my $KeepTarOnly   = ($T or $TAR);
my $nThread       = ($n or 4);
my $Rsync         = ($rsync or $sync);
my $Replace       = ($replace);
my $AllParam      = ($param or $allparam);
my $Pattern       = $p;
my $Format        = ($f or $format); $Format = "-f=$Format" if $Format;
my $NoPtec        = $noptec;
my $JuliaTec      = ($vtu or $vtk or $vtu or $VTU);
my $Link          = ($l or $link);
my $Quiet         = $q;

use strict;
use File::Find;

my $rsync = 'rsync -avzt';
my $exclude = " --exclude '*.idl' --exclude '*.tec' --exclude '*.dat'".
    " --exclude '*.[hHTS]'";

my $INFO    = "PostProc.pl";            # Info message string
my $WARNING = "WARNING in PostProc.pl"; # Warning message string
my $ERROR   = "ERROR in PostProc.pl";   # Error message string
my $StopFile = "PostProc.STOP";         # Stop repeat if this file is present
my $FinalRepeat;                        # True for the final repaat

my $ParamIn     = "PARAM.in";
my $ParamInOrig = "PARAM.in_orig_";
my $RunLog      = "runlog runlog_[0-9]*";


my $NameOutput;
if(@ARGV){
    die "$ERROR: option -r(epeat) cannot be combined with output directory!\n"
	if $Repeat;
    die "$ERROR: only one directory name can be given!\n" 
	unless $#ARGV == 0;
    $NameOutput = $ARGV[0];
    if(-e $NameOutput){
	if($Replace and $NameOutput !~ /^\./){
	    `/bin/rm -rf $NameOutput`;
	}else{
	    die "$ERROR: directory or file $NameOutput already exists!\n";
	}
    }
    `mkdir -p $NameOutput`;
    die "$ERROR: could not mkdir -p $NameOutput\n" if $?;
}

die "$ERROR: option -rsync requires a target directory: rsync=TARGETDIR\n"
    if $Rsync eq "1";

&print_help if $Help;

my $Pwd = `pwd`; chop $Pwd;

# Remove StopFile at the beginning.
unlink $StopFile;

# Set sleep option 
my $SleepFlag; 
$SleepFlag = '-s=10' if $Repeat;

# Name of the plot directories for various components
my %PlotDir = (
    "EE"     => "EE/IO2",
    "GM"     => "GM/IO2",
    "IE"     => "IE/ionosphere,IE/Output",
    "IH"     => "IH/IO2",
    "OH"     => "OH/IO2",
    "IM"     => "IM/plots,IM/output",
    "PW"     => "PW/plots",
    "PC"     => "PC/plots", 
    "PS"     => "PS/Output",
    "PT"     => "PT/plots",
    "RB"     => "RB/plots",
    "SC"     => "SC/IO2",
    "SP"     => "SP/IO2",
    "UA"     => "UA/Output,UA/data",
    "plots"  => "plots",
    "STDOUT" => "STDOUT",
	    );

# Flush output immediately
$| = 1;

if($Repeat){
    print "$INFO running on ", `hostname`;
    print "$INFO will stop in $Stop days after ", `date`;
    print "            or when 'touch $StopFile' is done in this directory.\n";
}

my $time_start = time();
REPEAT:{
    foreach my $Dir (sort keys %PlotDir){
	next unless -d $Dir;

	my $PlotDir = $PlotDir{$Dir};

	# Find the actual plot directory
	if($PlotDir =~ /,/){
	    my @PlotDir;
	    @PlotDir = split(/,/,$PlotDir);
	    foreach (@PlotDir){
		if(-d $_){
		    $PlotDir{$Dir} = $_;
		    $PlotDir       = $_;
		    last;
		}
	    }
	}

	warn "$WARNING: plot directory $PlotDir is missing\n" 
	    unless -d $PlotDir;
	next unless -d $PlotDir;

	print "cd $Dir\n" if $Verbose;
	chdir $Dir 
	    or die "$ERROR: could not change directory to $Dir\n";

	my $pIDL = "./pIDL $MovieFlag $SleepFlag -n=$nThread $Pattern $Format";
	$pIDL .= " -q" if $Quiet;

	# Post process files if necessary
	if($Dir eq "IE"){
	    if($Gzip){
		&shell("./pION -g -n=$nThread");
	    }else{
		&shell("./pION -n=$nThread");
	    }
            &concat_sat_log if $Concat;
	}elsif( $Dir =~ /^PC|PT$/ ){
	    &shell($pIDL);
	}elsif( $Dir eq "UA"){
	    if (-e "./post_process.py") {
		&shell("./pGITM -np $nThread");
	    }else{
		&shell("./pGITM");
	    }
	}elsif( $Dir =~ /^SC|IH|OH|GM|EE$/ ){
	    &shell($pIDL);
	    unless($NoPtec){
		if($Gzip){
		    &shell("./pTEC A g");
		}else{
		    if($JuliaTec){
			&shell("export JULIA_NUM_THREADS=$nThread; julia convert2VTK.jl");
		    }else{
			&shell("./pTEC A p r");
		    }
		}
	    }
            &concat_sat_log if $Concat;
	}elsif( $Dir eq "IM" ){
	    my @files=glob("plots/*.dat");
	    if($Gzip){
		&shell("gzip",@files) if @files;
	    }else{
		&shell("./Preplot.pl",@files) if @files;
	    }
	}elsif( $Dir eq "PS" ){
	    my @files=glob("Output/dgcpm*.dat");
	    if($Gzip){
		&shell("gzip", @files) if @files;
	    }
	}elsif( $Dir eq "PW" ){
	    # PWOM output files cannot be gzipped while code is running
	    # because it is appending to the files.
	    if($Gzip and not $Repeat){
		my @files=glob("plots/*.out");
		&shell("gzip", @files) if @files;
	    }
	}elsif( $Dir eq "RB" ){
	    if($Gzip){
		my @files=glob("plots/*.fls");
		&shell("gzip",@files) if @files;
	    }
	}elsif( $Dir eq "plots" ){
	    &shell("cd ..; $pIDL");
	}
	chdir $Pwd;
    }

    if($Rsync and not $NameOutput){
	my $Dir;
	foreach $Dir (keys %PlotDir){
	    my $PlotDir = $PlotDir{$Dir};
	    next unless -d $PlotDir;
	    my $command = $rsync;
	    $command .= $exclude if $Dir =~ /GM|SC|IH|OH|EE/;
	    &shell("$command $PlotDir/ $Rsync/$Dir") if -d $PlotDir;
	}
	&shell("$rsync $ParamIn $Rsync/")          if -f $ParamIn;
	&shell("$rsync PARAM.* $Rsync/")           if $AllParam;
	&shell("$rsync runlog $Rsync/")            if -f "runlog";
	&shell("$rsync runlog_[0-9]* $Rsync/")     if glob("runlog_[0-9]*");
	&shell("$rsync log.[0-9]* $Rsync/")        if glob("log.[0-9]*");
    }

    if($Repeat){
	if(-f $StopFile){
	    if(not $FinalRepeat){
		$FinalRepeat = 1;
		print "$INFO doing final post-processing since $StopFile file is present.\n";
		redo REPEAT;
	    }
	    print "$INFO stopping because $StopFile file is present.\n";
	    exit 0;
	}
	if((time - $time_start) > $Stop*3600*24){
	    print "$INFO stopping because already ran for $Stop days.\n";
	    exit 0;
	}
	sleep $Repeat;
	redo REPEAT;
    }
}

&read_runlog unless $Quiet;

# Done except for collecting output files
exit 0 unless $NameOutput;

# Collect plot directories into $NameOutput 
# and make empty plot directories if requested
foreach my $Dir (sort keys %PlotDir){
    next unless -d $Dir;
    my $PlotDir = $PlotDir{$Dir};
    next unless -d $PlotDir;

    # Check if the plot directory is empty
    my @Files;
    opendir(DIR, $PlotDir)
	or die "$ERROR: could not open directory $PlotDir!\n";
    @Files = readdir(DIR) 
	or die "$ERROR: could not read directory $PlotDir!\n";
    closedir(DIR);
    if($#Files > 1){
	print "$INFO: mv $PlotDir $NameOutput/$Dir with ",
	       $#Files-1," file"; print "s" if $#Files > 2; print "\n";
	rename $PlotDir, "$NameOutput/$Dir" or 
	    die "$ERROR: could not rename $PlotDir $NameOutput/$Dir\n";

	# Make sure that the directory is moved before trying to recreate it
	sleep 1;


	# Recreate an empty directory tree in $PlotDir
	# Store current directory so the recursive mkdir can work
	my $Pwd = `pwd`; chop $Pwd;
	find sub {return unless -d; 
		  $_ = $File::Find::name; return if /_amrex\b/;
		  s/$NameOutput\/$Dir/$PlotDir/; 
		  mkdir "$Pwd/$_", 0777 or warn "failed mkdir $Pwd/$_\n"}, 
	"$NameOutput/$Dir";

    }else{
	warn "$WARNING: no files were found in $PlotDir\n";
    }
}

# Copy and move some input and output files if present
if(-f $ParamIn){
    if($AllParam){
	&shell_info("cp PARAM.* $NameOutput");
    }else{
	&shell_info("cp $ParamIn $NameOutput");
	&shell_info("mv $ParamInOrig $NameOutput") if -f $ParamInOrig;
    }
}else{
    warn "$WARNING: no $ParamIn file was found\n";
}

if(-f "runlog"){
    &shell_info("mv runlog $NameOutput");
}elsif(glob("runlog_[0-9]*")){
    &shell_info("mv runlog_[0-9]* $NameOutput");
}else{
    warn "$WARNING: no $RunLog file was found\n";
}

&shell_info("./Restart.pl -o -W -l=$Link $NameOutput/RESTART");

if($Rsync){
    &shell_info("rsync -avzt $NameOutput/ $Rsync");
    print "$INFO: rsync is complete\n";
}

exit 0;

#############################################################
sub shell{
    my $command = join(" ",@_);
    print "$command\n" if $Verbose;
    my $result = `$command`;
    print $result if $Verbose or $result =~ /error/i;
}

#############################################################
sub shell_info{
    my $command = join(" ",@_);
    print "$INFO: $command\n";
    my $result = `$command`;
    print $result if $Verbose or $result =~ /error/i;
}

#############################################################

sub read_runlog{
    # Read runlog and print out init time and runtime without init time
    my $timeinit;
    my $timerun;
    my $runlogfile;
    foreach $runlogfile (glob("runlog*")){

	# Read first timing for initialization
	open(INPUT, $runlogfile) or die "Could not open $runlogfile: $!\n";
	while(<INPUT>){
	    if(/^(BATSRUS|SWMF)[^\d]+(\d+\.\d+).*/ or 
	       /^(BATSRUS|SWMF)\s*1\s*1\s+(.*?)\s+/){
	       $timeinit = $2;
	       last;
	    }   
	}
	close(INPUT);

	# Read last timing for total runtime
	open(INPUT, "tail -n 400 $runlogfile |");
	while(<INPUT>){
	    if(/^(BATSRUS|SWMF)[^\d]+(\d+\.\d+).*/ or 
	       /^(BATSRUS|SWMF)\s*1\s*1\s+(.*?)\s+/){
		$timerun = $2;
	    }
	}
	close(INPUT);

	print "$INFO: TIMINGS from $runlogfile (init, run)".
	    " $timeinit $timerun\n" if $timeinit or $timerun;
    }
}

##############################################################################

sub concat_sat_log{

    chdir "IO2" or chdir "ionosphere" or return;
    opendir(DIR,'.');
    my @LogSatFiles = sort(grep /\.(log|sat|mag)$/, readdir(DIR));
    closedir(DIR);

    # Concatenate the .log/.sat files with same name
    my %FirstFile;
    my $File;
    for $File (@LogSatFiles){
	my $BaseName = $File;

	# Remove extension
	$BaseName =~ s/_[ent][\d\-_]+\.(log|sat|mag)$// or
	    die "$ERROR: file name $File does not match "
	    .   "_[ent]TIMESTAMP.(log|sat|mag) format\n";

	# Check if there was another file with the same base name.
	my $FirstFile = $FirstFile{$BaseName};
	if(not $FirstFile){
	    $FirstFile{$BaseName} = $File;
	    next;
	}

	# Append this file's content (without the header) to the first file
	open (FIRST, ">>$FirstFile") or 
	    die "$ERROR: could not open first file $FirstFile for append\n";
	open (FILE, "$File") or 
	    die "$ERROR: could not file $FirstFile for read\n";
	while(<FILE>){
            # skip lines that contain other things than numbers
	    next unless /^[\s\d\.eEdD\+\-\*]+$/; 
	    print FIRST $_;
	}
	close(FIRST);
	close(FILE);
	unlink $File;
    }
}

##############################################################################
#!QUOTE: \clearpage
#BOP
#!QUOTE: \subsection{Post-Process Plot Files with PostProc.pl}
#!ROUTINE: PostProc.pl - post-process plot files of the components
#!DESCRIPTION:
# This script is copied into the run directory and it should be executed there.
# The script post processes the plot files created by the components.
# The script can run in the background and post process periodically.
# It can also collect the plot files, the restart files, the standard output, 
# the runlog files and the PARAM.in file into a single 'output directory tree'.
# The output can also be rsync-ed to a remote machine.
#
#!REVISION HISTORY:
# 02/12/2005 G. Toth - initial version
# 05/08/2005           added -o option to collect output into a directory tree
# 09/08/2005           for -o option copy PARAM.in and move runlog into tree.
# 2008                 move last restart files into the tree.
# 2008                 for -c option concatenate log and satellite files.
# 02/04/2009 R. Oran   added OH component, same as IH
#EOP

sub print_help{
    print
#BOC
'Purpose:

   Post-process the output files and/or collect them into an output tree.
   The PARAM.in, runlog and restart files (if present) 
   are also copied/moved into the output tree. 
   The processed files or the output tree can be rsync-ed to another machine.

Usage:

   PostProc.pl [-h] [-v] [-c] [-g] [-m | -M | -t | -T] [-noptec] [-vtu]
               [-n=NTHREAD] [-p=PATTERN] [-f=FORM]
               [-r=REPEAT [-s=STOP] | DIR [-l=COMP] ] 
               [-rsync=TARGET] [-allparam]
 
   -h -help    Print help message and exit.

   -v -verbose Print verbose information.

   -c -cat     Concatenate series of satellite, log and magnetometer output
               files into one file. Cannot be used with the -r(epeat) option

   -f=FORM     - overwrite the output format for pIDL with FORM that has the 
   -format=FORM  following options: ascii, real4, real8, tec

   -g -gzip    Gzip the big ASCII files.

   -m -movie   Create movies (.outs) from series of IDL files (.out) 
               and keep series.

   -M -MOVIE   Create movies from series of IDL files and remove IDL files.

   -t -tar     Create tar files (.out.tar) from series of .out files 
               and keep series.

   -T -TAR     Create tar files from series of IDL files and remove IDL files.

   -noptec     Do not process Tecplot files with the pTEC script. 

   -n=NTHREAD  Run pIDL in parallel using NTHREAD threads. The default is 4.

   -p=PATTERN  Pass pattern to pIDL so it only processes the files that match.

   -q          Quiet mode. Do not print out speeds from runlog file.

   -r=REPEAT   Repeat post processing every REPEAT seconds.
               Cannot be used with the DIR argument. The script will stop
               after a certain number of days or when the 
               '.$StopFile.' file is present (touched).

   -replace    Replace the directory DIR if it already exists. By default the
	       code stops with an error message if DIR would be overwritten.

   -s=STOP     Exit from the script after STOP days. Useful when the script
               is run in the background with repeat flag. Default is 2 days.

   -vtu -vtk   Concatenate unstructured 3D outputs to *.dat and convert 
               to VTK format (.vtu) using Julia.

   -allparam   Copy and/or rsync all PARAM.* files.

   -rsync=TARGET Copy processed plot files into an other directory 
               (possibly on another machine) using rsync. The TARGET
               is the name of the target directory (with host machine). 
               When -rsync is used without the output direcectory DIR, 
               the original plot directories are synchronized. 
               When -rsync is used with the output directory DIR,
               then the output directory is synchronized.
               rsync must be installed on the local and target machines,
               and no password should be required to execute rsync.

   -l=COMP     Pass -l=COMP argument to Restart.pl. This can be useful when
   -link=COMP  creating the RESTART tree inside the output tree DIR.

   DIR         Name of the directory tree to collect the processed files in.
               Cannot be used with the -r option. The directory is created
               and it should be new (to avoid overwriting older results).
               By default the processed data is not collected.

Examples:

   Post-process the plot files:

PostProc.pl

   Post-process the .idl output files into Tecplot format
   and do not process the .tec Tecplot files with pTEC:

PostProc.pl -noptec -f=tec

   Post-process the plot files, create movies from IDL output,
   concatenate satellite, log, and magnetometer files, move output into a 
   directory tree "RESULTS/run23" even if it is already there, 
   run PostIDL.exe on 8 cores, and link RESULTS/run23/RESTART/IH 
   to the current IH input restart directory if IH/restartOUT is empty:

PostProc.pl -M -cat -n=8 -l=IH -replace RESULTS/run23

   Post-process the plot files, compress the ASCII files, rsync the results
   and all PARAM.* files to another machine and print verbose info:

PostProc.pl -g -allparam -rsync=ME@OTHERMACHINE:My/Results -v

   Post-process the .tec files, convert to unstructured VTK format:

PostProc.pl -vtk

   Repeat post-processing every 360 seconds for files matching "IO2/x=",
   pipe standard output and error into a log file and stop after 3 days:

PostProc.pl -r=360 -s=3 -p=IO2/x= >& PostProc.log &

   Repeat post-processing every minute, collect .out files into .out.tar
   files and stop after the SWMF.exe run is finished:

PostProc.pl -r=60 -T -n=16 >& PostProc.log &
mpiexec SWMF.exe
touch '.$StopFile.'

   Collect processed output into a directory tree named OUTPUT/New
   and rsync it together with all the PARAM.* files 
   into the run/OUTPUT/New directory on another machine:

PostProc.pl -allparam -rsync=ME@OTHERMACHINE:run/OUTPUT/New OUTPUT/New'

#EOC
    ,"\n\n";
    exit;
}
##############################################################################

