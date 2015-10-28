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
 *
 * Author: jmarantz@google.com (Joshua Marantz)
 */


goog.provide('mob');
goog.provide('mob.Mob');

goog.require('goog.events.EventType');
goog.require('mob.Nav');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ThemeData');


// Setup some initial beacons to track page loading.
mob.util.sendBeaconEvent(mob.util.BeaconEvents.INITIAL_EVENT);

window.addEventListener(goog.events.EventType.LOAD, function() {
  mob.util.sendBeaconEvent(mob.util.BeaconEvents.LOAD_EVENT);
});



/**
 * Creates a context for PageSpeed mobilization.
 *
 * @constructor
 */
mob.Mob = function() {
  /**
   * Navigation context.
   * @private {?mob.Nav}
   */
  this.mobNav_ = null;
};


/**
 * Initialize mobilization.
 */
mob.Mob.prototype.initialize = function() {
  var themeData = new mob.util.ThemeData(
      window.psMobLogoUrl || '', window.psMobForegroundColor || [255, 255, 255],
      window.psMobBackgroundColor || [0, 0, 0]);
  this.mobNav_ = new mob.Nav();
  this.mobNav_.run(themeData);
};


/**
 * Mob object to be used by the exported functions below.
 * @private {!mob.Mob}
 */
var mobilizer_ = new mob.Mob();


/**
 * Called by C++-created JavaScript.
 * TODO(jud): Remove from mobilize_rewrite_filter.cc.
 * @export
 */
function psSetDebugMode() {}


/**
 * Called by C++-created JavaScript.
 * TODO(jud): Remove from mobilize_rewrite_filter.cc.
 * @export
 */
function psRemoveProgressBar() {}


/**
 * Main entry point to mobilization.
 * @export
 */
var psStartMobilization = function() {
  mobilizer_.initialize();
};
