/*
 * Copyright 2012 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// This js redefines the non-deterministic parts of JS, such as Date and
// Math.random, so that contents of the rewritten page are more or less
// deterministic when loaded repeatedly. This can help in reducing variance
// found while measuring page load metrics over time.

var orig_date = Date;
var random_count = 0;
var date_count = 0;
var random_seed = 0.462;
var time_seed = 1204251968254;
var random_count_threshold = 25;
var date_count_threshold = 25;
Math.random = function() {
  random_count++;
  if (random_count > random_count_threshold) {
   random_seed += 0.1;
   random_count = 1;
  }
  return (random_seed % 1);
};
Date = function() {
  if (this instanceof Date) {
    date_count++;
    if (date_count > date_count_threshold) {
      time_seed += 50;
      date_count = 1;
    }
    switch (arguments.length) {
    case 0: return new orig_date(time_seed);
    case 1: return new orig_date(arguments[0]);
    default: return new orig_date(arguments[0], arguments[1],
       arguments.length >= 3 ? arguments[2] : 1,
       arguments.length >= 4 ? arguments[3] : 0,
       arguments.length >= 5 ? arguments[4] : 0,
       arguments.length >= 6 ? arguments[5] : 0,
       arguments.length >= 7 ? arguments[6] : 0);
    }
  }
  return new Date().toString();
};
Date.__proto__ = orig_date;
Date.prototype.constructor = Date;
orig_date.now = function() {
  return new Date().getTime();
};
