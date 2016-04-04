/*
 * Copyright 2014 Google Inc.
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
 * @fileoverview Shared set of defined externs used by static JS files. This
 * file is included by default when building with closure.
 */


/** @type {string} */
var blocked_resource_url_pattern;


/** @type {boolean} */
var deferjs_enabled;


/** @type {string} */
var deferjs_url;


// This is set by a script in the head inserted by add_instrumentaiton.
/** @type {number} */
window.mod_pagespeed_start;

window.pagespeed;
window.pagespeed.deferJs;
window.pagespeed.deferJs.registerScriptTags;
window.pagespeed.deferJs.run;
window.pagespeed.getResourceTimingData;
window.pagespeed.shouldAllowResource;
window.performance.getEntries;
window.performance.webkitGetEntries;

window.pagespeed.highPriorityDeferJs;
window.pagespeed.lowPriorityDeferJs;

window.pagespeed.CriticalImages;
window.pagespeed.CriticalImages.checkCriticalImages;
window.pagespeed.CriticalImages.checkImageForCriticality;
