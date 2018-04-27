#!/usr/bin/env bash
# SPDX-License-Identifier: MIT

VERSION=10.0.0

wget https://www.unicode.org/Public/$VERSION/ucd/UnicodeData.txt
wget https://www.unicode.org/Public/$VERSION/ucd/SpecialCasing.txt
wget https://www.unicode.org/Public/$VERSION/ucd/DerivedCoreProperties.txt
wget https://www.unicode.org/Public/$VERSION/ucd/CompositionExclusions.txt
wget https://www.unicode.org/Public/$VERSION/ucd/CaseFolding.txt
wget https://www.unicode.org/Public/$VERSION/ucd/auxiliary/GraphemeBreakProperty.txt

./gen.pl
cp unicode_data.c ../src/
