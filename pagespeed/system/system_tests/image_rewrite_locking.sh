#!/bin/bash
#
# Copyright 2016 Google Inc.
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
# Ideally the system should only rewrite an image once when when it gets
# a burst of requests.  A bug was fixed where we were not obeying a
# failed lock and were rewriting it potentially many times.  It still
# happens fairly often that we rewrite the image twice.  I am not sure
# why that is, except to observe that our locks are 'best effort'.
start_test A burst of image requests should yield only one two rewrites.
URL="$EXAMPLE_ROOT/images/Puzzle.jpg?a=$RANDOM"
start_image_rewrites=$(scrape_stat image_rewrites)
echo Running burst of 20x: \"wget -q -O - $URL '|' wc -c\"
for ((i = 0; i < 20; ++i)); do
  echo -n $(wget -q -O - $URL | wc -c) ""
done
echo "... done"
sleep 1
num_image_rewrites=$(($(scrape_stat image_rewrites) - start_image_rewrites))
check [ $num_image_rewrites = 1 -o $num_image_rewrites = 2 ]
URL=""
