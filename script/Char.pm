# SPDX-License-Identifier: MIT

# This file implements a convenient structure for manipulating Unicode
# code points.

package Char;

use strict;
use warnings;
use v5.10;

use Moose;

has line            => (is => 'rw');

has code            => (is => 'rw');
has name            => (is => 'rw');
has category        => (is => 'rw');
has combining_class => (is => 'rw');
has bidi_class      => (is => 'rw');
has decomp_type     => (is => 'rw');
has decomp_mapping  => (is => 'rw');
has bidi_mirrored   => (is => 'rw');
has uppercase       => (is => 'rw');
has lowercase       => (is => 'rw');
has titlecase       => (is => 'rw');

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
		$self->{code}            = hex($1);
		$self->{name}            = $2;
		$self->{category}        = $3;
		$self->{combining_class} = int($4);
		$self->{bidi_class}      = $5;
		$self->{decomp_type}     = $6;

		$self->{decomp_mapping}  = $7 eq '' ? undef : split /\s+/, $7;
		$self->{bidi_mirrored}   = $11 eq 'Y' ? 1 : 0;

		$self->{uppercase}       = $14 eq '' ? undef : hex($14);
		$self->{lowercase}       = $15 eq '' ? undef : hex($15);
		$self->{titlecase}       = $16 eq '' ? undef : hex($16);
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

# Prints out the code point as an entry in the C table. Needs to
# have the combining indices because those are important.

sub echo {
	my ($self, @comb) = @_;

	return "\t{ " .
	  cvar("CATEGORY",    $self->{category})    .
	  cvar("BIDI_CLASS",  $self->{bidi_class})  .
	  cvar("DECOMP_TYPE", $self->{decomp_type}) .
	  "UINT16_MAX, " .
	  $self->bidi_mirrored . " },\n";
}

1;
