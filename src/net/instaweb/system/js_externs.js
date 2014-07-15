/*
 * Copyright 2013 Google Inc.
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

/**
 * @fileoverview  List of all external objects/functions referenced in js
 * flies under system/ but which are actually defined in some external file
 * (in this case in https://www.google.com/jsapi). Since we are not compiling
 * that file we need all those objects/functions to keep the same name. Listing
 * them here accomplishes that. This list is only used by console.js currently
 * but we could add externs needed by other files here in the future.
 *
 * @author sligocki@google.com (Shawn Ligocki)
 *
 * @externs
 */


/** @type {string} */
var pagespeedStatisticsUrl = '';


/** @param {function()} callback */
google.setOnLoadCallback = function(callback) {};
