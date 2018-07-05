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

package Char;

use v5.10;
use strict;
use warnings;

use Moose;

has line          => (is => 'rw');
has entry_index   => (is => 'rw');

has bound_class   => (is => 'rw');

has special_lc    => (is => 'rw');
has special_tc    => (is => 'rw');
has special_uc    => (is => 'rw');

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

	$self->{uppercase}     = $14 eq '' ? '-1'
	    : main::emit_sequence((hex($14)));
	$self->{lowercase}     = $15 eq '' ? '-1'
	    : main::emit_sequence((hex($15)));
	$self->{titlecase}     = $16 eq '' ? '-1'
	    : main::emit_sequence((hex($16)));

	$self->{decomp} = $7 eq "" ? undef : [map { hex } split(/\s+/, $7)];
    } else {
	die "Input line could not be parsed: $self->line\n";
    }
}

sub cvar {
    my ($prefix, $data) = @_;
    return (not defined $data or $data eq "")
	? "0,"
	: $prefix . "_" . uc $data . ",";
}

# Prints out the code point as an entry in the C table.

sub echo {
    my $self = shift;

    return "{" .
	cvar("CATEGORY",    $self->{category})    .
	cvar("BOUNDCLASS",  $self->{bound_class}) .
	cvar("BIDI",        $self->{bidi_class})  .
	cvar("DECOMP_TYPE", $self->{decomp_type}) .

	$self->{ccc}           . "," .
	$self->{bidi_mirrored} . "," .

	$self->{lowercase}     . "," .
	$self->{uppercase}     . "," .
	$self->{titlecase}     . "," .

	($self->{special_lc} ? main::emit_sequence(@{$self->{special_lc}}) : "-1") . "," .
	($self->{special_tc} ? main::emit_sequence(@{$self->{special_tc}}) : "-1") . "," .
	($self->{special_uc} ? main::emit_sequence(@{$self->{special_uc}}) : "-1") . "," .

	($self->{decomp} ? main::emit_sequence(@{$self->{decomp}}) : "-1") . "},";
}

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

sub LOG { print "$0: ", @_, "\n" if $verbose }

LOG("Running with verbose output.");
LOG("Running in debugging mode.") if $debug;
LOG("Will dump output into `$filename'.");

my $fh;
open($fh, '<:encoding(UTF-8)', "CompositionExclusions.txt")
    or die "$0: Could not open `CompositionExclusions.txt': $!\n";
my %exclusions;
while (my $l = <$fh>) {
    if ($l =~ /^([0-9A-F]+)/i) {
	$exclusions{hex($1)} = 1;
    }
}

open($fh, '<:encoding(UTF-8)', "GraphemeBreakProperty.txt")
    or die "$0: Could not open `GraphemeBreakProperty.txt': $!\n";
my %boundclasses;
while (my $l = <$fh>) {
    if ($l =~ /^([0-9A-F]+)\.\.([0-9A-F]+)\s*;\s*([A-Za-z_]+)/) {
	for (my $i = hex($1); $i < hex($2); $i++) {
	    $boundclasses{$i} = $3;
	}
    } elsif ($l =~ /^([0-9A-F]+)\s*;\s*([A-Za-z_]+)/) {
	$boundclasses{hex($1)} = $2;
    }
}

open($fh, '<:encoding(UTF-8)', "SpecialCasing.txt")
    or die "$0: Could not open `SpecialCasing.txt': $!\n";
my (%special_lc, %special_tc, %special_uc);
while (my $l = <$fh>) {
    last if $l =~ /Conditional Mappings/;
    if ($l =~ /^(\S+); (.*?); (.*?); (.*?); # .*/) {
	$special_lc{hex($1)} = [map { hex } split ' ', $2];
	$special_tc{hex($1)} = [map { hex } split ' ', $3];
	$special_uc{hex($1)} = [map { hex } split ' ', $4];
    }
}

# Table of sequences and related logic.
my (@sequences, %sequences_table);
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

# Dumps out an array of code points into the sequence table and
# returns the index, with the top two bits containing the length.
sub emit_sequence {
    my (@array) = @_;
    my @out;
    foreach my $c (@array) { push @out, utf16_encode($c) }
    my $idx = $sequences_table{join ',', @out};
    my $len = scalar @out;

    # We can't fit the length into the top two bits, so we'll stick it
    # in at the beginning of the entry instead.
    if ($len >= 3) {
	unshift @out, $len;

	# The value 3 for the top two bits is a magic value that
	# indicates the length is the first value in the sequence
	# entry.
	$len = 3;
    }

    if (not defined $idx) {
	$idx = scalar @sequences;
	push @sequences, @out;
	$sequences_table{join ',', @out} = $idx;
    }

    return $idx | $len << 14;
}

open(my $out, ">", $filename);
open($fh, '<:encoding(UTF-8)', "UnicodeData.txt")
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

    LOG("Parsing `UnicodeData.txt'...");

    my $linecount = int `wc -l UnicodeData.txt | awk '{ print \$1 }'`;
    my $linenum = 0;
    my $next_update = 0;
    my $progress = $verbose
	? Term::ProgressBar->new({count => $linecount})
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
	    $chars{hex($1)}->{bound_class} = $boundclasses{hex($1)};
	    $chars{hex($1)}->{special_lc} = $special_lc{hex($1)};
	    $chars{hex($1)}->{special_tc} = $special_tc{hex($1)};
	    $chars{hex($1)}->{special_uc} = $special_uc{hex($1)};
	    next;
	}

	# It's a range!
	$progress->message("$0: Generating range for `$2'.")
	    if $verbose;

	# Most important lookups don't happen in the ranged entries,
	# so we can just pretend they don't exist to speed up the
	# initial character data loading.
	next if $debug;

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

    $progress->update($linecount);
    LOG("Loaded ", scalar(keys %chars), " code points.");

    return %chars;
}

# Build and return an array of unique strings.
sub gen_properties {
    my (%chars) = @_;
    my (%properties_indicies, @properties);

    LOG("Generating properties...");

    foreach my $key (keys %chars) {
	my $entry = $chars{$key}->echo;
	$chars{$key}->{entry_index} = $properties_indicies{$entry};

	if (not defined $chars{$key}->{entry_index}) {
	    $properties_indicies{$entry} = scalar @properties;
	    $chars{$key}->{entry_index} = scalar @properties;
	    push @properties, "\t" . $entry;
	}
    }

    LOG("Generated ", scalar(@properties), " properties.");

    return (\%chars, \@properties);
}

# Generates tables used for efficient look ups of Unicode code
# points. If you just skim over this you may wonder how this is more
# space-efficient than a simple flat array mapping a code point to an
# index: the strength of this system is that duplicate blocks don't
# have to be regenerated.
sub gen_tables {
    my (%chars) = @_;
    my (@stage1, @stage2, %old_indices);

    LOG("Generating tables...");

    for (my $code = 0; $code < 0x10FFFF; $code += $BLOCK_SIZE) {
	my @entry;

	for (my $code2 = $code;
	     $code2 < $code + $BLOCK_SIZE;
	     $code2++) {
	    push @entry, $chars{$code2}
	    ? $chars{$code2}->{entry_index} + 1
		: 0;
	}

	my $old = $old_indices{join ',', @entry};

	if ($old) {
	    push @stage1, $old * $BLOCK_SIZE;
	} else {
	    $old_indices{join ',', @entry} = scalar @stage2;
	    push @stage1, scalar(@stage2) * $BLOCK_SIZE;
	    push @stage2, \@entry;
	}
    }

    LOG("Generated stage 1 table with ",
	scalar(@stage1),
	" elements.");
    LOG("Generated stage 2 table with ",
	scalar(flat @stage2),
	" elements.");
    LOG("=> Total: ",
	scalar(@stage1) + scalar(flat @stage2),
	" entries.");

    return (\@stage1, \@stage2);
}

# Generates a table mapping a pair of code points (the first two
# numbers in the entry) to a canonical composition.
sub gen_comb {
    my (%chars) = @_;
    my @comb;

    LOG("Generating combining indices...");

    for (my $i = 0; $i < 0x10FFFF; $i++) {
	my $cp = $chars{$i};

	if (not defined $cp
	    # Only include canonical decompositions.
	    or $cp->{decomp_type}
	    or not defined $cp->{decomp}
	    or scalar(@{$cp->{decomp}}) != 2
	    or $chars{@{$cp->{decomp}}[0]}->{ccc}
	    or $exclusions{$cp->{code}}) {
	    next;
	}

	push @comb, $cp->{decomp}[0];
	push @comb, $cp->{decomp}[1];
	push @comb, $cp->{code};
    }

    LOG("Generated ", (scalar @comb / 3), " combining indices.");

    return @comb;
}

sub print_array {
    my ($type, $name, @array) = @_;

    print $out "$type $name\[] = {\n";
    for (my $i = 0; $i < scalar @array; $i++) {
	print $out "\t" if $i % 20 == 0;
	print $out "$array[$i],";
	print $out "\n" if ($i + 1) % 20 == 0
	    or $i + 1 == scalar @array;
    }
    print $out "};\n\n";
};

my ($chars, $properties) = gen_properties(gen_chars($fh));
my ($stage1, $stage2) = gen_tables(%$chars);
my @comb = gen_comb(%$chars);

# Generation is now done.

LOG("Generated ", scalar(@sequences), " sequence elements.");

print $out "struct codepoint codepoints[] = {\n";
print $out "\t{0,0,0,0,0,0,-1,-1,-1,-1,-1,-1,-1},\n";
foreach my $cp (@$properties) { print $out "$cp\n"; }
print $out "};\n\n";

print_array("uint32_t", "compositions", @comb);
print $out "int num_comp = ", (scalar @comb / 3), ";\n";

print_array("uint16_t", "stage1", @$stage1);
print_array("uint16_t", "stage2", flat @$stage2);
print_array("uint16_t", "sequences", @sequences);

LOG("Done; exiting.");

__DATA__
// SPDX-License-Identifier: Unicode-DFS-2016

/*
 * This file is generated by `script/gen.pl'; do not edit by hand.
 */

// #include "kdgu.h"
#include "unicode_data.h"

int num_comp;

struct codepoint *
codepoint(uint32_t c)
{
	if (c > 0x10FFFF) return codepoints;
	return codepoints + (stage2[stage1[c / 256]
	                            + (c % 256)]);
}

unsigned
write_sequence(uint32_t *buf, uint16_t idx)
{
	if (idx == (uint16_t)-1) return 0;

	unsigned len = (idx & 0xC000) == 0xC000
		          ? sequences[idx & 0x3FFF]
			  : (idx & 0xC000) >> 14;
	if (!buf) return len;
	unsigned j = 0;
	idx &= 0x3FFF;

	for (unsigned i = len >= 3 ? 1 : 0;
	     i < (len >= 3 ? len + 1 : len);
	     i++) {
		uint32_t d = sequences[idx + i];
		if (d > 0xD7FF && d < 0xE000) {
			uint32_t e = sequences[idx + i + 1];
			d = (d - 0xD800) * 0x400 + e - 0xDC00 + 0x10000;
			i++;
		}
		buf[j++] = d;
	}

	return len;
}

uint32_t
lookup_comp(uint32_t a, uint32_t b)
{
	for (int i = 0; i < num_comp * 3; i += 3)
		if (compositions[i] == a && compositions[i + 1] == b)
			return compositions[i + 2];
	return UINT32_MAX;
}
