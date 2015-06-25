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
 * Author: huibao@google.com (Huibao Lin)
 */

goog.provide('pagespeed.MobTheme');

goog.require('pagespeed.MobUtil');



/**
 * Creates a context for Pagespeed theme extraction.
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 * @constructor
 */
pagespeed.MobTheme = function(doneCallback) {
  /**
   * Callback to invoke when this object finishes its work.
   * @private {?function(!pagespeed.MobUtil.ThemeData)}
   */
  this.doneCallback_ = doneCallback;
};


/**
 * Compute color and synthesize logo span.
 * @param {?string} logoUrl
 * @param {!goog.color.Rgb} backgroundColor
 * @param {!goog.color.Rgb} foregroundColor
 * @return {!pagespeed.MobUtil.ThemeData}
 */
pagespeed.MobTheme.createThemeData = function(logoUrl, backgroundColor,
                                              foregroundColor) {
  var themeData = new pagespeed.MobUtil.ThemeData(logoUrl, backgroundColor,
                                                  foregroundColor);

  // Export theme info if we are asked to.
  if (window.psMobPrecompute) {
    window.psMobLogoUrl = logoUrl;
    window.psMobBackgroundColor = backgroundColor;
    window.psMobForegroundColor = foregroundColor;
  }
  return themeData;
};


/**
 * @param {!Array.<!pagespeed.MobLogoCandidate>} candidates
 */
pagespeed.MobTheme.prototype.logoComplete = function(candidates) {
  if (this.doneCallback_) {
    var themeData;
    if (candidates.length >= 1) {
      var candidate = candidates[0];
      themeData = pagespeed.MobTheme.createThemeData(
          candidate.logoRecord.foregroundImage.src, candidate.background,
          candidate.foreground);
    } else {
      themeData = pagespeed.MobTheme.createThemeData(
          null, [255, 255, 255], [0, 0, 0]);
    }
    var doneCallback = this.doneCallback_;
    this.doneCallback_ = null;
    doneCallback(themeData);
  }
};


/**
 * Returns true if precomputed theme information is available.
 * @return {boolean}
 */
pagespeed.MobTheme.precomputedThemeAvailable = function() {
  return Boolean(window.psMobBackgroundColor && window.psMobForegroundColor &&
                 !window.psMobPrecompute);
};


/**
 * Extract theme of the page. This is the entry method.
 * @param {!pagespeed.MobLogo} mobLogo
 * @param {function(!pagespeed.MobUtil.ThemeData)} doneCallback
 */
pagespeed.MobTheme.extractTheme = function(mobLogo, doneCallback) {
  // See if there is a precomputed theme + logo (and we are not asked to do
  // the computation ourselves).
  if (window.psMobBackgroundColor && window.psMobForegroundColor &&
      !window.psMobPrecompute) {
    var themeData = pagespeed.MobTheme.createThemeData(
        window.psMobLogoUrl, window.psMobBackgroundColor,
        window.psMobForegroundColor);
    doneCallback(themeData);
  } else {
    var mobTheme = new pagespeed.MobTheme(doneCallback);
    mobLogo.run(goog.bind(mobTheme.logoComplete, mobTheme),
                1 /* numCandidates */);
  }
};
