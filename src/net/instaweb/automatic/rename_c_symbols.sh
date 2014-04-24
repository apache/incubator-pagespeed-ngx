#!/bin/sh
#
# Copyright 2013 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Author: morlovich@google.com (Maksim Orlovich)
#
# Script that takes an .a file and renames all internal C functions in it
# to have a new prefix in order to avoid symbol clashes.
#
# Usage:  ./rename_c_symbols.sh input_library.a output_library.a

set -e  # exit script if any command returns an error
set -u  # exit the script if any variable is uninitialized

IN=$(readlink -f $1)
OUT=$(readlink -f $2)

# Get a list of defined non-C++ symbols that are global and not weak.
# _Z is used at start of C++-mangled symbol names.
# Lower-cased letters indicate symbols local to the object file.
# V and W denote weak symbols.
#
# We exclude AtomicOps_Internalx86CPUFeatures since it's used by some
# Chromium headers.
# We also skip x86 PIC thunk functions: those use fancy link-once
# features we can't detect easily.
#
ALL_SYMBOLS=$(mktemp)
nm -o -P --defined-only $IN | grep -v " _Z" | egrep -v " [[:lower:]] " \
  | grep -v " V " | grep -v " W " | grep -v AtomicOps_Internalx86CPUFeatures \
  | fgrep -v 86.get_pc_thunk > $ALL_SYMBOLS

COMMON_SYMBOLS=$(mktemp)
NON_COMMON_SYMBOLS=$(mktemp)
# Separate out common symbols (marked by nm as " C "), since they are
# permitted to have duplicates. Also drop the non-name fields now.
grep " C " $ALL_SYMBOLS | cut -d ' ' -f 2 > $COMMON_SYMBOLS
grep -v " C " $ALL_SYMBOLS | cut -d ' ' -f 2 > $NON_COMMON_SYMBOLS
rm $ALL_SYMBOLS

SYMBOLS=$(mktemp)
sort -u  $COMMON_SYMBOLS > $SYMBOLS
cat $NON_COMMON_SYMBOLS >> $SYMBOLS
rm $COMMON_SYMBOLS
rm $NON_COMMON_SYMBOLS

# Generate a renaming map
RENAME_MAP=$(mktemp)
while read S; do
  echo $S pagespeed_ol_$S >> $RENAME_MAP
done < $SYMBOLS

rm $SYMBOLS

# Rename the symbols in the the input file.
objcopy -v --redefine-syms $RENAME_MAP $IN $OUT

# Index the new archive.
echo ranlib $OUT
ranlib $OUT
