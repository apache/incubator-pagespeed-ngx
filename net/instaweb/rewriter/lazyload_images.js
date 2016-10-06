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
 * @fileoverview Code for lazy loading images on the client side.
 * This javascript is part of LazyloadImages filter.
 *
 * @author nikhilmadan@google.com (Nikhil Madan)
 */

goog.require('pagespeedutils');

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See https://developers.google.com/closure/compiler/docs/api-tutorial3#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];



/**
 * @constructor
 * @param {string} blankImageSrc The blank placeholder image used for images
 *     that are not visible.
 */
pagespeed.LazyloadImages = function(blankImageSrc) {
  /**
   * Array of images that have been deferred.
   * @type {Array.<Element>}
   * @private
   */
  this.deferred_ = [];

  /**
   * The number of additional pixels above or below the current viewport
   * within which to fetch images. This can be configured for producing a
   * smoother experience for the user while scrolling down by fetching images
   * just outside the current viewport. Should be a positive number.
   * @type {number}
   * @private
   */
  this.buffer_ = 0;

  /**
   * Boolean indicating whether we should force loading of all images,
   * irrespective of whether or not they are visible. This is set to true when
   * we want to load all images after onload is triggered.
   * @type {boolean}
   * @private
   */
  this.force_load_ = false;

  /**
   * The blank placeholder image used for images that are not visible.
   * @type {string}
   * @private
   */
  this.blank_image_src_ = blankImageSrc;

  /**
   * The ID of the event that is currently scheduled to be triggered to detect
   * and display visible images.
   * @private
   */
  this.scroll_timer_ = null;

  /**
   * The time the last scroll event was fired in milliseconds since epoch.
   * @type {number}
   * @private
   */
  this.last_scroll_time_ = 0;

  /**
   * The minimum time in milliseconds between two scroll events being fired.
   * @type {number}
   * @private
   */
  this.min_scroll_time_ = 200;

  /**
   * Boolean indicating whether the onload of the page has been triggered.
   * @type {boolean}
   * @private
   */
  this.onload_done_ = false;

  /**
   * Tracks how many images are left to load before firing the after-onload
   * critical image beacon.
   * @private {number}
   */

  this.imgs_to_load_before_beaconing_ = 0;
};


/**
 * Count the number of total imgs that will be lazyloaded.
 * @return {number} The number of imgs deferred.
 * @private
 */
pagespeed.LazyloadImages.prototype.countDeferredImgs_ = function() {
  var deferredImgCount = 0;
  var imgs = document.getElementsByTagName('img');
  for (var i = 0, img; img = imgs[i]; i++) {
    if (img.src.indexOf(this.blank_image_src_) != -1 &&
        this.hasAttribute_(img, 'data-pagespeed-lazy-src')) {
      deferredImgCount++;
    }
  }
  return deferredImgCount;
};


/**
 * Returns a bounding box for the currently visible part of the window.
 * @return {{
 *     top: (number),
 *     bottom: (number),
 *     height: (number)
 * }}
 * @private
 */
pagespeed.LazyloadImages.prototype.viewport_ = function() {
  var scrollY = 0;
  if (typeof(window.pageYOffset) == 'number') {
    scrollY = window.pageYOffset;
  } else if (document.body && document.body.scrollTop) {
    scrollY = document.body.scrollTop;
  } else if (document.documentElement && document.documentElement.scrollTop) {
    scrollY = document.documentElement.scrollTop;
  }
  var height = window.innerHeight || document.documentElement.clientHeight ||
      document.body.clientHeight;
  return {
    top: scrollY,
    bottom: scrollY + height,
    height: height
  };
};


/**
 * Returns the position of the top of the given element with respect to the top
 * of the page.
 * @param {Element} element DOM element to measure offset of.
 * @return {number} The position of the element in the page.
 * @private
 */
pagespeed.LazyloadImages.prototype.compute_top_ = function(element) {
  var position = element.getAttribute('data-pagespeed-lazy-position');
  if (position) {
    return parseInt(position, 0);
  }
  position = element.offsetTop;
  var parent = element.offsetParent;
  if (parent) {
    position += this.compute_top_(parent);
  }
  // Since position is absolute, it should always be >= 0.
  position = Math.max(position, 0);
  element.setAttribute('data-pagespeed-lazy-position', position);
  return position;
};


/**
 * Returns a bounding box for the given element.
 * @param {Element} element DOM element whose position is to be computed.
 * @return {{
 *     top: (number),
 *     bottom: (number)
 * }}
 * @private
 */
pagespeed.LazyloadImages.prototype.offset_ = function(element) {
  var top_position = this.compute_top_(element);
  return {
    top: top_position,
    bottom: top_position + element.offsetHeight
  };
};


/**
 * Returns the value of the given style property for the element.
 * @param {Element} element DOM element whose style property is to be computed.
 * @param {string} property The CSS property we are trying to find.
 * @return {string} The given property if found, and empty string otherwise.
 * @private
 */
pagespeed.LazyloadImages.prototype.getStyle_ = function(element, property) {
  if (element.currentStyle) {
    // IE.
    return element.currentStyle[property];
  }
  if (document.defaultView &&
      document.defaultView.getComputedStyle) {
    // Other browsers.
    var style = document.defaultView.getComputedStyle(element, null);
    if (style) {
      return style.getPropertyValue(property);
    }
  }
  if (element.style && element.style[property]) {
    return element.style[property];
  }
  // Fallback.
  return '';
};


/**
 * Returns true if an element is currently visible or within the buffer.
 * @param {Element} element The DOM element to check for visibility.
 * @return {boolean} True if the element is visible.
 * @private
 */
pagespeed.LazyloadImages.prototype.isVisible_ = function(element) {
  if (!this.onload_done_ &&
      (element.offsetHeight == 0 || element.offsetWidth == 0)) {
    // The element is most likely hidden.
    // Since we don't know when the element will become visible, we'll try to
    // load the image after onload, so that we can improve PLT.
    return false;
  }

  var element_position = this.getStyle_(element, 'position');
  if (element_position == 'relative') {
    // TODO(ksimbili): Check if this code is still needed. Find out if any other
    // alternative will solve this.
    // If the element contains a "position: relative" style attribute, assume
    // it is visible since getBoundingClientRect() doesn't seem to work
    // correctly here.
    return true;
  }

  var viewport = this.viewport_();
  var rect = element.getBoundingClientRect();
  var top_diff, bottom_diff;
  if (rect) {
    // getBoundingClientRect() gives the position with respect to the current
    // viewport.
    top_diff = rect.top - viewport.height;
    bottom_diff = rect.bottom;
  } else {
    var position = this.offset_(element);
    top_diff = position.top - viewport.bottom;
    bottom_diff = position.bottom - viewport.top;
  }
  return (top_diff <= this.buffer_ &&
          bottom_diff + this.buffer_ >= 0);
};


/**
 * Loads the given element if it is visible. Otherwise, adds it to the deferred
 * queue. Note that if force_load_ is true, the visibility check is skipped.
 * Also sets the onload handler to the image beaconing onload code if required.
 * @param {Element} element The element to check for visibility.
 */
pagespeed.LazyloadImages.prototype.loadIfVisibleAndMaybeBeacon =
    function(element) {
  // Override this element's attributes if they haven't already been overridden.
  this.overrideAttributeFunctionsInternal_(element);

  var context = this;
  window.setTimeout(function() {
    var data_src = element.getAttribute('data-pagespeed-lazy-src');
    if (data_src) {
      if ((context.force_load_ || context.isVisible_(element)) &&
          element.src.indexOf(context.blank_image_src_) != -1) {
        // Only replace the src if the old value is the one we set. Note that we
        // do a 'contains' match to handle the case when the blank src is a url
        // starting with //. It is possible that a script has already changed
        // the url, in which case, we should not modify it.
        // Remove the element from the DOM and add it back in, since simply
        // setting the src doesn't seem to always work in chrome.
        var parent_node = element.parentNode;
        var next_sibling = element.nextSibling;
        if (parent_node) {
          parent_node.removeChild(element);
        }

        // Restore the old functions. Make sure that this element actually has
        // the old function before restoring it. Otherwise, we'll end up setting
        // getAttribute to undefined if we haven't seen this node before.
        if (element._getAttribute) {
          element.getAttribute = element._getAttribute;
        }
        // Remove attributes that are no longer needed.
        element.removeAttribute('onload');
        if (element.tagName && element.tagName == 'IMG') {
          // If CriticalImages is defined, we should add the per-image
          // checkImageForCriticality logic because the lazyload_images_filter
          // would have removed this.
          if (pagespeed.CriticalImages) {
            pagespeedutils.addHandler(element, 'load', function(e) {
              pagespeed.CriticalImages.checkImageForCriticality(this);
              if (context.onload_done_) {
                context.imgs_to_load_before_beaconing_--;
                if (context.imgs_to_load_before_beaconing_ == 0) {
                  pagespeed.CriticalImages.checkCriticalImages();
                }
              }
            });
          }
        }
        element.removeAttribute('data-pagespeed-lazy-src');
        element.removeAttribute('data-pagespeed-lazy-replaced-functions');
        // If there was a next sibling, insert element before it.
        if (parent_node) {
          parent_node.insertBefore(element, next_sibling);
        }

        // Set srcset before src to avoid potentially loading 2 sizes.
        var srcset = element.getAttribute('data-pagespeed-lazy-srcset');
        if (srcset) {
          element.srcset = srcset;
          element.removeAttribute('data-pagespeed-lazy-srcset');
        }

        // Set the src back to the original.
        element.src = data_src;
      } else {
        context.deferred_.push(element);
      }
    }
  }, 0);
};

pagespeed.LazyloadImages.prototype['loadIfVisibleAndMaybeBeacon'] =
    pagespeed.LazyloadImages.prototype.loadIfVisibleAndMaybeBeacon;


/**
 * Loads all the images irrespective of whether or not they are in the
 * viewport.
 */
pagespeed.LazyloadImages.prototype.loadAllImages = function() {
  this.force_load_ = true;
  this.loadVisible_();
};

pagespeed.LazyloadImages.prototype['loadAllImages'] =
    pagespeed.LazyloadImages.prototype.loadAllImages;


/**
 * Loads the visible elements in the deferred queue. Also, removes the loaded
 * elements from the deferred queue.
 * @private
 */
pagespeed.LazyloadImages.prototype.loadVisible_ = function() {
  var old_deferred = this.deferred_;
  var len = old_deferred.length;
  this.deferred_ = [];
  for (var i = 0; i < len; ++i) {
    this.loadIfVisibleAndMaybeBeacon(old_deferred[i]);
  }
};


/**
 * Returns true if the given element has an attribute with the given name.
 * @param {Element} element The element whose attributes we are checking.
 * @param {string} attribute The attribute we are checking for.
 * @return {boolean} True if the element has the given attribute.
 * @private
 */
pagespeed.LazyloadImages.prototype.hasAttribute_ =
    function(element, attribute) {
  if (element.getAttribute_) {
    return element.getAttribute_(attribute) != null;
  }
  return element.getAttribute(attribute) != null;
};


/**
 * Overrides attribute functions for all lazily loaded images if they have not
 * already been overridden.
 */
pagespeed.LazyloadImages.prototype.overrideAttributeFunctions = function() {
  var images = document.getElementsByTagName('img');
  for (var i = 0, element; element = images[i]; i++) {
    if (this.hasAttribute_(element, 'data-pagespeed-lazy-src')) {
      this.overrideAttributeFunctionsInternal_(element);
    }
  }
};

pagespeed.LazyloadImages.prototype['overrideAttributeFunctions'] =
    pagespeed.LazyloadImages.prototype.overrideAttributeFunctions;


/**
 * Overrides attribute functions for the given image if they have not already
 * been overridden.
 * @param {Element} element The element whose attribute functions should be
 *     overridden.
 * @private
 */
pagespeed.LazyloadImages.prototype.overrideAttributeFunctionsInternal_ =
    function(element) {
  var context = this;
  if (!this.hasAttribute_(element, 'data-pagespeed-lazy-replaced-functions')) {
    element._getAttribute = element.getAttribute;
    element.getAttribute = function(name) {
      if (name.toLowerCase() == 'src' &&
          context.hasAttribute_(this, 'data-pagespeed-lazy-src')) {
        name = 'data-pagespeed-lazy-src';
      }
      return this._getAttribute(name);
    };
    element.setAttribute('data-pagespeed-lazy-replaced-functions', '1');
  }
};


/**
 * Initializes the lazyload module.
 * @param {boolean} loadAfterOnload If true, all images will be loaded at
 *     window.onload.
 * @param {string} blankImageSrc The blank placeholder image used for images
 *     that are not visible.
 * is fired. Otherwise, load images on scrolling as they become visible.
 */
pagespeed.lazyLoadInit = function(loadAfterOnload, blankImageSrc) {
  var context = new pagespeed.LazyloadImages(blankImageSrc);
  pagespeed['lazyLoadImages'] = context;

  // Add an event to the onload handler to check if any new images have now
  // become visible because of reflows or DOM manipulation. If loadAfterOnload
  // is true, load all images on the page.
  var lazy_onload = function() {
    context.onload_done_ = true;
    context.force_load_ = loadAfterOnload;
    // Set the buffer to 200 after onload, so that images that are just below
    // the fold are pre-loaded and scrolling is smoother.
    context.buffer_ = 200;

    if (pagespeed.CriticalImages) {
      // We don't want to fire the critical image beacon onload check until all
      // images have been loaded, so keep a count of the number of images we are
      // waiting for. If all images have already been loaded, then go ahead and
      // fire the critical image beacon check now.
      context.imgs_to_load_before_beaconing_ = context.countDeferredImgs_();
      if (context.imgs_to_load_before_beaconing_ == 0) {
        pagespeed.CriticalImages.checkCriticalImages();
      }
    }

    context.loadVisible_();
  };
  pagespeedutils.addHandler(window, 'load', lazy_onload);

  // Pre-load the blank image placeholder.
  if (blankImageSrc.indexOf('data') != 0) {
    new Image().src = blankImageSrc;
  }

  // Always attach the onscroll, even if the onload option is enabled.
  var lazy_onscroll = function() {
    if (context.onload_done_ && loadAfterOnload) {
      return;
    }

    // NOTE: We don't delay any scroll event by greater than min_scroll_time_.
    if (!context.scroll_timer_) {
      // Check that a scroll_timer_ has not been attached already.
      var now = new Date().getTime();
      var timeout_ms = context.min_scroll_time_;
      if (now - context.last_scroll_time_ > context.min_scroll_time_) {
        // If the time since the last scroll is greater than min_scroll_time_,
        // load visible images immediately.
        timeout_ms = 0;
      }
      // Otherwise, load images after min_scroll_time_.
      context.scroll_timer_ = window.setTimeout(function() {
        context.last_scroll_time_ = new Date().getTime();
        context.loadVisible_();
        context.scroll_timer_ = null;
      }, timeout_ms);
    }
  };
  pagespeedutils.addHandler(window, 'scroll', lazy_onscroll);
  pagespeedutils.addHandler(window, 'resize', lazy_onscroll);
};

pagespeed['lazyLoadInit'] = pagespeed.lazyLoadInit;
