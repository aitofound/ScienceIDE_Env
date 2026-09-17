#!/usr/bin/perl
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

# Allow in-place editing                                                        
$^I = "";

# Add local directory to search                                                 
push @INC, ".";

use strict;
our @Arguments       = @ARGV;

# get util and share
require "../../share/Scripts/Config.pl";
