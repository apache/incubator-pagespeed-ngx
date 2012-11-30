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
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 * @param {string} beaconUrl Url of beacon.
 * @param {string} event Event to trigger on, either 'load' or 'beforeunload'.
 * @param {string} headerFetchTime Time to fetch header.
 * @param {string} originFetchTime Time to fetch origin.
 * @param {string} experimentId Id of current experiment.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 */
pagespeed.AddInstrumentation = function(beaconUrl, event, headerFetchTime,
                                        originFetchTime, experimentId,
                                        htmlUrl) {
  this.beaconUrl_ = beaconUrl;
  this.event_ = event;
  this.headerFetchTime_ = headerFetchTime;
  this.originFetchTime_ = originFetchTime;
  this.experimentId_ = experimentId;
  this.htmlUrl_ = htmlUrl;
};

/**
 * Create beacon URL and send request to server.
 */
pagespeed.AddInstrumentation.prototype.sendBeacon = function() {
  var url = this.beaconUrl_;
  url += (this.event_ == 'load') ? 'load:' : 'unload:';
  url += Number(new Date() - window['mod_pagespeed_start']);

  if (this.event_ == 'beforeunload' && window['mod_pagespeed_loaded']) {
    return;
  }
  url += (window.parent != window) ? '&ifr=1' : '&ifr=0';
  if (this.event_ == 'load') {
    window['mod_pagespeed_loaded'] = true;
    if (window['mod_pagespeed_num_resources_prefetched']) {
      url += '&nrp=' + window['mod_pagespeed_num_resources_prefetched'];
    }
    if (window['mod_pagespeed_prefetch_start']) {
      url += '&htmlAt=' + (window['mod_pagespeed_start'] -
                           window['mod_pagespeed_prefetch_start']);
    }
  }

  if (this.headerFetchTime_ != '') {
    url += '&hft=' + this.headerFetchTime_;
  }
  if (this.originFetchTime_ != '') {
    url += '&ft=' + this.originFetchTime_;
  }
  if (this.experimentId_ != '') {
    url += '&exptid=' + this.experimentId_;
  }
  url += '&url=' + encodeURIComponent(this.htmlUrl_);

  new Image().src = url;
};

/**
 * Initialize instrumentation beacon.
 * @param {string} beaconUrl Url of beacon.
 * @param {string} event Event to trigger on, either 'load' or 'beforeunload'.
 * @param {string} headerFetchTime Time to fetch header.
 * @param {string} originFetchTime Time to fetch origin.
 * @param {string} experimentId Id of current experiment.
 * @param {string} htmlUrl Url of the page the beacon is being inserted on.
 */
pagespeed.addInstrumentationInit = function(beaconUrl, event, headerFetchTime,
                                            originFetchTime, experimentId,
                                            htmlUrl) {

  var temp = new pagespeed.AddInstrumentation(beaconUrl, event, headerFetchTime,
                                              originFetchTime, experimentId,
                                              htmlUrl);
  if (window.addEventListener) {
    window.addEventListener(event, function() { temp.sendBeacon() }, false);
  } else {
    window.attachEvent('on' + event, function() { temp.sendBeacon() });
  }

};

pagespeed['addInstrumentationInit'] = pagespeed.addInstrumentationInit;
