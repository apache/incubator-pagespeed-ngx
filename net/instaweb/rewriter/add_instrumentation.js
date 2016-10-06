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

/**
 * @fileoverview Code for beaconing client page load time back to the server.
 * This javascript is part of the AddInstrumentationFilter.
 *
 * @author jud@google.com (Jud Porter)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 * @param {string} beaconUrlPrefix The prefix portion of the beacon url.
 * @param {string} event Event to trigger on, either 'load' or 'beforeunload'.
 * @param {string} extraParams Additional parameters to be added to the beacon.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 */
pagespeed.AddInstrumentation = function(beaconUrlPrefix, event, extraParams,
                                        htmlUrl) {
  this.beaconUrlPrefix_ = beaconUrlPrefix;
  this.event_ = event;
  this.extraParams_ = extraParams;
  this.htmlUrl_ = htmlUrl;
};

pagespeed['beaconUrl'] = '';

/**
 * Create beacon URL and send request to server.
 */
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var url = this.beaconUrlPrefix_;

  var oldStartTime = window['mod_pagespeed_start'];
  var currentTime = Number(new Date());
  var traditionalPLT = (currentTime - oldStartTime);

  // Handle a beacon url that already has query params.
  url += (url.indexOf('?') == -1) ? '?' : '&';
  url += 'ets=';
  url += (this.event_ == 'load') ? 'load:' : 'unload:';
  url += traditionalPLT;

  // We use navigation timing api for getting accurate start time. This api is
  // available in Internet Explorer 9+, Google Chrome 6+ and Firefox 7+.
  // If not present, we set the start time to when the rendering started.
  // TODO(satyanarayana): Remove the oldStartTime usages once
  // devconsole code has been updated to use the new "rload" param value.

  if (this.event_ == 'beforeunload' && window['mod_pagespeed_loaded']) {
    return;
  }

  url += '&r' + this.event_ + '=';
  if (window['performance']) {
    var timingApi = window['performance']['timing'];
    var navStartTime = timingApi['navigationStart'];
    var requestStartTime = timingApi['requestStart'];
    url += (timingApi[this.event_ + 'EventStart'] - navStartTime);
    url += '&nav=' + (timingApi['fetchStart'] - navStartTime);
    url += '&dns=' + (
        timingApi['domainLookupEnd'] - timingApi['domainLookupStart']);
    url += '&connect=' + (
        timingApi['connectEnd'] - timingApi['connectStart']);
    url += '&req_start=' + (requestStartTime - navStartTime);
    url += '&ttfb=' + (
        timingApi['responseStart'] - requestStartTime);
    url += '&dwld=' + (
        timingApi['responseEnd'] - timingApi['responseStart']);
    url += '&dom_c=' + (timingApi['domContentLoadedEventStart'] - navStartTime);

    if (window['performance']['navigation']) {
      url += '&nt=' + window['performance']['navigation']['type'];
    }
    var firstPaintTime = -1;
    if (timingApi['msFirstPaint']) {
      // IE.
      firstPaintTime = timingApi['msFirstPaint'];
    } else if (window['chrome'] && window['chrome']['loadTimes']) {
      // Chrome. Note that window.chrome.loadTimes returns a time in seconds.
      firstPaintTime = Math.floor(
          window['chrome']['loadTimes']()['firstPaintTime'] * 1000);
    }
    firstPaintTime = firstPaintTime - requestStartTime;
    if (firstPaintTime >= 0) {
      url += '&fp=' + firstPaintTime;
    }
  } else {
   url += traditionalPLT;
  }

  if (pagespeed['getResourceTimingData'] && window.parent == window) {
    url += pagespeed.getResourceTimingData();
  }

  url += (window.parent != window) ? '&ifr=1' : '&ifr=0';

  if (this.event_ == 'load') {
    window['mod_pagespeed_loaded'] = true;
    var numPrefetchedResources =
        window['mod_pagespeed_num_resources_prefetched'];
    if (numPrefetchedResources) {
      url += '&nrp=' + numPrefetchedResources;
    }
    var prefetchStartTime = window['mod_pagespeed_prefetch_start'];
    if (prefetchStartTime) {
      url += '&htmlAt=' + (oldStartTime - prefetchStartTime);
    }
  }

  if (pagespeed['criticalCss']) {
    var cc = pagespeed['criticalCss'];
    url += '&ccis=' + cc['total_critical_inlined_size'] +
           '&cces=' + cc['total_original_external_size'] +
           '&ccos=' + cc['total_overhead_size'] +
           '&ccrl=' + cc['num_replaced_links'] +
           '&ccul=' + cc['num_unreplaced_links'];
  }

  // Collect devicePixelRatios to find common values.
  // Note: This may append =undefined for old browsers.
  url += '&dpr=' + window.devicePixelRatio;

  if (this.extraParams_ != '') {
    url += this.extraParams_;
  }
  if (document.referrer) {
    url += '&ref=' + encodeURIComponent(document.referrer);
  }
  url += '&url=' + encodeURIComponent(this.htmlUrl_);

  pagespeed['beaconUrl'] = url;
  new Image().src = url;
};

/**
 * Initialize instrumentation beacon.
 * @param {string} beaconUrl Url of beacon.
 * @param {string} event Event to trigger on, either 'load' or 'beforeunload'.
 * @param {string} extraParams Additional parameters to be added to the beacon.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 */
pagespeed.addInstrumentationInit = function(beaconUrl, event, extraParams,
                                            htmlUrl) {

  var temp = new pagespeed.AddInstrumentation(beaconUrl, event, extraParams,
                                              htmlUrl);
  if (window.addEventListener) {
    window.addEventListener(event, function() { temp.sendBeacon(); }, false);
  } else {
    window.attachEvent('on' + event, function() { temp.sendBeacon(); });
  }

};

pagespeed['addInstrumentationInit'] = pagespeed.addInstrumentationInit;
