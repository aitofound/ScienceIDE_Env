#!/usr/bin/perl -s
#  Copyright (C) 2002 Regents of the University of Michigan, 
#  portions used with permission 
#  For more information, see http://csem.engin.umich.edu/tools/swmf

use strict;
use warnings;
use Config::IniFiles;

# Use the specified input file or default to "PARAM.in"
my $input_file = $ARGV[0] // 'PARAM.in';
# Generate the output file name by replacing ".in" with ".ini"
my $output_file = $input_file;
$output_file =~ s/\.in(\..*)$/.ini$1/;

# Create a new Config::IniFiles object
my $cfg = Config::IniFiles->new();

# Read the input file
open my $fh, '<', $input_file or die "Could not open '$input_file' $!";
# Convert command blocks
my $section;
my %key_count; # Hash to keep track of key occurrences within a section
while (my $line = <$fh>) {
    chomp $line;

    # Stop conversion on "END"
    last if $line =~ /^\s*#?END\s*$/;

    # Skip empty lines, marking the end of a section
    if ($line =~ /^\s*$/) {
        $section = undef;
        next;
    }

    # Skip the #RUN section
    next if defined $section && $section eq 'RUN';

    # Detect section names starting with #
    if ($line =~ /^#(\w+)/) {
        $section = $1;
        # Skip adding [END] section
        $cfg->AddSection($section) unless $section eq 'END';

        # Initialize key count for this section
        $key_count{$section} = {};
    } elsif (defined $section) {
        # Split keys and values by tabs (ctab) instead of spaces
        my ($value, $key) = split /\t+/, $line, 2;
        # Trim leading whitespace from the value
        $value =~ s/^\s+// if defined $value;

        # Assign the value and key to config
        if (defined $key && defined $value) {
            # Count occurrences of each key
            $key_count{$key}++;
            # Add suffix if key appears more than once
            if ($key_count{$key} > 1) {
                $key = $key . "_$key_count{$key}";
            }
            $cfg->newval($section, $key, $value);
        }
    }
}
close $fh;

# Open the output file
open my $out_fh, '>', $output_file or die "Could not open '$output_file' $!";
# Write to the config file with custom formatting
my $first_section = 1;
foreach my $section ($cfg->Sections) {
    # Skip writing content of/after #END and #RUN
    next if $section eq 'END' || $section eq 'RUN';

    # Only add a newline if this is not the first section
    unless ($first_section) {
        print $out_fh "\n";
    }

    print $out_fh "[$section]\n";
    foreach my $key ($cfg->Parameters($section)) {
        my $value = $cfg->val($section, $key);
        # Print key and value with spaces around '='
        print $out_fh "$key = $value\n";
    }
    $first_section = 0;
}
close $out_fh;

print "Configuration has been written to '$output_file'\n";
