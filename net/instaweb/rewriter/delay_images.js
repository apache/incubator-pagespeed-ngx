/*
 * Copyright 2011 Google Inc.
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
 * @fileoverview Code for loading high resolution critical images.
 * This javascript is part of DelayImages filter.
 *
 * @author pulkitg@google.com (Pulkit Goyal)
 */

goog.require('pagespeedutils');

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];



/**
 * @constructor
 */
pagespeed.DelayImages = function() {
  /**
   * Boolean that controls whether the event handlers for lazy load are already
   * registered.
   * @type {boolean}
   * @private
   */
  this.lazyLoadHighResHandlersRegistered_ = false;

  /**
   * Boolean that controls the logic to replace low res images with high res
   * only once.
   * @type {boolean}
   * @private
   */
  this.highResReplaced_ = false;
};


/**
 * For given elements, replace src with data-pagespeed-high-res-src if present.
 * @param {!NodeList<!Element>} elements list of DOM elements to check.
 */
pagespeed.DelayImages.prototype.replaceElementSrc = function(elements) {
  for (var i = 0; i < elements.length; ++i) {
    var src = elements[i].getAttribute('data-pagespeed-high-res-src');
    if (src) {
      elements[i].setAttribute('src', src);
    }
  }
};
pagespeed.DelayImages.prototype['replaceElementSrc'] =
    pagespeed.DelayImages.prototype.replaceElementSrc;


/**
 * Register the event handlers to lazy load the high res images. This is
 * called only when lazyload_high_res_experimental flag is enabled.
 */
pagespeed.DelayImages.prototype.registerLazyLoadHighRes = function() {
  // Add event handlers only once
  if (this.lazyLoadHighResHandlersRegistered_) {
    this.highResReplaced_ = false;
    return;
  }

  var elem = document.body;
  var interval = 500;
  var tapStart, tapEnd = 0;

  var me = this;

  this.highResReplaced = false;
  if ('ontouchstart' in elem) {
    pagespeedutils.addHandler(elem, 'touchstart', function(e) {
      tapStart = pagespeedutils.now();
    });

    pagespeedutils.addHandler(elem, 'touchend', function(e) {
      tapEnd = pagespeedutils.now();
      // Load the high res images if there is a multi-touch or if the tap
      // duration is less than 500ms i.e single click. The timer catches the
      // click event sooner than the click handler on most phones.
      if ((e.changedTouches != null && e.changedTouches.length == 2) ||
          (e.touches != null && e.touches.length == 2) ||
          tapEnd - tapStart < interval) {
        me.loadHighRes();
      }
    });
  } else {
    pagespeedutils.addHandler(window, 'click', function(e) {
      me.loadHighRes();
    });
  }
  pagespeedutils.addHandler(window, 'load', function(e) {
    me.loadHighRes();
  });
  this.lazyLoadHighResHandlersRegistered_ = true;
};

pagespeed.DelayImages.prototype['registerLazyLoadHighRes'] =
    pagespeed.DelayImages.prototype.registerLazyLoadHighRes;


/**
 * Triggered from event handlers that were previously registered. Replaces
 * low res images with high res sources.
 */
pagespeed.DelayImages.prototype.loadHighRes = function() {
  if (!this.highResReplaced_) {
    this.replaceWithHighRes();
    this.highResReplaced_ = true;
  }
};


/**
 * Replaces low resolution image with high resolution image.
 */
pagespeed.DelayImages.prototype.replaceWithHighRes = function() {
  this.replaceElementSrc(document.getElementsByTagName('img'));
  this.replaceElementSrc(document.getElementsByTagName('input'));
};
pagespeed.DelayImages.prototype['replaceWithHighRes'] =
    pagespeed.DelayImages.prototype.replaceWithHighRes;


/**
 * Initializes the delay images module.
 */
pagespeed.delayImagesInit = function() {
  var temp = new pagespeed.DelayImages();
  pagespeed['delayImages'] = temp;
};

pagespeed['delayImagesInit'] = pagespeed.delayImagesInit;
