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

package main;

use v5.10;
use strict;
use warnings;

use FileHandle;
use Term::ProgressBar;
use List::Flatten;
use Getopt::Long;

# Global configuration variables.
my $filename   = "unicode_data.c";
my $verbose    = 0;
my $debug      = 0;
my $BLOCK_SIZE = 256;

GetOptions("output=s" => \$filename,
           "length=i" => \$BLOCK_SIZE,
           "debug"    => \$debug,
           "verbose"  => \$verbose)
  or die("$0: Exiting due to invalid " .
         "command-line parameters.\n");

if ($verbose) {
	print "$0: Running with verbose output.\n";
	print "$0: Dumping output into `$filename'.\n";
}

# Table of sequences and related logic.
my @sequences;
sub utf16_encode {
	my ($c) = @_;
	my @buf;

	if ($c <= 0xFFFF) {
		push @buf, $c;
		return @buf
	}

	push @buf, (($c - 0x10000) >> 10)   | 0xDC00;
	push @buf, (($c - 0x10000) & 0x3FF) | 0xDC00;

	return @buf;
}

sub emit_sequence {
	my (@array) = @_;
	my @out;
	foreach my $c (@array) { push @out, utf16_encode($c) }
	my $idx = scalar @sequences;
	push @sequences, scalar @out;
	push @sequences, @out;
	return $idx;
}

package Char;

use v5.10;
use strict;
use warnings;

use Moose;

has line          => (is => 'rw');

# This code point's entry number in the primary data table.
has entry_index   => (is => 'rw');

has code          => (is => 'rw');
has name          => (is => 'rw');
has category      => (is => 'rw');
has ccc           => (is => 'rw');
has bidi_class    => (is => 'rw');
has decomp_type   => (is => 'rw');
has decomp        => (is => 'rw');
has bidi_mirrored => (is => 'rw');
has uppercase     => (is => 'rw');
has lowercase     => (is => 'rw');
has titlecase     => (is => 'rw');

sub BUILD {
	my $self = shift;

	# The details of the UnicodeData.txt format can be found at
	# http://www.unicode.org/reports/tr44/tr44-20.html#UnicodeData.txt

	if ($self->line =~
	    /^
	     (.*?);          # 1. Code Point
	     (.*?);          # 2. Name
	     (.*?);          # 3. General_Category

	     (.*?);          # 4. Canonical_Combining_Class
	     (.*?);          # 5. Bidi_Class
	     (?:<(.*?)>)?\s* # 6. Decomposition_Type
	     (.*?);          # 7. Decomposition_Mapping

	     (.*?);          # 8. Numeric_Type
	     (.*?);          # 9. Digit
	     (.*?);          # 10. Numeric

	     (.*?);          # 11. Bidi_Mirrored

	     (.*?);          # 12. Unicode_1_Name (deprecated)
	     (.*?);          # 13. ISO_Comment (deprecated)

	     (.*?);          # 14. Simple_Uppercase_Mapping
	     (.*?);          # 15. Simple_Lowercase_Mapping
	     (.*?)           # 16. Simple_Titlecase_Mapping
	     $/x) {
		$self->{code}          = hex($1);
		$self->{name}          = $2;
		$self->{category}      = $3;
		$self->{ccc}           = int($4);
		$self->{bidi_class}    = $5;
		$self->{decomp_type}   = $6;

		$self->{bidi_mirrored} = $11 eq 'Y' ? 1 : 0;

		$self->{uppercase}     = $14 eq '' ? '-1' : hex($14);
		$self->{lowercase}     = $15 eq '' ? '-1' : hex($15);
		$self->{titlecase}     = $16 eq '' ? '-1' : hex($16);

		$self->{decomp}        = $7 eq ''
		  ? '-1'
		  : main::emit_sequence(map { hex } split(/\s+/, $7));
	} else {
		die "Input line could not be parsed: $self->line\n";
	}
}

sub cvar {
	my ($prefix, $data) = @_;
	return (not defined $data or $data eq "")
	  ? "0, "
	  : $prefix . "_" . uc $data . ", ";
}

# Prints out the code point as an entry in the C table.

sub echo {
	my $self = shift;

	return "{ " .
	  cvar("CATEGORY",    $self->{category})    .
	  cvar("BIDI",        $self->{bidi_class})  .
	  cvar("DECOMP_TYPE", $self->{decomp_type}) .
	  $self->{bidi_mirrored} . ", " .
	  $self->{lowercase} . ", " .
	  $self->{uppercase} . ", " .
	  $self->{titlecase} . ", " .
	  $self->{decomp} .
	  " },";
}

package main;

open(my $out, ">", $filename);
open(my $fh, '<:encoding(UTF-8)', "UnicodeData.txt")
  or die "$0: Could not open `UnicodeData.txt': $!\n";

# <DATA> contains the hand-maintained includes/comments for the
# file. Additionally, it contains the implementations of the data
# accessor functions.
my $DATA = do { local $/; <DATA> };
$DATA =~ s/BLOCK_SIZE/$BLOCK_SIZE/g;
print $out $DATA, "\n";

# Build and return a hash table of code points.
sub gen_chars {
	my ($fh) = @_;
	my %chars;

	print "$0: Parsing `UnicodeData.txt'...\n" if $verbose;

	my $line_count = `wc -l UnicodeData.txt | awk '{ print \$1 }'`;
	my $linenum = 0;
	my $next_update = 0;
	my $progress = $verbose
	  ? Term::ProgressBar->new({count => $line_count})
	  : undef;
	$progress->minor(0) if $verbose;

	while (my $l = <$fh>) {
		$l =~ s/(.*?)#.*/$1/;
		chomp $l;

		$next_update = $progress->update($linenum)
		  if $verbose and $linenum >= $next_update;
		$linenum++;

		# It's not a range, it's just a regular character.
		if ($l !~ /^([0-9A-F]+);<([^;>,]+), First>;/i) {
			$l =~ /^(.*?);/;
			$chars{hex($1)} = Char->new(line => $l);
			next;
		}

		# Most important lookups don't happen in the ranged entries,
		# so we can just pretend they don't exist to speed up the
		# initial character data loading.
		next if $debug;

		# It's a range!
		$progress->message("$0: Generating range for $2.")
		  if $verbose;

		$l = <$fh>;
		my $start = hex($1);
		my $char = Char->new(line => $l);

		die "$0: Expected range end-point at line: $l\n"
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
	print "$0: Loaded ", scalar keys %chars, " code points.\n"
	  if $verbose;

	return %chars;
}

# Build and return an array of unique strings.
sub gen_properties {
	my (%chars) = @_;
	my (%properties_indicies, @properties);

	print "$0: Generating properties...\n" if $verbose;

	foreach my $key (keys %chars) {
		my $entry = $chars{$key}->echo;
		$chars{$key}->{entry_index} = $properties_indicies{$entry};

		if (not defined $chars{$key}->{entry_index}) {
			$properties_indicies{$entry} = scalar @properties;
			$chars{$key}->{entry_index} = scalar @properties;
			push @properties,
			  "\t/* "
			  . sprintf("U+%04X", $key)
			  . " */ " . $entry;
		}
	}

	print "$0: Generated ", scalar @properties, " properties.\n"
	  if $verbose;

	return (\%chars, \@properties);
}

sub gen_tables {
	my (%chars) = @_;
	my (@stage1, @stage2, %old_indices);

	print "$0: Generating tables...\n" if $verbose;

	for (my $code = 0; $code < 0x10FFFF; $code += $BLOCK_SIZE) {
		my @stage2_entry;

		for (my $code2 = $code;
		     $code2 < $code + $BLOCK_SIZE;
		     $code2++) {
			push @stage2_entry, $chars{$code2}
			  ? $chars{$code2}->{entry_index} + 1
			  : 0;
		}

		my $old = $old_indices{join ',', @stage2_entry};

		if ($old) {
			push @stage1, $old * $BLOCK_SIZE;
		} else {
			$old_indices{join ',', @stage2_entry} = scalar @stage2;
			push @stage1, scalar @stage2 * $BLOCK_SIZE;
			push @stage2, \@stage2_entry;
		}
	}

	print "$0: Generated stage 1 table with ",
	  scalar @stage1, " elements.\n" if $verbose;
	print "$0: Generated stage 2 table with ",
	  (scalar flat @stage2), " elements.\n" if $verbose;

	return (\@stage1, \@stage2);
}

my ($chars, $properties) = gen_properties(gen_chars($fh));
my ($stage1, $stage2) = gen_tables(%$chars);

sub print_array {
	my ($name, @array) = @_;

	print $out "uint16_t $name\[] = {\n";
	for (my $i = 0; $i < scalar @array; $i++) {
		print $out "\t" if $i % 20 == 0;
		print $out "$array[$i],";
		print $out "\n" if ($i + 1) % 20 == 0
		  or $i + 1 == scalar @array;
	}
	print $out "};\n\n";
}

print $out "struct codepoint codepoints[] = {\n";
print $out "\t{ 0, 0, 0, 0 },\n";
foreach my $cp (@$properties) { print $out "$cp\n"; }
print $out "};\n\n";

print_array("stage1", @$stage1);
print_array("stage2", flat @$stage2);
print_array("sequences", @sequences);

print "$0: Done; exiting.\n" if $verbose;

__DATA__
// SPDX-License-Identifier: Unicode-DFS-2016

/*
 * This file is generated by `script/gen.pl'; do not edit by hand.
 */

// #include "kdgu.h"
#include "unicode_data.h"

struct codepoint *
codepoint(uint32_t c)
{
	return codepoints + (stage2[stage1[c / BLOCK_SIZE]
	                            + (c % BLOCK_SIZE)]);
}
