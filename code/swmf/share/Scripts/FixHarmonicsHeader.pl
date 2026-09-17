#!/usr/bin/perl -ni

# get order and Carrington rotation from header
$order = $1 if /order\s*[:=]\s*(\d+)/i;
$cr = $1 if /CT(\d+)/;

# print header
if(/^\s+l\s+m\s+g/){
    # total number of coefficients
    $n = ($order+1)*($order+2)/2; 
    print "$ARGV
   0    0.0    -2  3  2
   $n    1
   $order    $cr    180.0
n m g h nOrder CR LonCentral
";
    $start = 1;
}

# pring lines with numbers only
print if ($start and /^\s+\d+\s+\d[\s\d\+\-\.]+$/);
