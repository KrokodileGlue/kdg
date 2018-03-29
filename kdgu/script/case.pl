#!/usr/bin/env perl

use strict;
use warnings;

open(my $fh, '<:encoding(UTF-8)', $ARGV[0])
    or die "Could not open '$ARGV[0]': $!";

print "#include \"kdgu.h\"\n";
print "struct kdgu_case special_case[] = {\n";
my $num = 0;

while (my $l = <$fh>) {
    chomp $l;

    if ($l =~ /^([^#]*?); (.*?); (.*?); (.*?); (?:(.*?); )?# (.*)$/) {
	my $code = $1;
	my $lower = $2;
	my $title = $3;
	my $upper = $4;
	my $condition = $5;
	my $comment = $6;

	print "	{ 0x$code, ";

	print scalar(split(/ /, $lower)), ", { ";
	print join(', ', map { "0x" . $_ } split(/ /, $lower));
	if ($lower eq "") {
	    print "0";
	}
	print " }, ";

	print scalar(split(/ /, $title)), ", { ";
	print join(', ', map { "0x" . $_ } split(/ /, $title));
	if ($title eq "") {
	    print "0";
	}
	print " }, ";

	print scalar(split(/ /, $upper)), ", { ";
	print join(', ', map { "0x" . $_ } split(/ /, $upper));
	if ($upper eq "") {
	    print "0";
	}
	print " }";

	print " }, /* $6 */\n";
	$num++;
    }
}

print "};\n";
print "size_t num_special_case = $num;\n";
