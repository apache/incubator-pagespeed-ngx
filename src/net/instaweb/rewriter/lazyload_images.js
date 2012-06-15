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

// Exporting functions using quoted attributes to prevent js compiler from
// renaming them.
// See http://code.google.com/closure/compiler/docs/api-tutorial3.html#dangers
window['pagespeed'] = window['pagespeed'] || {};
var pagespeed = window['pagespeed'];

/**
 * @constructor
 * @param {string} blankImageSrc The blank placeholder image used for images
 *   that are not visible.
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
  var position = element.getAttribute('pagespeed_lazy_position');
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
  element.setAttribute('pagespeed_lazy_position', position);
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
  var element_position = this.getStyle_(element, 'position');
  if (element_position == 'relative') {
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
 * @param {Element} element The element to check for visibility.
 */
pagespeed.LazyloadImages.prototype.loadIfVisible = function(element) {
  var context = this;
  window.setTimeout(function() {
    var data_src = element.getAttribute('pagespeed_lazy_src');
    if (data_src != null) {
      if ((context.force_load_ || context.isVisible_(element)) &&
          element.src == context.blank_image_src_) {
        // Only replace the src if the old value is the one we set. It is
        // possible that a script has already changed it, in which case, we
        // should not try to modify it.
        element.removeAttribute('pagespeed_lazy_src');
        element.removeAttribute('onload');
        element.removeAttribute('src');
        // Create a new image element and replace the old image with the new
        // one since setting the src doesn't seem to always work in chrome.
        var newElement = new Image();
        // Copy attributes.
        for (var i = 0, a = element.attributes, n = a.length; i < n; ++i) {
          newElement.setAttribute(a[i].name, a[i].value);
        }
        newElement.setAttribute('src', data_src);

        // Replace the old element with the new one.
        element.parentNode.replaceChild(newElement, element);
      } else {
        context.deferred_.push(element);
      }
    }
  }, 0);
};

pagespeed.LazyloadImages.prototype['loadIfVisible'] =
    pagespeed.LazyloadImages.prototype.loadIfVisible;

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
    this.loadIfVisible(old_deferred[i]);
  }
};

/**
 * Runs the function when event is triggered.
 * @param {Window|Element} elem Element to attach handler.
 * @param {string} ev Name of the event.
 * @param {function()} func New onload handler.
 *
 * TODO(nikhilmadan): Avoid duplication with the DeferJs code.
 */
pagespeed.addHandler = function(elem, ev, func) {
  if (elem.addEventListener) {
    elem.addEventListener(ev, func, false);
  } else if (elem.attachEvent) {
    elem.attachEvent('on' + ev, func);
  } else {
    var oldHandler = elem['on' + ev];
    elem['on' + ev] = function() {
      func.call(this);
      if (oldHandler) {
        oldHandler.call(this);
      }
    }
  }
};

/**
 * Initializes the lazyload module.
 * @param {boolean} loadAfterOnload If true, load images when the onload event.
 * @param {string} blankImageSrc The blank placeholder image used for images
 *   that are not visible.
 * is fired. Otherwise, load images on scrolling as they become visible.
 */
pagespeed.lazyLoadInit = function(loadAfterOnload, blankImageSrc) {
  var temp = new pagespeed.LazyloadImages(blankImageSrc);
  pagespeed['lazyLoadImages'] = temp;
  // Add an event to the onload handler to check if any new images have now
  // become visible because of reflows or DOM manipulation. If loadAfterOnload
  // is true, load all images on the page.
  var lazy_onload = function() {
    // Note that the timeout here should be greater than the timeout for the
    // delay_images filter to avoid CPU contention between the two filters.
    window.setTimeout(function() {
      temp.force_load_ = loadAfterOnload;
      temp.loadVisible_();
    }, 200);
  }
  pagespeed.addHandler(window, 'load', lazy_onload);
  if (!loadAfterOnload) {
    var lazy_onscroll = function() {
      temp.loadVisible_();
    };
    pagespeed.addHandler(window, 'scroll', lazy_onscroll);
  }
};

pagespeed['lazyLoadInit'] = pagespeed.lazyLoadInit;
