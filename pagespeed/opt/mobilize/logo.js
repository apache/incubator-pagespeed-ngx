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


// Steps for detecting logo and computing theme color.
// 1: Find out all elements which may be logo. An element is a logo candidate
//    if it has the 'logo' string or the organization name in its attributes,
//    or has HREF pointing to the landing URL.
// 2: Find out all images which may be the foreground images of the logo. For
//    each of the logo elements, we consider all images (IMG, SVG, and
//    background image) in its descedants, and the background image in its
//    nearest ancestor.
//    Wait until all of these images loaded before doing the next steps.
// 3. Remove the candidates which do not have images of proper size and
//    position.
// 4. Find out the best candidate element, considering image size, position, and
//    the number of attributes with 'logo' or organization string.
// 5. Find out the background color of the best candidate element. This is the
//    non-transparent color of its nearest ancestor.
// 6. Compute theme color based on the background color and foreground image.

goog.provide('mob.Logo');
goog.provide('mob.LogoCandidate');

goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.string');
goog.require('mob.Color');
goog.require('mob.util');
goog.require('mob.util.ImageSource');



/**
 * @constructor
 * @struct
 * @param {!mob.Logo.LogoRecord} logoRecord
 * @param {!goog.color.Rgb} background
 * @param {!goog.color.Rgb} foreground
 */
mob.LogoCandidate = function(logoRecord, background, foreground) {
  /** {!mob.Logo.LogoRecord} */
  this.logoRecord = logoRecord;

  /** {!goog.color.Rgb} */
  this.background = background;

  /** {!goog.color.Rgb} */
  this.foreground = foreground;
};



/**
 * Creates a context for Pagespeed logo detector.
 * @constructor
 */
mob.Logo = function() {
  /**
   * Callback to invoke when this object finishes its work.
   * @private {?function(!Array.<!mob.LogoCandidate>)} doneCallback_
   */
  this.doneCallback_ = null;

  /** @private {?string} */
  this.organization_ = mob.util.getSiteOrganization();

  /** @private {string} */
  this.landingUrl_ = mob.util.getWindow().location.origin +
                     mob.util.getWindow().location.pathname;

  /**
   * Array of logo candidates.
   * @private {!Array.<!mob.Logo.LogoRecord>}
   */
  this.candidates_ = [];

  /** @private {number} */
  this.pendingEventCount_ = 0;

  /** @private {number} */
  this.maxNumCandidates_ = 1;
};



/**
 * Creates an empty logo record.
 * @param {number} metric
 * @param {!Element} element
 * @constructor @struct
 */
mob.Logo.LogoRecord = function(metric, element) {
  /**
   * Metric of being a logo element. Metric is computed for the elements with
   * size and position within certain ranges, and with an image in its sub-tree
   * or ancestor. Metric value is determined by the number of attributes of
   * this element or its ancestor which have the substring of 'logo' or
   * organization name.
   *
   * @type {number}
   */
  this.metric = metric;
  /** @type {!Element} */
  this.logoElement = element;
  /** @type {!Array.<!Element>} */
  this.childrenElements = [];
  /** @type {!Array.<!Element>} */
  this.childrenImages = [];
  /** @type {?Element} */
  this.ancestorElement = null;
  /** @type {?Element} */
  this.ancestorImage = null;
  /** @type {?Element} */
  this.foregroundElement = null;
  /** @type {?Element} */
  this.foregroundImage = element;
  /** @type {?mob.util.Rect} */
  this.rect = null;
  /** @type {?Array.<number>} */
  this.backgroundColor = null;
};


/**
 * Minimum width of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
mob.Logo.prototype.MIN_WIDTH_ = 20;


/**
 * Minimum height of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
mob.Logo.prototype.MIN_HEIGHT_ = 10;


/**
 * Maximum height of an element in the origin site to be considered as the logo.
 * @private @const {number}
 */
mob.Logo.prototype.MAX_HEIGHT_ = 400;


/**
 * Minimum area of an element which must be covered by an image for that image
 * to be considered a logo.
 * @private @const {number}
 */
mob.Logo.prototype.RATIO_AREA_ = 0.5;


/**
 * Find the element that is likely to be a logo.
 * @param {!Element} element Element being tested whether has logo attributes
 * @return {?mob.Logo.LogoRecord}
 * @private
 */
mob.Logo.prototype.findLogoElement_ = function(element) {
  var style = mob.util.getWindow().getComputedStyle(element);
  if (style.getPropertyValue('visibility') == 'hidden') {
    return null;
  }

  // Find URL of the image. Some sites use 'logo' or the organization name
  // to name the logo image, so the file name is a signal for identifying the
  // logo. However, such signal is not available for inlined images.
  var imageSrc = null;
  if (element.nodeName.toUpperCase() == goog.dom.TagName.IMG) {
    imageSrc = element.src;
  } else {
    // IMG tag can also have background image, but it's ignored for now until
    // we see actual use of it.
    imageSrc = mob.util.findBackgroundImage(element);
  }
  // Note that resourceFileName returns '' for a 'data:image/' src, so we don't
  // need another check for that here.
  imageSrc = mob.util.resourceFileName(imageSrc);

  var metric = 0;
  var organization = this.organization_;
  function accumulateMetric(signal) {
    if (signal && (typeof(signal) == 'string')) {
      if (goog.string.caseInsensitiveContains(signal, 'logo')) {
        ++metric;
      }
      if (organization && mob.util.findPattern(signal, organization)) {
        ++metric;
      }
    }
  }

  accumulateMetric(element.title);
  accumulateMetric(element.id);
  accumulateMetric(element.className);
  accumulateMetric(element.alt);
  accumulateMetric(imageSrc);

  // If the element has 'href' and it points to the landing page, the element
  // may be a logo candidate. Typical construct looks like
  //   <a href='...'><img src='...'></a>
  if (element.href == this.landingUrl_) {
    ++metric;
  }

  if (metric > 0) {
    return (new mob.Logo.LogoRecord(metric, element));
  }

  return null;
};


/**
 * Find all of the logo candidates.
 * @param {!Element} element
 * @private
 */
mob.Logo.prototype.findLogoCandidates_ = function(element) {
  var newCandidate = this.findLogoElement_(element);
  if (newCandidate) {
    this.candidates_.push(newCandidate);
  }

  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    this.findLogoCandidates_(childElement);
  }
};


/**
 * Add the image to the pending list. We will wait until all pending images
 * have been loaded before finding the best logo.
 * @param {!Element} img
 * @private
 */
mob.Logo.prototype.addImageToPendingList_ = function(img) {
  ++this.pendingEventCount_;
  img.addEventListener(goog.events.EventType.LOAD,
                       goog.bind(this.eventDone_, this));
  img.addEventListener(goog.events.EventType.ERROR,
                       goog.bind(this.eventDone_, this));
};


/**
 * Create a new IMG tag using the specified image source. This IMG tag will be
 * monitored.
 * @param {string} imageSrc
 * @return {!Element}
 * @private
 */
mob.Logo.prototype.newImage_ = function(imageSrc) {
  var img = mob.util.getWindow().document.createElement(goog.dom.TagName.IMG);
  this.addImageToPendingList_(img);
  img.src = imageSrc;
  return img;
};


/**
 * Collect all images in the element's descendants.
 * @param {!Element} element
 * @param {!Array.<!Element>} childrenElements
 * @param {!Array.<!Element>} childrenImages
 * @private
 */
mob.Logo.prototype.collectChildrenImages_ = function(element, childrenElements,
                                                     childrenImages) {
  var imageSrc = null;
  for (var src in mob.util.ImageSource) {
    imageSrc = mob.util.extractImage(element, mob.util.ImageSource[src]);
    if (imageSrc) {
      var img = null;
      if (src == mob.util.ImageSource.IMG) {
        img = element;
        if (!element.naturalWidth) {
          this.addImageToPendingList_(img);
        }
      } else {
        img = this.newImage_(imageSrc);
      }
      childrenElements.push(element);
      childrenImages.push(img);
      // Ignore background image of IMG and SVG tags.
      break;
    }
  }

  for (var childElement = element.firstElementChild; childElement;
       childElement = childElement.nextElementSibling) {
    this.collectChildrenImages_(childElement, childrenElements, childrenImages);
  }
};


/**
 * Find all images which may be the foreground of the logo.
 * @param {!Array.<!mob.Logo.LogoRecord>} logoCandidates
 * @private
 */
mob.Logo.prototype.findImagesAndWait_ = function(logoCandidates) {
  for (var i = 0; i < logoCandidates.length; ++i) {
    var logo = logoCandidates[i];
    var element = logo.logoElement;
    this.collectChildrenImages_(element, logo.childrenElements,
                                logo.childrenImages);

    // Find the background in the logo element's nearest ancestor.
    element = element.parentElement;
    while (element) {
      var imageSrc = mob.util.findBackgroundImage(element);
      if (imageSrc) {
        logo.ancestorElement = element;
        logo.ancestorImage = this.newImage_(imageSrc);
        break;
      }
      element = element.parentElement;
    }
  }

  if (this.pendingEventCount_ == 0) {
    this.findBestLogoAndColor_();
  }
};


/**
 * @param {!Array.<!Object>} array
 * @param {number} index
 * @private
 */
mob.Logo.fastRemoveArrayElement_ = function(array, index) {
  var last = array.length - 1;
  if (index < last) {
    array[index] = array[last];
  }
  array.pop();
};


/**
 * Remove the logo candidates which do not have image of proper size and
 * position.
 * @private
 */
mob.Logo.prototype.pruneCandidateBySizePos_ = function() {
  var logoCandidates = this.candidates_;
  for (var i = 0; i < logoCandidates.length; ++i) {
    var logo = logoCandidates[i];
    var element = logo.logoElement;
    var rect = mob.util.boundingRectAndSize(element);
    var area = rect.width * rect.height;

    var minArea = area * this.RATIO_AREA_;
    var bestIndex = -1;
    var bestImageArea = 0;
    for (var j = 0; j < logo.childrenElements.length; ++j) {
      var img = logo.childrenElements[j];
      if (!img) {
        continue;
      }
      rect = mob.util.boundingRectAndSize(img);
      area = rect.width * rect.height;
      if (area >= minArea && rect.width > this.MIN_WIDTH_ &&
          rect.height > this.MIN_HEIGHT_ && rect.height < this.MAX_HEIGHT_) {
        if (area > bestImageArea) {
          bestImageArea = area;
          bestIndex = j;
        }
      }
    }

    if (bestIndex >= 0) {
      logo.foregroundElement = logo.childrenElements[bestIndex];
      logo.foregroundImage = logo.childrenImages[bestIndex];
      logo.rect = rect;
    } else if (logo.ancestorElement) {
      img = logo.ancestorElement;
      rect = mob.util.boundingRectAndSize(img);
      area = rect.width * rect.height;
      if (area >= minArea && rect.width > this.MIN_WIDTH_ &&
          rect.height > this.MIN_HEIGHT_ && rect.height < this.MAX_HEIGHT_) {
        logo.foregroundElement = logo.ancestorElement;
        logo.foregroundImage = logo.ancestorImage;
        logo.rect = rect;
      } else {
        mob.Logo.fastRemoveArrayElement_(logoCandidates, i);
        --i;
      }
    } else {
      mob.Logo.fastRemoveArrayElement_(logoCandidates, i);
      --i;
    }
  }
};


/**
 * Find the best logo and compute theme color.
 * @private
 */
mob.Logo.prototype.findBestLogoAndColor_ = function() {
  this.pruneCandidateBySizePos_();
  var logos = this.findBestLogos_();
  var candidates = [];
  var candidateMap = {};  // Dedup duplicates from findBestLogos_.
  for (var i = 0;
      (candidates.length < this.maxNumCandidates_) && (i < logos.length);
       ++i) {
    var logo = logos[i];
    if (!candidateMap[logo.foregroundImage.src]) {
      candidateMap[logo.foregroundImage.src] = true;
      this.findLogoBackground_(logo);
      var mobColor = new mob.Color();
      var themeColor = mobColor.run(logo.foregroundImage, logo.backgroundColor);
      candidates.push(new mob.LogoCandidate(logo, themeColor.background,
                                            themeColor.foreground));
    }
  }
  var callback = this.doneCallback_;
  this.doneCallback_ = null;
  callback(candidates);
};


/**
 * @private
 */
mob.Logo.prototype.eventDone_ = function() {
  --this.pendingEventCount_;
  if (this.pendingEventCount_ == 0) {
    this.findBestLogoAndColor_();
  }
};


/**
 * @private {!Array.<!Function.<!mob.util.Rect>>}
 */
mob.Logo.rectAccessors_ = [
  /**
   * @param {!mob.util.Rect} rect
   * @return {number}
   */
  function(rect) { return rect.top; },

  /**
   * @param {!mob.util.Rect} rect
   * @return {number}
   */
  function(rect) { return rect.left; },

  /**
   * @param {!mob.util.Rect} rect
   * @return {number}
   */
  function(rect) { return rect.width * rect.height; }
];


/**
 * Compare 2 LogoRecords, return -1 if a is better, and 1 if b is better.
 * @param {!mob.Logo.LogoRecord} a
 * @param {!mob.Logo.LogoRecord} b
 * @return {number}
 * @private
 */
mob.Logo.compareLogos_ = function(a, b) {
  if (a.metric > b.metric) {  // Higher is better.
    return -1;
  } else if (b.metric > a.metric) {
    return 1;
  }

  for (var i = 0; i < mob.Logo.rectAccessors_.length; ++i) {
    var accessor = mob.Logo.rectAccessors_[i];
    var aval = accessor(a.rect);
    var bval = accessor(b.rect);
    if (aval < bval) {
      return -1;
    } else if (bval < aval) {
      return 1;
    }
  }
  // Resolve a tie by comparing the logo URLs, so the order is stable.
  if (a.logoElement && a.logoElement.src &&
      b.logoElement && b.logoElement.src) {
    if (a.logoElement.src < b.logoElement.src) {
      return -1;
    } else if (a.logoElement.src > b.logoElement.src) {
      return 1;
    }
  }
  return 0;
};


/**
 * Use the position and size to update the metric of all elements in
 * this.candidates_
 * @private
 */
mob.Logo.prototype.updateCandidateMetricsWithRect_ = function() {
  var maxBot = 0;
  var minTop = Infinity;
  var i, rect, candidate;
  for (i = 0; candidate = this.candidates_[i]; ++i) {
    rect = candidate.rect;
    minTop = Math.min(minTop, rect.top);
    maxBot = Math.max(maxBot, rect.bottom);
  }
  for (i = 0; candidate = this.candidates_[i]; ++i) {
    rect = candidate.rect;
    // TODO(huibao): Investigate a better way for incorporating size and
    // position in the selection of the best logo, for example
    // Math.sqrt((maxBot - rect.bottom) / (maxBot - minTop)).
    var multTop = Math.sqrt((maxBot - rect.top) / (maxBot - minTop));
    candidate.metric *= multTop;
  }
};


/**
 * Find and rank the best logo candidates. The best candidate is the one with
 * the largest metric value. If there are more than one candiates with the same
 * largest metric, the follow rules are applied on them in order for choosing
 * the best one:
 *   - the candidate with the highest top border
 *   - the candidate with the smallest left border
 *   - the candidate with the largest size
 *
 * If there are still multiple candidates after these rules, then the first
 * one which was found will be chosen.
 *
 * If there are no logo candidates then null is returned.
 *
 * @return {!Array.<!mob.Logo.LogoRecord>}
 * @private
 */
mob.Logo.prototype.findBestLogos_ = function() {
  if (this.candidates_.length <= 1) {
    return this.candidates_;
  }

  this.updateCandidateMetricsWithRect_();
  var logoCandidates = this.candidates_;

  if ((logoCandidates.length > 0) && (this.maxNumCandidates_ == 1)) {
    // Just pick the best one, which is faster than sorting.
    var bestLogo = logoCandidates[0];
    for (var i = 1; i < logoCandidates.length; ++i) {
      var candidate = logoCandidates[i];
      if (mob.Logo.compareLogos_(candidate, bestLogo) < 0) {
        bestLogo = candidate;
      }
    }
    logoCandidates[0] = bestLogo;
  } else {
    logoCandidates.sort(mob.Logo.compareLogos_);
  }
  return logoCandidates;
};


/**
 * Extract background color.
 * @param {!Element} element
 * @return {?Array.<number>}
 * @private
 */
mob.Logo.prototype.extractBackgroundColor_ = function(element) {
  var computedStyle =
      mob.util.getWindow().document.defaultView.getComputedStyle(element, null);
  if (computedStyle) {
    var colorString = computedStyle.getPropertyValue('background-color');
    if (colorString) {
      var colorValues = mob.util.colorStringToNumbers(colorString);
      if (colorValues && (colorValues.length == 3 ||
                          (colorValues.length == 4 && colorValues[3] != 0))) {
        // colorValue should be in RGB format (3 element-array) or RGBA format
        // (4 element-array). If it is in RGBA format and the last element is 0,
        // this color is fully transparent and should be ignored.
        return colorValues;
      }
    }
  }
  return null;
};


/**
 * Find the background color for the logo.
 * @param {?mob.Logo.LogoRecord} logo
 * @private
 */
mob.Logo.prototype.findLogoBackground_ = function(logo) {
  if (!logo || !logo.foregroundElement) {
    return;
  }

  var backgroundColor = null;
  var element = logo.foregroundElement;
  while (element && !backgroundColor) {
    backgroundColor = this.extractBackgroundColor_(element);
    element = element.parentElement;
  }

  logo.backgroundColor = backgroundColor;
};


/**
 * Extract theme of the page. This is the entry method.  If the
 * body is empty, or if there is a currently outstanding call to run(),
 * then doneCallback will be called immediately with an empty array.
 *
 * @param {?function(!Array.<!mob.LogoCandidate>)} doneCallback
 * @param {number} maxNumCandidates
 */
mob.Logo.prototype.run = function(doneCallback, maxNumCandidates) {
  // If running in WKH, the event listeners attached to the images created by
  // logo detection don't fire, so instead we check for loadComplete to mark
  // when the image elements are finished loading.
  if (typeof extension != 'undefined') {
    extension.addEventListener('loadComplete', goog.bind(this.eventDone_, this),
                               false);
  }
  var body = mob.util.getWindow().document.body;
  if (this.doneCallback_ || !body) {
    doneCallback([]);
  } else {
    this.doneCallback_ = doneCallback;
    this.maxNumCandidates_ = maxNumCandidates;
    this.findLogoCandidates_(body);
    this.findImagesAndWait_(this.candidates_);
  }
};
