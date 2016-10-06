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
 * @fileoverview Code for extracting information from resource timing API.
 * This javascript is part of the AddInstrumentationFilter.
 *
 * @author satyanarayana@google.com (Satyanarayana Manyam)
 */

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * Iterates over the resource timing data and computes various metrics.
 * @return {string} containing all the metrics that are extracted.
 */
pagespeed.getResourceTimingData = function() {
  // performance.webkitGetEntries is deprecated and has been replaced by
  // performance.getEntries.
  // TODO(bharathbhushan): Remove webkitGetEntries once the traffic from older
  // chrome variants becomes small.
  if (window['performance'] &&
      (window['performance']['getEntries'] ||
       window['performance']['webkitGetEntries'])) {
    var totalFetchDuration = 0;
    var maxFetchDuration = 0;
    var numFetches = 0;

    var totalDnsDuration = 0;
    var numDnsLookups = 0;

    var totalConnectionTime = 0;
    var numConnections = 0;

    var totalTTFB = 0;
    var numTTFBRequests = 0;

    var totalBlockingTime = 0;
    var numRequestsBlocked = 0;

    var entryCountMap = {};

    var entries = window['performance']['getEntries'] ?
        window['performance'].getEntries() :
        window['performance'].webkitGetEntries();
    for (var i = 0; i < entries.length; i++) {
      var duration = entries[i]['duration'];
      if (duration > 0) {
        totalFetchDuration += duration;
        ++numFetches;
        maxFetchDuration = Math.max(maxFetchDuration, duration);
      }

      var connectTime = entries[i]['connectEnd'] - entries[i]['connectStart'];
      if (connectTime > 0) {
        totalConnectionTime += connectTime;
        ++numConnections;
      }

      var dnsTime = entries[i]['domainLookupEnd'] -
          entries[i]['domainLookupStart'];
      if (dnsTime > 0) {
        totalDnsDuration += dnsTime;
        ++numDnsLookups;
      }

      var initiator = entries[i]['initiatorType'];
      if (entryCountMap[initiator]) {
        ++entryCountMap[initiator];
      } else {
        entryCountMap[initiator] = 1;
      }

      var blockingTime = entries[i]['requestStart'] - entries[i]['fetchStart'];
      if (blockingTime > 0) {
        totalBlockingTime += blockingTime;
        ++numRequestsBlocked;
      }

      var ttfb = entries[i]['responseStart'] - entries[i]['requestStart'];
      if (ttfb > 0) {
        totalTTFB += ttfb;
        ++numTTFBRequests;
      }
    }

    return '&afd=' + (numFetches ? Math.round(
            totalFetchDuration / numFetches) : 0) +
        '&nfd=' + numFetches +
        '&mfd=' + Math.round(maxFetchDuration) +
        '&act=' + (numConnections ? Math.round(
            totalConnectionTime / numConnections) : 0) +
        '&nct=' + numConnections +
        '&adt=' + (numDnsLookups ? Math.round(
            totalDnsDuration / numDnsLookups) : 0) +
        '&ndt=' + numDnsLookups +
        '&abt=' + (numRequestsBlocked ? Math.round(
            totalBlockingTime / numRequestsBlocked) : 0) +
        '&nbt=' + numRequestsBlocked +
        '&attfb=' + (numTTFBRequests ? Math.round(
            totalTTFB / numTTFBRequests) : 0) +
        '&nttfb=' + numTTFBRequests +
        (entryCountMap['css'] ? '&rit_css=' + entryCountMap['css'] : '') +
        (entryCountMap['link'] ? '&rit_link=' + entryCountMap['link'] : '') +
        (entryCountMap['script'] ? '&rit_script=' +
            entryCountMap['script'] : '') +
        (entryCountMap['img'] ? '&rit_img=' + entryCountMap['img'] : '');
  }

  return '';
};

pagespeed['getResourceTimingData'] = pagespeed.getResourceTimingData;

