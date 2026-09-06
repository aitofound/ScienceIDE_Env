#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

use strict;
use warnings;
use threads;
use File::Which;

my $HELP = 
"The scripts performs one of several actions over M-FLAMPA output such as\n".
"(1) combine output files for individual lines into a single file;\n".
"(2) split file produced by (1) into files for individual lines;\n".
"(3) preplot output files to be readily available for import to Tecplot;\n".
"actions may be performed in a multi-threaded fashion\n";

my $USAGE="Usage:\n".
" ./ProcessOutput.pl [-help] [-n=THREAD_NUMBER] [-action[=ACTION FOLDER]]\n\n".
"Options:\n".
" -h -help\tdisplay help message\n".
" -action\tname of the action to execute\n".
" -n\t\tnumber of threads to execute the action (default is 1)\n\n".
"If called without arguments, the error message is displayed\n\n".
"Examples:\n\nDisplay available actions:\n".
" ./ProcessOutput.pl -action\n".
"Combine files using 10 threads\n".
" ./ProcessOutput.pl -n=10 -action=combine FOLDER\n";

my $ACTIONS="Available actions:\n".
" combine\tcombine output files for individual lines into a single file\n".
" split\t\tsplit combined file into files for individual lines\n".
" preplot\tpreplot files to be readily available for import to Tecplot\n";

die "ERROR: called without arguments\n".('-'x80)."\n$USAGE" if(@ARGV == 0);

# number of threads to execute action
my $nThread=1;

# folder with output data
my $Folder='';

# name of the action
my $action='';

# process command lone arguments
foreach my $arg (@ARGV){
    if($arg =~ m/^-n(=(.*))?$/){
	die "ERROR: Provide a valid number of threads\n".('-'x80)."\n$USAGE"
	    unless($1 && length($2));
	$nThread = $2;
	die "ERROR: Invalid number of threads ($nThread)\n" 
	    unless($nThread =~m/\d+/);
	die "ERROR: Invalid number of threads ($nThread)\n" 
	    if($nThread <= 0);
    }
    elsif($arg =~ m/^(-h|-help)$/){
	print "Displaying help message, other arguments are ignored\n".
	    ('-'x80)."\n" if(@ARGV > 0);
	print "$HELP\n$USAGE\n$ACTIONS";
	exit;
    }
    elsif($arg =~ m/^-action(=(.*))?$/){
	unless($1){
	   print "Displaying available actions, other arguments are ignored\n".
	       ('-'x80)."\n" if(@ARGV > 0);
	   print "$ACTIONS"; 
	   exit;
	}
	$action = $2;
	die "ERROR: Unknown action '$action'\n".('-'x80)."\n$ACTIONS"
	    unless($action =~ m/^(combine|split|preplot)$/);
	# check that preplot is available
	if($action eq 'preplot'){
	    die "ERROR: preplot command is not available\n"
		unless(which('preplot'));
	}
    }
    else{
	$Folder = $arg;
	die "ERROR: Folder '$Folder' doesn't exist\n" unless (-d -e $Folder);
	# remove trailing '/'
	$Folder =~ s/\/*$//;
    }
}

# check that the action has been provided
die "ERROR: action has not been indicated\n".('-'x80)."\nUSAGE"unless($action);

# unless $Folder is a full path, append './' to beginning
$Folder = "./$Folder" unless (substr($Folder,0,1) eq '/');

my %Actions = (
    'combine' => \&combine,
    'split'   => \&split,
    'preplot' => \&preplot
    );

my %Mask = (
    'combine' => 'MH_data_\d{3}_\d{3}_(t\d{8}_n\d{6})\.dat',
    'split'   => 'MH_data_(t\d{8}_n\d{6})\.dat',
    'preplot' => 'MH_data_(t\d{8}_n\d{6})\.dat'
    );

my @Stamps;

# get the list of all timestamps
@Stamps = process_directory("./$Folder", $Mask{$action});
for(my $i=0; $i<@Stamps; $i++){
    $Stamps[$i] =~ s/^.*$Mask{$action}/$1/;
}
@Stamps = sort @Stamps;

# remove repeating stamps; since @Stamps is sorted, duplicates are adjacent
my $i_last = 0;
for(my $i=1; $i<@Stamps; $i++){
    $Stamps[++$i_last] = $Stamps[$i] 
	unless($Stamps[$i_last] eq $Stamps[$i]);
}

# remove trailing stamps
splice(@Stamps, $i_last+1, @Stamps-$i_last-1);

# use the initThreads subroutine to create an array of threads.
my @threads = initThreads();

# Loop through the array:
foreach(@threads){
    # Tell each thread to perform our 'doOperation()' subroutine.
    $_ = threads->create(\&doOperation);
}
 
# This tells the main program to keep running until all threads have finished.
foreach(@threads){
    $_->join();
}

exit;

#==============================================

sub initThreads{
    my @initThreads;
    for(my $i = 1;$i<=$nThread;$i++){
	push(@initThreads,$i);
    }
    return @initThreads;
}

#----------------------------------------------

sub combine{# args: $id
    # current thread id
    my $id = $_[0];

    my $TimeStamp;
    my $FileName;
    
    # file handles
    my $FH; my $fh;

    # go over the found timestamp and combine matching files
    for(my $iStamp=($id-1); $iStamp < @Stamps; $iStamp+=$nThread){
	$TimeStamp = $Stamps[$iStamp];
	print"Thread".sprintf("%3d",$id)." Combining TimeStamp = $TimeStamp\n";

	# find files matching time stamp
	$FileName = 'MH_data_\d{3}_\d{3}_'."$TimeStamp.dat";
	my @Files = process_directory("$Folder",$FileName); 
	@Files = sort @Files;
	next unless(@Files);

	# create new file
	$FileName = "MH_data_$TimeStamp.dat";
	open($FH, '>', "$Folder/$FileName") || die "Can't open $FileName\n";
	# go over list of found files
	my $PrintHeader='1';
	foreach $FileName (@Files){
	    open($fh, '<', $FileName) || die "Can't open $FileName\n";
	    my @lines = <$fh>;
	    close $fh;
	    if($PrintHeader) { 
		print $FH @lines[0..1]; 
		$PrintHeader='';
	    }
	    # determine latitude and longitude indices
	    $FileName =~ m/^$Folder\/MH_data_(\d{3})_(\d{3})_/;
	    my $iLon = sprintf("%03d",$1);
	    my $iLat = sprintf("%03d",$2);
	    $lines[2] =~ 
		s/STRUCTURED GRID/STRUCTURED GRID LON=$iLon LAT=$iLat/;
	    print $FH @lines[2..@lines-1];
	}
	close($FH)
    }
}

#----------------------------------------------

sub split{# args: $id
    # id of the current thread
    my $id = $_[0];
    
    my $TimeStamp;
    my $FileName;

    # file handles
    my $FH; my $fh;
    
    # go over found timestamps and split corresponding files
    for(my $iStamp=($id-1); $iStamp < @Stamps; $iStamp+=$nThread){
	$TimeStamp = $Stamps[$iStamp];
	print"Thread".sprintf("%3d",$id)." Splitting TimeStamp = $TimeStamp\n";
	
	# read the file's content
	$FileName = "$Folder/MH_data_$TimeStamp.dat";
	open($FH, '<', "$FileName") || die "Can't open $FileName\n";
	my @LINES = <$FH>;
	close  $FH;
	
	# loop over contents
	foreach my $LINE (@LINES[2..(@LINES-1)]){
	    # at the beginning of new zone - > open new file
	    if($LINE =~m/STRUCTURED GRID LON=(\d{3}) LAT=(\d{3})/){
		# new file's name
		$FileName = "$Folder/MH_data_$1_$2_$TimeStamp.dat";
		# close file if its open
		close $fh if($fh);
		open($fh, '>', $FileName) || die "Can't open $FileName\n";
		# print header first
		print $fh @LINES[0..1];
		# also, shorten the zone name
		$LINE =~s/STRUCTURED GRID LON=\d{3} LAT=\d{3}/STRUCTURED GRID/;
	    }
	    print $fh $LINE;
	}
	close $fh;
    }
}

#----------------------------------------------

sub preplot{# args: $id
    # id of the current thread
    my $id = $_[0];

    # create folder to store Tecplot temprorary files
    my @chars = ("A".."Z", "a".."z");
    my $dir;
    $dir= 'tmp';
    $dir .= $chars[rand @chars] for 1..8;
    while(-e $dir && -d $dir){
	$dir='tmp';
	$dir .= $chars[rand @chars] for 1..8;
    }
    system("mkdir $dir");

    print "Thread $id: created dir $dir\n";

    # get the list of files to preplot
    my @Files = process_directory("$Folder",'MH_data_t\d{8}_n\d{6}\.dat'); 

    if(@Files){
	# preplot files
	for(my $i=$id-1; $i<@Files; $i+=$nThread){
	    print "Thread $id: preplot $Files[$i]\n";
	    if(substr($Folder,0,1) eq '/'){
		system("cd $dir && preplot $Files[$i] >& /dev/null");
	    }
	    else{
		system("cd $dir && preplot ../$Files[$i] >& /dev/null");
	    }
	}
    }
    else {
	print "Thread $id: no files found to preplot\n";
    }

    # remove temprary folder
    system("rm -rf $dir");
}

#----------------------------------------------

sub doOperation{
    # Get the thread id. Allows each thread to be identified.
    my $id = threads->tid();

    # execute selected action
    $Actions{$action}($id);

    # Exit the thread
    threads->exit();
}

#----------------------------------------------

sub process_directory{
    # check correctness
    die("Trying to process file $_[0] as a folder!")
        unless (-d "$_[0]");

    die("Folder $_[0] doess not exist!")
        unless (-e "$_[0]");

    # copy the directory name without trailing '/'
    my $DirName = $_[0];
    $DirName =~ s/\/*$//;

    # move to the given directory
    opendir(my $DIR, "$DirName") or die $!;

    # storage for found files
    my @res = ();

    # collect all files
    while(my $file = readdir($DIR)){
        # skip folders
        next if(-d "$DirName/$file");
        # ignore temporary files
        next if($file =~ m/^.*~/ || $file =~ m/^#.*#$/);
        # if the file name matches the input mask -> add to the list
        if($file =~ m/$_[1]/){
            push @res, "$DirName/$file";
        }
    }
    # move out of the given directory
    closedir($DIR);
    # return found files
    return @res;
}
