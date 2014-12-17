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


goog.provide('pagespeed.Mob');

goog.require('goog.string');
goog.require('pagespeed.MobLayout');
goog.require('pagespeed.MobNav');
goog.require('pagespeed.MobUtil');



/**
 * Creates a context for PageSpeed mobilization, serving to orchestrate
 * the efforts of MobNav, MobLayout, and MobLogo.
 *
 * TODO(jmarantz): consider renaming above classes to pagespeed.Mob.Nav,
 * Mob.Layout, and pagespeed.Mob.Logo.  Simply doing that leads to compilation
 * errors with cyclic dependencies or to undefined variables.  One possible
 * resolution is to rename this class to pagespeed.Mob.Controller and have
 * pagespeed.Mob be strictly a namespace.
 *
 * @constructor
 */
pagespeed.Mob = function() {
  /**
   * Tracks the number of currently active XHR requests made.  We
   * delay mobilization until the active XHR request count goes to 0.
   * @private {number}
   */
  this.activeRequestCount_ = 0;

  /**
   * Maps image URLs to 'img' tags synthesized to force an image
   * load so we can detect image sizes for background images.
   * @private {Object.<string, Element>}
   */
  this.imageMap_ = {};

  /**
   * Time in milliseconds since epoch when mobilization started.
   * @private {number}
   */
  this.startTimeMs_ = Date.now();

  /**
   * Determines whether the mobilization is in debug mode.  This
   * is initialized in C++ based on whether the 'debug' filter was
   * on, but can also be enabled dynamically by a pressing a button
   * in the progress bar.
   * @private {boolean}
   */
  this.debugMode_ = false;

  /**
   * The number of DOM elements counted at the start of mobilization.
   * @private {number}
   */
  this.domElementCount_ = 0;

  /**
   * The total amount of work needed to mobilize a site, measured in
   * arbitrary units.  This helps update the progress bar.
   * @private {number}
   */
  this.totalWork_ = 0;

  /**
   * The previous percentage of work done (for updating progress bar)
   * @private {number}
   */
  this.prevPercentage_ = 0;

  /**
   * The total amount of work done so far.
   * @private {number}
   */
  this.workDone_ = 0;

  /**
   * Number of images that still need to be loaded.
   * @private {number}
   */
  this.pendingImageLoadCount_ = 0;

  /**
   * Whether we should mobilize after the images are done loading.
   * @private {boolean}
   */
  this.mobilizeAfterImageLoad_ = false;

  /**
   * The number of pending callbacks (other than XHR) that we should wait
   * for before mobilizing.
   * @private {number}
   */
  this.pendingCallbacks_ = 0;

  /**
   * The amount of work per layout pass
   * @private {number}
   */
  this.workPerLayoutPass_ = 0;

  /**
   * MobLayout context.  This is not needed until we are ready to run the
   * layout engine.
   * @private {pagespeed.MobLayout}
   */
  this.layout_ = new pagespeed.MobLayout(this);

  this.layout_.addDontTouchId(pagespeed.Mob.PROGRESS_SCRIM_ID_);
};


/**
 * HTML attribute to save the visibility state of the body.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_SAVE_VISIBLITY_ = 'ps-save-visibility';


/**
 * HTML ID of the scrim element.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_SCRIM_ID_ = 'ps-progress-scrim';


/**
 * HTML ID of the button to remove the progress bar.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_REMOVE_ID_ = 'ps-progress-remove';


/**
 * HTML ID of the progress log div.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_LOG_ID_ = 'ps-progress-log';


/**
 * HTML ID of the progress bar.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_SPAN_ID_ = 'ps-progress-span';


/**
 * HTML ID of the button to show the mobilization error log.
 * @private
 * @const
 */
pagespeed.Mob.PROGRESS_SHOW_LOG_ID_ = 'ps-progress-show-log';


/**
 * Here's a model for how much time it takes to mobilize the site, and it's a
 * real finger-to-the-wind estimation.  Each unit represents a millisecond of
 * delay.  Background images must be loaded from the server and we have no idea
 * how long this will take, but let's call it 100ms per image, and measure in
 * units of tenths of a millisecond.
 * @private {number}
 * @const
 */
pagespeed.Mob.COST_PER_IMAGE_ = 1000;


/**
 * Mobilizes the current web page.
 * @private
 */
pagespeed.Mob.prototype.mobilizeSite_ = function() {
  if (this.pendingImageLoadCount_ == 0) {
    console.log('mobilizing site');
    // TODO(jmarantz): Remove this hack once we are compiling mob_logo.js in
    // the same module.
    var extractTheme = window['extractTheme'];
    if (extractTheme && !pagespeed.MobUtil.inFriendlyIframe()) {
      ++this.pendingCallbacks_;
      extractTheme(this.imageMap_, this.logoComplete_.bind(this));
    } else {
      this.maybeRunLayout();
    }
  } else {
    this.mobilizeAfterImageLoad_ = true;
  }
};


/**
 * Called after logo computation is complete.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.Mob.prototype.logoComplete_ = function(themeData) {
  --this.pendingCallbacks_;
  this.updateProgressBar(this.domElementCount_, 'extract theme');
  if (window['psNavMode']) {
    var mobNav = new pagespeed.MobNav();
    mobNav.Run(themeData);
    this.updateProgressBar(this.domElementCount_, 'navigation');
  }
  this.maybeRunLayout();
};


/**
 * Called each time a single background image is loaded.
 * @private
 */
pagespeed.Mob.prototype.backgroundImageLoaded_ = function() {
  --this.pendingImageLoadCount_;
  this.updateProgressBar(pagespeed.Mob.COST_PER_IMAGE_, 'background image');
  if (this.pendingImageLoadCount_ == 0) {
    if (this.mobilizeAfterImageLoad_) {
      this.mobilizeSite_();
      // TODO(jmarantz): mobilizeAfterImageLoad_ seems redundant; try to
      // eliminate it in favor of using pendingImageLoadCount.
      this.mobilizeAfterImageLoad_ = false;
    }
  }
};


/**
 * Makes a map of every background image in the DOM to a 'img' elements.
 * after onload, the dimensions will be available, which is useful for
 * checking background image sizes.
 * @param {!Node} node
 * @private
 */
pagespeed.Mob.prototype.collectBackgroundImages_ = function(node) {
  var element = this.layout_.getMobilizeElement(node);
  if (element == null) {
    return;
  }
  var image = pagespeed.MobUtil.findBackgroundImage(element);
  if (image &&
      (goog.string.startsWith(image, 'http://') ||
      (goog.string.startsWith(image, 'https://'))) &&
      !this.imageMap_[image]) {
    var img = new Image();
    ++this.pendingImageLoadCount_;
    img.onload = this.backgroundImageLoaded_.bind(this);
    img.onerror = img.onload;
    img.src = image;
    this.imageMap_[image] = img;
  }

  for (var child = element.firstChild; child; child = child.nextSibling) {
    this.collectBackgroundImages_(child);
  }
};


/**
 * Called by pagespeed.XhrHijack whenever an XHR is initiated.
 *
 * This is set as a property on the prototype because it is called by name from
 * a separately the compiled XHR hijack module that is inlined in the head.
 *
 * @this {pagespeed.Mob}
 */
pagespeed.Mob.prototype['xhrSendHook'] = function() {
  ++this.activeRequestCount_;
};


/**
 * Called by pagespeed.XhrHijack whenever an XHR response is received.
 *
 * This is set as a property on the prototype because it is called by name from
 * a separately the compiled XHR hijack module that is inlined in the head.
 *
 * @param {number} http_status_code
 * @this {pagespeed.Mob}
 */
pagespeed.Mob.prototype['xhrResponseHook'] = function(http_status_code) {
  // if (http_status_code == 200)
  --this.activeRequestCount_;
  this.addExtraWorkForDom();
  this.maybeRunLayout();
};


/**
 * Initializes the mobilization process.  This should be called after
 * all the mobilization JavaScript has loaded.  This is the only public
 * entry point to mobilization.
 */
pagespeed.Mob.prototype.initiateMobilization = function() {
  this.setDebugMode(window.psDebugMode);  // psDebugMode set from C++
  this.domElementCount_ = pagespeed.MobUtil.countNodes(document.body);

  // Compute the amount of work needed every time we need to run layout.
  // We'll layout at least once, but we will also run layout when we
  // complete an XHR request.
  this.workPerLayoutPass_ = this.domElementCount_ *
      pagespeed.MobLayout.numberOfPasses();
  this.addExtraWorkForDom();

  // We multiply the number of DOM elements by the number of passes.
  // That includes all the layout passes, plus 2 for menus and navigation.
  var extraPasses = 0;
  var extractTheme = window['extractTheme'];
  if (extractTheme && pagespeed.MobUtil.inFriendlyIframe()) {
    this.totalWork_ += this.domElementCount_;
    if (window.psNavMode) {
      this.totalWork_ += this.domElementCount_;
    }
  }

  if (document.body != null) {
    this.collectBackgroundImages_(document.body);
  }
  this.totalWork_ += this.pendingImageLoadCount_ *
      pagespeed.Mob.COST_PER_IMAGE_;

  // Instructs our XHR hijack to call this.xhrSendHook and this.xhrResponseHook
  // whenever XHRs are sent and responses are received.
  window['pagespeedXhrHijackSetListener'](this);
  this.mobilizeSite_();
};


/**
 * @return {boolean}
 */
pagespeed.Mob.prototype.isReady = function() {
  return ((this.activeRequestCount_ == 0) &&
          (this.pendingCallbacks_ == 0) &&
          (this.pendingImageLoadCount_ == 0));
};


/**
 * Runs the layout engine if all known activity has quiesced.
 */
pagespeed.Mob.prototype.maybeRunLayout = function() {
  if (this.isReady()) {
    this.layout_.computeAllSizingAndResynthesize();
    if (this.debugMode_) {
      var progressRemoveAnchor = document.getElementById(
          pagespeed.Mob.PROGRESS_REMOVE_ID_);
      if (progressRemoveAnchor) {
        progressRemoveAnchor.textContent =
            'Remove Progress Bar and show mobilized site';
      }
    } else {
      this.removeProgressBar();
    }
  }
};


/**
 * Updates the progress bar after a layout pass.
 * @param {string} name
 */
pagespeed.Mob.prototype.layoutPassDone = function(name) {
  this.updateProgressBar(this.domElementCount_, name);
};


/**
 * Looks up the sizing for an image URL, which is collected before
 * mobilization can begin.
 *
 * @param {string} url
 * @return {Element}
 */
pagespeed.Mob.prototype.findImgTagForUrl = function(url) {
  return this.imageMap_[url];
};


/**
 * Increases our estimate of the total work required for mobilization.
 * This is used for updating the progress bar.
 */
pagespeed.Mob.prototype.addExtraWorkForDom = function() {
  this.totalWork_ += this.workPerLayoutPass_;
};


/**
 * Puts the mobilization in 'debug' mode, where the progress bar echoes
 * the debug console log below it (for debugging on phones) and does not
 * disappear when mobilization finishes, but waits for a user to dismiss it.
 * @param {boolean} debug
 */
pagespeed.Mob.prototype.setDebugMode = function(debug) {
  this.debugMode_ = debug;
  var log = document.getElementById(pagespeed.Mob.PROGRESS_LOG_ID_);
  if (log) {
    log.style.color = debug ? '#333' : 'white';
  }

  if (debug) {
    var show_log = document.getElementById(pagespeed.Mob.PROGRESS_SHOW_LOG_ID_);
    if (show_log) {
      show_log.style.display = 'none';
    }
  }
};


/**
 * Determines the visibility of an element, as if the progress bar was not
 * present.
 *
 * @param {Element} element
 * @return {string}
 */
pagespeed.Mob.prototype.getVisibility = function(element) {
  var visibility = element.getAttribute(pagespeed.Mob.PROGRESS_SAVE_VISIBLITY_);
  if (!visibility) {
    var computedStyle = window.getComputedStyle(element);
    if (computedStyle) {
      visibility = computedStyle.getPropertyValue('visibility');
    }
    if (!visibility) {
      visibility = 'visible';
    }
  }
  return visibility;
};


/**
 * Record that a certain amount of work got done toward mobilization.  Updates
 * a progres-bar showing how far along we are in mobilization, and logs the
 * specific operation (currentOp) that was completed.  This goes both to the
 * console log, and if the 'debug' filter is on, it also is logged to the
 * progress-bar scrim, facilitating debug on phones.
 *
 * @param {number} unitsDone
 * @param {string} currentOp
 */
pagespeed.Mob.prototype.updateProgressBar = function(unitsDone, currentOp) {
  this.workDone_ += unitsDone;
  var percent = 100;
  if (this.totalWork_ > 0) {
    percent = Math.round((this.workDone_ * 100) / this.totalWork_);
    if (percent > 100) {
      percent = 100;
    }
  }
  if (percent != this.prevPercentage_) {
    var span = document.getElementById(pagespeed.Mob.PROGRESS_SPAN_ID_);
    if (span) {
      span.style.width = percent + '%';
    }
    this.prevPercentage_ = percent;
  }
  var elapsedMs = Date.now() - this.startTimeMs_;
  var msg = '' + percent + '% ' + elapsedMs + 'ms: ' + currentOp;
  console.log(msg);
  var log = document.getElementById(pagespeed.Mob.PROGRESS_LOG_ID_);
  if (log) {
    log.textContent += msg + '\n';
  }
};


/**
 * Removes the progress bar from the screen, if it was present.
 */
pagespeed.Mob.prototype.removeProgressBar = function() {
  var progressBar = document.getElementById(pagespeed.Mob.PROGRESS_SCRIM_ID_);
  if (progressBar) {
    progressBar.style.display = 'none';
    progressBar.parentNode.removeChild(progressBar);
  }
};


// We need a global 'psMob' object for now, for use in compatibility functions.
// This should eventually disappear.
var psMob = new pagespeed.Mob();


// Hide the body, then set a timer so we can start analyzing its geometry,
// assuming it will be laid out.
psMob.initiateMobilization();


// TODO(jmarantz): eliminate compatibility entry-points for mob_logo.js
// and C++-generated JS.


/**
 * Called by mob_logo.js.
 * @param {Element} element
 * @export
 */
function psGetVisiblity(element) {
  psMob.getVisibility(element);
}


/**
 * Called by C++-created JavaScript.
 * @export
 */
function psSetDebugMode() {
  psMob.setDebugMode(true);
}


/**
 * Called by C++-created JavaScript.
 * @export
 */
function psRemoveProgressBar() {
  psMob.removeProgressBar();
}
