#!/bin/bash
#
# Copyright 2003 Google Inc.
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
# Runs a file in $1 through a giant sed script, transforming normal
# C++ comments to Doxygen comments.  The resultant file is placed in
# $2/$1, so $2 must be a subdirectory.
#
# Usage:  scripts/doxify.sh filename destination_directory

set -u  # exit the script if any variable is uninitialized
set -e  # exit script if any command returns an error

filename=$1
destination_directory=$2
outfile=$destination_directory/$filename

mkdir -p "$(dirname $outfile)"

sed -r 's~/\*([^\*])~/\*\*\1~;                  # /* -> /**  \
       s~///*~///~;                 # // -> ///  \
       s~;[ 	]*/\*\*([^<*]*)~; /\*\*<\1~; # /** -> /**< on right after code  \
       s~;[ 	]*///*([^<])~; ///<\1~; # /// -> ///< on right after code \
       s~,([ 	]*)///*([^<])~,\1///<\2~; # /// -> ///< on right after enum \
       s~([[:alnum:]][ 	]*)///*([^<])~\1///<\2~; # /// -> ///< on right after enum \
       s~DISALLOW_COPY_AND_ASSIGN\(.*\)\;~~; # /// -> ///< on right after code \
       s~(///) *---+([^-].+[^-]) *---+~\1\2~; # /// ---- Bla ---- -> /// Bla
       s~(///) *===+([^=].+[^=]) *===+~\1\2~; # /// ==== Bla ==== -> /// Bla
       s~(///) *\*\*\*+([^\*].+[^\*]) *\*\*\*+~\1\2~; # /// **** Bla **** -> /// Bla
       s~(///) *----*( *)~\1\2~; # /// -------- -> ///
       s~(///) *====*( *)~\1\2~; # /// ======== -> ///
       s~(///) *\*\*\*\**( *)~\1\2~; # /// ******** -> ///
       s~(///) *\* \* \*( \*)* *~\1~; # /// * * * * * -> ///
       s~(([^A-Z_])((TODO|FIXME)[^A-Z_].*))~\2 @todo \3~; # TODO* -> @todo TODO* \
       s~(([^A-Z_])((BUG)[^A-Z_].*))~\2 @bug \3~; # BUG* -> @bug BUG* \
       s~([ \t]*)ABSTRACT([ \t]*\;)~\1\=0\2~; # void f() ABSTRACT; -> void f() =0; \
       s~DECLARE_string(.*)~DECLARE_STRING\1~; # /// -> ///< on right after code \
       s~DECLARE_bool(.*)~DECLARE_BOOL\1~; # /// -> ///< on right after code \
       s~DECLARE_int32(.*)~DECLARE_INT32\1~; # /// -> ///< on right after code \
       s~DECLARE_uint32(.*)~DECLARE_UINT32\1~; # /// -> ///< on right after code \
       s~DECLARE_int64(.*)~DECLARE_INT64\1~; # /// -> ///< on right after code \
       s~DECLARE_uint64(.*)~DECLARE_UINT64\1~; # /// -> ///< on right after code \
       s~/// *(Copyright(.*))~// \1~; # clutter \
       s~/// *(All [rR]ights [rR]eserved(.*))~// \1~; # clutter \
       s~/// *(Date: (.*))~/// @file~; # clutter \
       s~/// *Author:(.*)~/// @file~; # /// Author -> /// @file \
       s~/// *Author(.*)~/// @file~; # /// Author -> /// @file ' \
  < $filename > $outfile
