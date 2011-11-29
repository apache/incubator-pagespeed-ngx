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

var pagespeed = pagespeed || {};

/**
 * @constructor
 */
pagespeed.LazyloadImages = function() {
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
};

/**
 * Returns a bounding box for the currently visible part of the window.
 * @return {{
 *     top: (number),
 *     bottom: (number)
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
  return {
    top: scrollY,
    bottom: scrollY + window.innerHeight
  };
};

/**
 * Returns a bounding box for the given element.
 * @param {Element} element DOM element to measure offset of.
 * @return {{
 *     top: (number),
 *     bottom: (number)
 * }}
 * @private
 */
pagespeed.LazyloadImages.prototype.offset_ = function(element) {
  var original = element;
  var curtop = 0;
  if (element.offsetParent) {
    curtop = element.offsetTop;
    while ((element = element.offsetParent) != null) {
      curtop += element.offsetTop;
    }
  }
  return {
    top: curtop,
    bottom: curtop + original.offsetHeight
  };
};

/**
 * Returns true if an element is currently visible or within the buffer.
 * @param {Element} element The DOM element to check for visibility.
 * @return {boolean} True if the element is visible.
 * @private
 */
pagespeed.LazyloadImages.prototype.isVisible_ = function(element) {
  var viewport = this.viewport_();
  var position = this.offset_(element);
  return (position.top - this.buffer_ <= viewport.bottom &&
          position.bottom + this.buffer_ >= viewport.top);
};

/**
 * Loads the given element if it is visible. Otherwise, adds it to the deferred
 * queue.
 * @param {Element} element The element to check for visibility.
 */
pagespeed.LazyloadImages.prototype.loadIfVisible = function(element) {
  var data_src = element.getAttribute('pagespeed_lazy_src');
  if (data_src != null) {
    if (this.isVisible_(element)) {
      element.src = data_src;
      element.removeAttribute('pagespeed_lazy_src');
    } else {
      this.deferred_.push(element);
    }
  }
};

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
 */
pagespeed.lazyLoadInit = function() {
  pagespeed.lazyLoadImages = new pagespeed.LazyloadImages();
  var lazy_onscroll = function() {
    pagespeed.lazyLoadImages.loadVisible_();
  };
  pagespeed.addHandler(window, 'scroll', lazy_onscroll);
};

