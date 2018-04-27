#!/usr/bin/env perl
# SPDX-License-Identifier: MIT

# This script generates unicode_data.c from the following files:
#
#     - UnicodeData.txt
#     - SpecialCasing.txt
#     - DerivedCoreProperties.txt
#     - GraphemeBreakProperty.txt
#     - CompositionExclusions.txt
#     - CaseFolding.txt
#
# Don't run this script directly if you're generating the file for the
# first time; run gen.sh in this directory and all of the relevant
# Unicode Data Files will be downloaded automatically and the
# resulting unicode_data.c file will be copied to src/. The Unicode
# Data Files are licensed under the Unicode Data License; see
# LICENSE.md for details.

use strict;
use warnings;

use Getopt::Long;

my $blocksize = 4000;
my $verbose = 0;
my $filename = "unicode_data.c";

GetOptions("length=i" => \$blocksize,
	   "output=s" => \$filename,
	   "verbose" => \$verbose)
    or die("gen.pl: Exiting due to invalid " .
	   "command-line parameters.\n");

print "gen.pl: Running with verbose output.\n" if $verbose;
print "gen.pl: Dumping output into `$filename'.\n" if $verbose;

my @script = ('Arabic', 'Armenian', 'Bengali', 'Bopomofo', 'Braille',
	      'Buginese', 'Buhid', 'Canadian_Aboriginal', 'Cherokee',
	      'Common', 'Coptic', 'Cypriot', 'Cyrillic', 'Deseret',
	      'Devanagari', 'Ethiopic', 'Georgian', 'Glagolitic',
	      'Gothic', 'Greek', 'Gujarati', 'Gurmukhi', 'Han',
	      'Hangul', 'Hanunoo', 'Hebrew', 'Hiragana', 'Inherited',
	      'Kannada', 'Katakana', 'Kharoshthi', 'Khmer', 'Lao',
	      'Latin', 'Limbu', 'Linear_B', 'Malayalam', 'Mongolian',
	      'Myanmar', 'New_Tai_Lue', 'Ogham', 'Old_Italic',
	      'Old_Persian', 'Oriya', 'Osmanya', 'Runic', 'Shavian',
	      'Sinhala', 'Syloti_Nagri', 'Syriac', 'Tagalog',
	      'Tagbanwa', 'Tai_Le', 'Tamil', 'Telugu', 'Thaana',
	      'Thai', 'Tibetan', 'Tifinagh', 'Ugaritic', 'Yi',
	      'Balinese', 'Cuneiform', 'Nko', 'Phags_Pa',
	      'Phoenician', 'Carian', 'Cham', 'Kayah_Li', 'Lepcha',
	      'Lycian', 'Lydian', 'Ol_Chiki', 'Rejang', 'Saurashtra',
	      'Sundanese', 'Vai', 'Avestan', 'Bamum',
	      'Egyptian_Hieroglyphs', 'Imperial_Aramaic',
	      'Inscriptional_Pahlavi', 'Inscriptional_Parthian',
	      'Javanese', 'Kaithi', 'Lisu', 'Meetei_Mayek',
	      'Old_South_Arabian', 'Old_Turkic', 'Samaritan',
	      'Tai_Tham', 'Tai_Viet', 'Batak', 'Brahmi', 'Mandaic');

my @category = ('Cc', 'Cf', 'Cn', 'Co', 'Cs', 'Ll', 'Lm', 'Lo', 'Lt',
		'Lu', 'Mc', 'Me', 'Mn', 'Nd', 'Nl', 'No', 'Pc', 'Pd',
		'Pe', 'Pf', 'Pi', 'Po', 'Ps', 'Sc', 'Sk', 'Sm', 'So',
		'Zl', 'Zp', 'Zs' );

open(my $out, ">", $filename);

# Generates an entry in the basic data table.
sub entry {
    my ($category,
	$comb_class,
	$bidi,
	$decomp_type,
	$decomp_mapping) = @_;

    print $out "\t{ ";

    print $out "$category, ";
    print $out "$comb_class, ";
    print $out "$bidi, ";
    print $out "$decomp_type ";

    print $out "},\n";
}

# <DATA> contains the header includes/comments for the file and any
# other hand-maintained data.
print $out <DATA>, "\n";

# Begin generating the basic table from `UnicodeData.txt'.
print $out "struct codepoint codepoints[] = {\n";

open(my $fh, '<:encoding(UTF-8)', "UnicodeData.txt")
    or die "Could not open '$ARGV[0]': $!";

my $n = 1;
while (my $l = <$fh>) {
    chomp $l;

    # The details of the UnicodeData.txt format can be found at
    # http://www.unicode.org/reports/tr44/tr44-20.html#UnicodeData.txt

    if ($l =~ /^
	(.*?);		# 1. Code Point
	(.*?);		# 2. Name
	(.*?);		# 3. General_Category

	(.*?);		# 4. Canonical_Combining_Class
	(.*?);		# 5. Bidi_Class
	(?:<(.*?)>)?\s* # 6. Decomposition_Type
	(.*?);		# 7. Decomposition_Mapping

	(.*?);		# 8. Numeric_Type
	(.*?);		# 9. Digit
	(.*?);		# 10. Numeric

	(.*?);		# 11. Bidi_Mirrored

	(.*?);		# 12. Unicode_1_Name (deprecated)
	(.*?);		# 13. ISO_Comment (deprecated)

	(.*?);		# 14. Simple_Uppercase_Mapping
	(.*?);		# 15. Simple_Lowercase_Mapping
	(.*?)		# 16. Simple_Titlecase_Mapping
	$/x) {
	entry("CATEGORY_" . uc $3,
	      $4,
	      "BIDI_" . $5, $6 ? "DECOMP_TYPE_" . uc $6 : 0,
	      $7);
    } else {
	die "Line $n of UnicodeData.txt could not be parsed: $l\n";
    }

    $n++;
}

print $out "};\n\n";

# Now it's time to generate the trie lookup table. A basic explanation
# of this system is here:
# http://site.icu-project.org/design/struct/utrie

# We use a 2-stage table, not counting the data table itself.
#
# Thinking about how the process works can be a bit confusing, so a
# practical example of a lookup may be helpful:
#
#     1. Begin with U+61 --- block 0
#     2. Look up 0 in stage1 --- index 0
#     3. Look up 0x0061 (the bottom two byes of the original code
#        point) in the first table in stage2 -- index 12
#     4. Read the 12th entry
#
# Reading the 12th entry yields a struct with indices for various data
# elements in other tables.

# I found https://goo.gl/9kKWzH very useful.

my $SHIFT_1                    = 6+5;
my $SHIFT_2                    = 5;
my $SHIFT_1_2                  = $SHIFT_1 - $SHIFT_2;
my $OMITTED_BMP_INDEX_1_LENGTH = 0x10000 >> $SHIFT_1;
my $CP_PER_INDEX_1_ENTRY       = 1 << $SHIFT_1;
my $INDEX_2_BLOCK_LENGTH       = 1 << $SHIFT_1_2;
my $INDEX_2_MASK               = $INDEX_2_BLOCK_LENGTH - 1;
my $DATA_BLOCK_LENGTH          = 1 << $SHIFT_2;
my $DATA_MASK                  = $DATA_BLOCK_LENGTH - 1;

my @stage1;

print $out "uint16_t stage1[] = {\n";
print $out "}\n";

__DATA__
// SPDX-License-Identifier: Unicode-DFS-2016

/*
 * This file is generated by `gen.pl' in script/; do not edit by hand.
 */

#include "kdgu.h"
#include "unicode_data.h"
