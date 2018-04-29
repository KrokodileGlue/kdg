#!/usr/bin/env perl
# SPDX-License-Identifier: MIT

# This script generates `unicode_data.c' from the following files:
#
#     - UnicodeData.txt
#     - SpecialCasing.txt
#     - DerivedCoreProperties.txt
#     - GraphemeBreakProperty.txt
#     - CompositionExclusions.txt
#     - CaseFolding.txt
#
# Don't run this script directly if you're generating the file for the
# first time. Instead, run `./gen.sh' in this directory and all of the
# relevant Unicode Data Files will be downloaded automatically and
# unicode_data.c will be copied to src/. The Unicode Data Files are
# licensed under the Unicode Data License; see LICENSE.md for details.
#
# A basic explanation of this system is here:
# http://site.icu-project.org/design/struct/utrie
#
# Here's an example of a lookup to aid understanding:
#
#     1. Begin with U+61 --- block 0
#     2. Look up 0 in stage1 --- index 0
#     3. Look up 0x0061 (the bottom two byes of the original code
#        point) in the first table in stage2 -- index 12
#     4. Read the 12th entry in the basic property table

use v5.10;
use strict;
use warnings;

use lib '.';

use FileHandle;
use Term::ProgressBar;
use List::Flatten;
use Getopt::Long;
use Char;

# Global configuration variables.
my $filename = "unicode_data.c";
my $verbose  = 0;

GetOptions("output=s" => \$filename,
           "verbose"  => \$verbose)
  or die("gen.pl: Exiting due to invalid " .
         "command-line parameters.\n");

if ($verbose) {
	print "gen.pl: Running with verbose output.\n";
	print "gen.pl: Dumping output into `$filename'.\n";
}

# <DATA> contains the hand-maintained includes/comments for the file.
open(my $out, ">", $filename);
open(my $fh, '<:encoding(UTF-8)', "UnicodeData.txt")
  or die "gen.pl: Could not open `UnicodeData.txt': $!\n";
print $out <DATA>, "\n";

# Build and return a hash table of code points.
sub gen_chars {
	my ($fh) = @_;
	my %chars;

	print "gen.pl: Parsing `UnicodeData.txt'...\n" if $verbose;

	my $line_count = `wc -l UnicodeData.txt | awk '{ print \$1 }'`;
	my $linenum = 0;
	my $next_update = 0;
	my $progress = Term::ProgressBar->new({count => $line_count});
	$progress->minor(0);

	while (my $l = <$fh>) {
		$l =~ s/(.*?)#.*/$1/;
		chomp $l;

		$next_update = $progress->update($linenum)
		  if $linenum >= $next_update;
		$linenum++;

		# It's not a range, it's just a regular character.
		if ($l !~ /^([0-9A-F]+);<([^;>,]+), First>;/i) {
			$l =~ /^(.*?);/;
			$chars{hex($1)} = Char->new(line => $l);
			next;
		}

		# It's a range!
		$progress->message("gen.pl: Generating range for $2.\n")
		  if $verbose;

		$l = <$fh>;
		my $start = hex($1);
		my $char = Char->new(line => $l);

		die "gen.pl: Expected range end-point at line: $l\n"
		  if $l !~ /^([0-9A-F]+);<([^;>,]+), Last>;/i;

		my $end = hex($1);
		my $name = $2;

		for (my $i = $start; $i < $end; $i++) {
			my $clone = Char->new(line => $char->{line});
			$clone->{code} = $i;
			$clone->{name} = $name;
			$chars{$i} = $clone;
		}
	}

	$progress->update($line_count);
	print "gen.pl: Loaded ", scalar keys %chars, " code points.\n"
	  if $verbose;

	return %chars;
}

# Build and return an array of unique strings.
sub gen_properties {
	my (%chars) = @_;
	my (%properties_indicies, @properties);

	print "gen.pl: Generating properties...\n" if $verbose;

	foreach my $key (keys %chars) {
		my $entry = $chars{$key}->echo;
		$chars{$key}->{entry_index} = $properties_indicies{$entry};

		if (not defined $chars{$key}->{entry_index}) {
			$properties_indicies{$entry} = scalar @properties;
			$chars{$key}->{entry_index} = scalar @properties;
			push @properties, $entry . " /* U+" . $key . " */";
		}
	}

	print "gen.pl: Generated ", scalar @properties, " properties.\n"
	  if $verbose;

	return (\%chars, \@properties);
}

sub gen_tables {
	my (%chars) = @_;
	my (@stage1, @stage2, %old_indices);

	print "gen.pl: Generating tables...\n" if $verbose;

	for (my $code = 0; $code < 0x110000; $code += 0x100) {
		my @stage2_entry;

		for (my $code2 = $code; $code2 < $code + 0x100; $code2++) {
			push @stage2_entry, $chars{$code2}
			  ? $chars{$code2}->{entry_index} + 1
			  : 0;
		}

		my $old = $old_indices{@stage2_entry};

		if ($old) {
			push @stage1, $old * 0x100;
		} else {
			$old_indices{@stage2_entry} = scalar @stage2;
			push @stage1, scalar @stage2 * 0x100;
			push @stage2, \@stage2_entry;
		}
	}

	print "gen.pl: Generated stage 1 table with ",
	  scalar @stage1, " elements.\n" if $verbose;
	print "gen.pl: Generated stage 2 table with ",
	  (scalar flat @stage2), " elements.\n" if $verbose;

	return (\@stage1, \@stage2);
}

my ($chars, $properties) = gen_properties(gen_chars($fh));
my ($stage1, $stage2) = gen_tables(%$chars);

sub print_table {
	my ($name, @table) = @_;

	print $out "uint16_t $name\[] = {\n";
	foreach (my $i = 0; $i < scalar @table; $i++) {
		print $out "\t" if $i % 20 == 0;
		print $out "$table[$i],";
		print $out "\n" if ($i + 1) % 20 == 0
		  or $i + 1 == scalar @table;
	}
	print $out "};\n\n";
}

print $out "struct codepoint codepoints[] = {\n";
print $out "\t{ 0, 0, 0, 0 },\n";
foreach my $cp (@$properties) { print $out "$cp\n"; }
print $out "};\n\n";

print_table("stage1", @$stage1);
print_table("stage2", flat @$stage2);

print "gen.pl: Done.\n" if $verbose;

__DATA__
// SPDX-License-Identifier: Unicode-DFS-2016

/*
 * This file is generated by `script/gen.pl'; do not edit by hand.
 */

// #include "kdgu.h"
#include "unicode_data.h"
