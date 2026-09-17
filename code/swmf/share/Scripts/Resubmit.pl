#!/usr/bin/perl -s

my $jobfile = $ARGV[0];

if(not $jobfile or $h or $help){
    print "
This script should be run from the run directory in the background.
It will submit a job and then resubmit it agiain if it finishes 
successfully but not fully done, because it ran out of CPU time.
The #CPUTIMEMAX command has to be present in the PARAM.in files. 
The Restart.pl script will be run first to link restart files.
If the PARAM.in.restart file is present, then PARAM.in will be moved 
into PARAM.in.start and PARAM.in.restart will be copied into PARAM.in.
The job will be resubmitted until the final time is reached,
indicated by the *.DONE file.

Usage:
   Resubmit.pl [-s=SLEEP] JOBFILE >& LOGFILE &

The JOBFILE is job script to be submitted. The LOGFILE 
provides a log of the (re)submissions of the job.

-s=SLEEP   - The optional -s=SLEEP argument sets the time 
             between checks in seconds. The default is SLEEP=10 seconds. 

Example:
    Resubmit.pl -s=10 job.frontera >& Resubmit.log &

";
    exit 0;
}

my $sleep = ($s or 10);

use strict;

die "$jobfile does not exist\n" unless -f $jobfile;
my $pbs = `grep #PBS $jobfile`;
my $slurm = `grep #SBATCH $jobfile`;

die "$jobfile does not have PBS or SBATCH directives\n"
    unless $pbs or $slurm;

my $command;
my $which;
if($pbs){
    $which = `which qsub`;
    die "qsub not found\n" unless $which =~ /qsub$/;
    $command = "qsub $jobfile";
}
if($slurm){
    $which = `which sbatch`;
    die "sbatch not found\n" unless $which =~ /sbatch$/;
    $command = "sbatch $jobfile";
}
    
my $Success1 = "SWMF.SUCCESS";
my $Success2 = "BATSRUS.SUCCESS";
my $Done1    = "SWMF.DONE";
my $Done2    = "BATSRUS.DONE";

unlink $Done1, $Done2;                    # remove stop indicators
while(not -f $Done1 and not -f $Done2){   # loop until done
    unlink $Success1, $Success2;          # remove success indicators
    `$command`;                           # submit job
    print "$command on ", `date`;         # report back
    sleep $sleep while not (-f $Success1 or -f $Success2); # wait for finish
    print "job finished successfully on ", `date`;
    sleep 60;                             # wait for the job to quit
    `./Restart.pl`;                       # process restart files
    if(-f "PARAM.in.restart" and not -f "PARAM.in.start"){
	`mv PARAM.in PARAM.in.start`;
	`cp PARAM.in.restart PARAM.in`;
    }
}
print "job reached final time on ", `date`;
exit 0;
