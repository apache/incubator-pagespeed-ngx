#!/bin/bash

# Copyright 2014 Google Inc.
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

# Script to assist downloading of the closure compiler from the gclient DEPS
# file. This is handled differently than the other DEPS because the compiler
# requires downloading and unzipping a jar file instead of checking out a
# svn/git repo. This avoids downloading the compiler every time gclient syncs.

COMPILERDIR=src/tools/closure
COMPILERZIP=$COMPILERDIR/compiler-latest.zip


# Download and unzip the compiler if we haven't before, or if it's older than a
# day.
if [[ -z $(find $COMPILERDIR -name compiler.jar -ctime -1) ]]
then
  curl https://dl.google.com/closure-compiler/compiler-latest.zip \
    --create-dirs -o $COMPILERZIP
  unzip -o  $COMPILERZIP -d $COMPILERDIR
fi
