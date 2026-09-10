#!/usr/bin/env perl
use strict;
use warnings;
use File::Find;
use File::Basename;
use File::Spec;
use Cwd;
use Getopt::Long;

# Configuration
my $BASE_DIR = File::Spec->catdir(getcwd(), "AMPS_TEST_RESULTS");  # Directory to scan in current directory
my $OUTPUT_FILE = "history.html";                                 # Output HTML file

# Display Help Function
sub display_help {
    print <<'HELP';
Usage: search_amps_test.pl [OPTIONS]

This script scans the AMPS_TEST_RESULTS directory (assumed to be in the current working directory)
to generate a history.html file and optionally search for keywords in "test_amps.html" files.

Options:
  -h, -help          Display this help message and exit.
  -k, --keyword      Specify the keyword to search for in "test_amps.html" files.
                     If not provided, the script will prompt for a keyword.
  -d, --diff-only    Print out matching results that contain both the specified keyword
                     and the word ".diff".

Behavior:
  - If no options are provided, the script will prompt you for a keyword.
  - Press Enter without entering a keyword to generate history.html without searching.

Outputs:
  1. history.html: Contains a directory structure of AMPS_TEST_RESULTS and search results.
  2. Console Output: If --diff-only is used, prints matching lines containing ".diff".

Examples:
  1. Run without options (prompts for a keyword):
     ./search_amps_test.pl

  2. Generate history.html and search for a keyword:
     ./search_amps_test.pl --keyword test

  3. Generate history.html, search for a keyword, and print results containing ".diff":
     ./search_amps_test.pl --keyword test --diff-only

HELP
    exit;
}

# Parse Command-Line Arguments
my $help;
my $keyword;
my $diff_only;

GetOptions(
    'h|help'     => \$help,
    'k|keyword=s' => \$keyword,
    'd|diff-only' => \$diff_only,
) or die "Invalid options passed. Use -h or -help for usage details.\n";

# Display help if requested
if ($help) {
    display_help();
}

# Prompt for a keyword if none was provided and no other options were given
if (!$keyword && !$help && !$diff_only) {
    print "Enter a search keyword (or press Enter to skip): ";
    chomp($keyword = <STDIN>);
}

# Functions
sub build_hierarchy {
    my ($base_dir) = @_;
    my $content = "";

    find(sub {
        return unless -f $_;
        return unless $_ eq 'test_amps.html';

        my $relative_path = File::Spec->abs2rel($File::Find::name, $base_dir);
        my $dir_name = basename(dirname($File::Find::name));
        $content .= qq(<li><a href="$relative_path">test_amps.html in $dir_name</a></li>\n);
    }, $base_dir);

    return $content ? "<ul>$content</ul>" : "";
}

sub search_files {
    my ($base_dir, $search_key) = @_;
    my @results;
    my @filtered_results;

    find(sub {
        return unless -f $_;
        return unless $_ eq 'test_amps.html';

        my $file_path = $File::Find::name;
        my $relative_path = File::Spec->abs2rel($file_path, $base_dir);
        my $stream_name = basename(dirname($file_path));
        my @time_path = (split('/', $relative_path))[0 .. 2];

        open my $fh, '<', $file_path or die "Could not open '$file_path': $!";
        while (<$fh>) {
            if (/$search_key/i) {
                my $result = {
                    time   => join('/', @time_path),
                    stream => $stream_name,
                    line   => $_,
                };
                push @results, $result;

                # Check for the additional condition of containing '.diff'
                if (/\.diff/i) {
                    push @filtered_results, $result;
                }
            }
        }
        close $fh;
    }, $base_dir);

    return (\@results, \@filtered_results);
}

sub generate_html {
    my ($output_file, $hierarchy, $search_results, $search_key) = @_;

    my $html = <<'HTML_HEADER';
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>AMPS Test Results</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 20px; }
        details { margin-bottom: 10px; }
        summary { font-weight: bold; cursor: pointer; }
        ul { list-style: none; padding-left: 20px; }
        li { margin: 5px 0; }
        a { text-decoration: none; color: #007bff; }
        a:hover { text-decoration: underline; }
        table { border-collapse: collapse; width: 100%; margin-top: 20px; }
        th, td { border: 1px solid #ddd; padding: 8px; }
        th { background-color: #f4f4f4; }
    </style>
</head>
<body>
    <h1>AMPS Test Results</h1>
    <form method="GET" action="">
        <label for="search">Search for keyword:</label>
        <input type="text" id="search" name="search" />
        <button type="submit">Search</button>
    </form>
HTML_HEADER

    if ($search_results && @$search_results) {
        $html .= "<h2>Search Results for '$search_key'</h2>\n";
        $html .= "<table>\n<thead>\n<tr><th>Time</th><th>Test Stream</th><th>Matching Line</th></tr>\n</thead>\n<tbody>\n";
        for my $result (@$search_results) {
            $html .= "<tr><td>$result->{time}</td><td>$result->{stream}</td><td>$result->{line}</td></tr>\n";
        }
        $html .= "</tbody>\n</table>\n";
    }

    $html .= qq(<div id="hierarchy">\n$hierarchy\n</div>\n</body>\n</html>);

    open my $fh, '>', $output_file or die "Could not write to '$output_file': $!";
    print $fh $html;
    close $fh;
}

# Main Execution
my $hierarchy = build_hierarchy($BASE_DIR);
my ($search_results, $filtered_results) = $keyword ? search_files($BASE_DIR, $keyword) : (undef, undef);

generate_html($OUTPUT_FILE, $hierarchy, $search_results, $keyword);

print "HTML file generated: $OUTPUT_FILE\n";
if ($search_results) {
    print "Found " . scalar(@$search_results) . " results for keyword: $keyword\n";
}

if ($diff_only && $filtered_results && @$filtered_results) {
    print "\nFiltered results containing '.diff':\n";
    for my $result (@$filtered_results) {
        print "Time: $result->{time}, Stream: $result->{stream}, Line: $result->{line}";
    }
}

