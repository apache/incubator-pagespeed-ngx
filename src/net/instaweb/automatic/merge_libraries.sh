#!/bin/sh
#
# Copyright 2011 Google Inc.
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
# Author: jmarantz@google.com (Joshua Marantz)
#
# Script to merge together multiple libraries (.a) into one larger
# library (.a).  A modest effort is expended to avoid duplicate
# filenames: we rename each .o with the index of the library where we
# find it.  Otherwise if multiple libraries contain string_util.o then
# only one would be included in the aggregate.
#
# Usage:  ./merge_libraries /PATH/output_library.a input1.a input2.a ...
#
# The output library must be specified as an absolute path.

output=$1
shift
tmpdir=/tmp/merge_libraries.$USER.$$
rm -rf $tmpdir
mkdir $tmpdir
cd $tmpdir

rm -f $output
prefix=0

ar=ar

for lib in $*; do
  prefix=`expr $prefix + 1`
  files=`$ar -t $lib`
  for entry in $files; do
    leaf=`basename $entry`.o
    $ar p $lib $entry > $prefix.$leaf
  done
  $ar -q -S $output *.o
  echo Adding `ls -l *.o | wc -l` files from $lib ...
  rm *.o
done

echo ranlib $output
ranlib $output
ls -l $output
rm -rf $tmpdir
