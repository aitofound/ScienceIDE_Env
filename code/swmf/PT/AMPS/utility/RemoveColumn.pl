#!/usr/bin/perl -s -i.tmp
# Remvove the nth column from a file. Copy the original file into FILE~.  
# Usage: RemoveColumn.pl -c=2,4,5 FILE

@c = split(/,/, $c);
while(<>){
    chop;
    @a = split;
    foreach $e (reverse sort @c){
	splice(@a,$e-1,1);
    }
    print "@a\n";
}
