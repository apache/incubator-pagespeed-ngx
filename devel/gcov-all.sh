#!/bin/bash
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
# A helper for running gcov on all of the project sources.
# Usage:
#   gcov-all.sh (--prepare | --summarize) path
#
#   where path is the same location where one runs make
#
# There are two modes:
#   --prepare cleans up all the .gcda files. This should be done
#   before running the test, as we need an accurate set of these
#   to know which object files to include in the executable.
#   It also effectively zeroes all the measurements, preventing
#   different runs from getting added together.
#
#   --summarize goes through the produced files, runs gcov on
#   them, producing the gcov-summary.html and the gcov/ directory
#   with individual dumps
#
# Glossary:
#   .gcno file: produced by gcc during compilation, along with the
#               corresponding .o file
#   .gcda file: produced when an instrumented application is run
#               (or .so is loaded), and then at its exit. Contains
#               the actual measurements.
#
# To invoke gcov, we need to pass it in a list of all the source
# files we want coverage information for, as well as the directory
# to look into for the corresponding .gcno/.gcda files. The
# summarize mode collects these based on the .gcda files that exist.
#
# TODO(morlovich): evaluate lcov as an option? Its output looks nice.

function summarize {
  WORKDIR=`mktemp -d`
  SRCDIR=`pwd`
  OUTNAME=gcov-summary.html

  echo "Collecting all object and profile data into:" $WORKDIR

  # Here, we look for the .gcda files, and the .o and .gcno that go with them.
  # This is because they get generated for any object files that gets linked in,
  # as soon as the executable/module are initialized, giving us an accurate
  # picture of what should be checked

  GCDAS=`find ./out/Debug_Coverage -name '*.gcda'`
  DATA=
  for F in $GCDAS
  do
    BASE=${F%.gcda}
    GCNO=$BASE.gcno
    O=$BASE.o
    if [ ! -f $GCNO  ]; then
      echo "WARNING: can't find " $GCNO
      continue
    fi

    if [ ! -f $O  ]; then
      echo "WARNING: can't find " $O
      continue
    fi

    DATA+=" $F $GCNO $O"
  done

  cp $DATA $WORKDIR/

  echo "Generating gcov summary into file://"$PWD/$OUTNAME

  # Collect relevant sources. For each one, we check if we have the
  # gcda (which means we have gcno, too). We want this for two reasons:
  #
  # 1) We only want coverage for a file if the gcda is there
  # 2) gcov has a bug that screws up output if some files' .gcno
  # does not exist (see http://gcc.gnu.org/bugzilla/show_bug.cgi?id=35568)
  #
  # TODO(morlovich): worry about duplicate names!
  SOURCES=`find -L $SRCDIR/net $SRCDIR/pagespeed -name '*.cc' -or -name '*.c'`

  FILTERED_SOURCES=
  for F in $SOURCES
  do
    GCDA=`basename $F .c`
    GCDA=$WORKDIR/`basename $GCDA .cc`.gcda
    if [ -f $GCDA ]; then
      if [ $F == ${F/.svn/marker/} ]; then
         FILTERED_SOURCES="$FILTERED_SOURCES $GCDA"
      fi
    fi
  done
  htmlDriver > $OUTNAME
  echo "<pre id='data' style='display:none'>" >> $OUTNAME
  gcov -o $WORKDIR $FILTERED_SOURCES >> $OUTNAME
  echo "</pre>" >> $OUTNAME

  echo "Moving all the .gcov files to gcov subdir (after wiping it)"
  rm -rf $SRCDIR/gcov
  mkdir $SRCDIR/gcov
  mv *.gcov $SRCDIR/gcov

  echo "Cleaning up..."
  rm -r $WORKDIR
}

# This outputs the html driver that visualizes the results
function htmlDriver {
  cat <<TEMPLATE_END
<!DOCTYPE html>
<head>
<script>
// Computes a color for given goodness percentage. (Using CSS3 hsl syntax)
function percentColor(percent) {
  var hue = (percent / 100 * 120).toFixed(0);
  return 'hsl(' + hue + ', 100%, 50%)';
}

// Adds a row with given DOM for the file info and given coverage
// percentage to the provided table section, giving it the appropriate color
function addResultRow(tsection, fileInfo, percent) {
  var row = tsection.insertRow(-1);
  row.style.backgroundColor = percentColor(percent);

  var fileNameCell = row.insertCell(-1);
  fileNameCell.appendChild(fileInfo);

  var percentCell = row.insertCell(-1);
  percentCell.align = 'right';
  percentCell.appendChild(document.createTextNode(percent.toFixed(2) + '%'));
}

// Adds a result for given filename and coverage percentage to the first body
// of the table with id 'outTable'
function addFileResultRow(fileName, percent) {
  var table = document.getElementById('outTable');
  var tbody = table.tBodies[0];

  // We want a link to the .gcov file here
  var a = document.createElement('a');
  a.appendChild(document.createTextNode(fileName));

  var fragments = fileName.split('/');
  a.setAttribute('href', 'gcov/' + fragments[fragments.length - 1] + '.gcov');

  addResultRow(tbody, a, percent);
}

function addSummaryResultRow(summary, percent) {
  var table = document.getElementById('outTable');
  var tfoot = table.tFoot;

  addResultRow(tfoot, document.createTextNode(summary), percent);
}

function prettifySummary() {
  // Get the raw data from the <pre id='data'>
  var preNode = document.getElementById('data');
  var txt = (preNode.textContent ? preNode.textContent : preNode.innerText);
  var allLines = txt.split('\n');

  var currentFile;

  // Collect file names, percentages, and lines
  var allFiles = []; // array of name, coverage %, lines pairs
  for (var i = 0; i < allLines.length; ++i) {
    var line = allLines[i];
    var fileInfo = /File '(.*)'/.exec(line);
    if (fileInfo) {
      currentFile = fileInfo[1];
      // get rid of ./ if needed.
      if (currentFile.substring(0, 2) == './') {
        currentFile = currentFile.substring(2);
      }
    }

    var linesInfo = /Lines executed:(.*)% of (\d+)/.exec(line);
    if (linesInfo) {
      allFiles.push([currentFile, Number(linesInfo[1]), Number(linesInfo[2])]);
    }

    if (/No executable lines/.exec(line)) {
      allFiles.push([currentFile, 0, 0]);
    }
  }

  // Sort by filename
  allFiles.sort(function(a, b) {
    if (a[0] < b[0]) {
      return -1;
    } else if (a[0] == b[0]) {
      return 0;
    } else {
      return 1;
    }
  });

  // Append all results we want to table, coloring by coverage; and also compute
  // an overall number (which may include a few things we don't care about)
  var totalLines   = 0;
  var totalCovered = 0;
  for (var i = 0; i < allFiles.length; ++i) {
    var fileName = allFiles[i][0];
    var percent = allFiles[i][1];
    var lines = allFiles[i][2];

    // Skip paths -- we don't need coverage information for system headers
    if (fileName.charAt(0) == '/') {
      continue;
    }

    totalLines += lines;
    totalCovered += Math.round(lines * percent / 100);

    addFileResultRow(fileName, percent);
  }

  addSummaryResultRow('Total (' + totalCovered + '/' + totalLines +')',
                      totalCovered / totalLines * 100);
}
</script>
</head>
<body onload="prettifySummary()">
  <table id="outTable">
    <thead>
      <tr><th>File name</th><th>Coverage percentage</th></tr>
    </thead>
    <tbody></tbody>
    <tfoot style="font-weight:bold; "></tfoot>
  </table>
TEMPLATE_END
}

function usage {
  echo "Usage:" $0 "(--prepare | --summarize) path"
}

if [ -z $2 ]; then
  usage
  exit
fi

cd $2

case $1 in
--prepare)
  echo "Removing old .gcda files"
  find $2/out/Debug_Coverage -name '*.gcda' -delete;;
--summarize)
  summarize;;
*)
  usage;;
esac

