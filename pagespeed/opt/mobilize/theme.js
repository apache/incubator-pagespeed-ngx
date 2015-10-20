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

goog.provide('mob.Theme');
goog.provide('mob.theme');

goog.require('mob.Logo');
goog.require('mob.util.ThemeData');



/**
 * Creates a context for Pagespeed theme extraction.
 * @param {function(!mob.util.ThemeData)} doneCallback
 * @constructor
 */
mob.Theme = function(doneCallback) {
  /**
   * Callback to invoke when this object finishes its work.
   * @private {?function(!mob.util.ThemeData)}
   */
  this.doneCallback_ = doneCallback;
};


/**
 * @param {!Array.<!mob.LogoCandidate>} candidates
 */
mob.Theme.prototype.logoComplete = function(candidates) {
  if (this.doneCallback_) {
    var themeData;
    if (candidates.length >= 1) {
      var candidate = candidates[0];
      themeData =
          new mob.util.ThemeData(candidate.logoRecord.foregroundImage.src,
                                 candidate.background, candidate.foreground);
    } else {
      themeData = new mob.util.ThemeData(null, [255, 255, 255], [0, 0, 0]);
    }
    var doneCallback = this.doneCallback_;
    this.doneCallback_ = null;
    doneCallback(themeData);
  }
};


/**
 * Extract theme of the page. This is the entry method.
 * @param {function(!mob.util.ThemeData)} doneCallback
 */
mob.theme.extractTheme = function(doneCallback) {
  var mobTheme = new mob.Theme(doneCallback);
  var mobLogo = new mob.Logo();
  mobLogo.run(goog.bind(mobTheme.logoComplete, mobTheme),
              1 /* numCandidates */);
};
