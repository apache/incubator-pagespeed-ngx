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

goog.require('goog.color');
goog.require('goog.dom.TagName');
goog.require('goog.events.EventType');
goog.require('goog.object');
goog.require('goog.string');
goog.require('goog.uri.utils');
goog.require('pagespeed.MobLayout');
goog.require('pagespeed.MobLogo');
goog.require('pagespeed.MobNav');
goog.require('pagespeed.MobTheme');
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
   * Maps image URLs to Dimensions structures so we can detect image sizes
   * for background images.  The Map is populated initially via from JSON
   * created by ImageRewriteFilter::RenderDone in
   * net/instaweb/rewriter/image_rewrite_filter.cc.
   *
   * Any images that are not populated from C++ will be populated in the
   * same format from an new image onload callback.  To avoid initiating
   * redundant fetches for the same image, we will initiatially populate
   * the map with the sential Dimensions pagespeed.Mob.IN_TRANSIT_.
   *
   * @private {Object.<string, pagespeed.MobUtil.Dimensions>}
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

  /**
   * Number of URLs to process. This property is used only when the query
   * parameters include 'PageSpeedSiteWideProcessing', which is defined in
   * QUERY_SITE_WIDE_PROCESS_. It should be '-1' if site wide processing
   * will not be used or has been completed.
   * @private {number}
   */
  this.configNumUrlsToProcess_ = -1;

  /**
   * ID of the timer which monitors the processing for the current page.
   * @private {?number}
   */
  this.configTimer_ = null;

  /**
   * List of URLs which shall be processed.
   * @private {!Array.<string>}
   */
  this.configUrls_ = [];

  /**
   * Theme data which have been found.
   * @private {!Array.<pagespeed.MobUtil.ThemeData>}
   */
  this.configThemes_ = [];

  /**
   * Navigation context.  We keep his around around to enable interactive logo
   * picking to back-annotate the alternate logo selection to the DOM.
   * @private {pagespeed.MobNav}
   */
  this.mobNav_ = null;

  /**
   * Logo context.
   * @private {!pagespeed.MobLogo}
   */
  this.mobLogo_ = new pagespeed.MobLogo(this);

  /**
   * Array of candidate logos, lazy-initilized when a user clicks the
   * logo to configure it with ?PageSpeedMobConfig=on.
   * @private {Array.<pagespeed.MobLogoCandidate>}
   */
  this.logoCandidates_ = null;
};


/**
 * HTML attribute to save the visibility state of the body.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_SAVE_VISIBLITY_ = 'ps-save-visibility';


/**
 * HTML ID of the scrim element.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_SCRIM_ID_ = 'ps-progress-scrim';


/**
 * HTML ID of the button to remove the progress bar.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_REMOVE_ID_ = 'ps-progress-remove';


/**
 * HTML ID of the progress log div.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_LOG_ID_ = 'ps-progress-log';


/**
 * HTML ID of the progress bar.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_SPAN_ID_ = 'ps-progress-span';


/**
 * HTML ID of the button to show the mobilization error log.
 * @private @const {string}
 */
pagespeed.Mob.PROGRESS_SHOW_LOG_ID_ = 'ps-progress-show-log';


/**
 * HTML ID of the hidden iframe which is used for site-wide processing.
 * @private @const {string}
 */
pagespeed.Mob.CONFIG_IFRAME_ID_ = 'ps-hidden-iframe';


/**
 * Query parameter for site-wide theme extraction.
 * @private @const {string}
 */
pagespeed.Mob.CONFIG_QUERY_SITE_WIDE_PROCESS_ = 'PageSpeedSiteWideProcessing';


/**
 * Maximum time for mobilizing a page.
 * @private @const {number}
 */
pagespeed.Mob.CONFIG_MAX_TIME_MS_ = 10000;


/**
 * Maximum number of links for site-wide theme extraction.
 * @private @const {number}
 */
pagespeed.Mob.CONFIG_MAX_NUM_LINKS_ = 100;


/**
 * String used as a temporary imageMap_ value after an image has
 * started to load, but before it's done loading.
 * @private @const {!pagespeed.MobUtil.Dimensions}
 */
pagespeed.Mob.IN_TRANSIT_ = new pagespeed.MobUtil.Dimensions(-1, -1);


/**
 * Here's a model for how much time it takes to mobilize the site, and it's a
 * real finger-to-the-wind estimation.  Each unit represents a millisecond of
 * delay.  Background images must be loaded from the server and we have no idea
 * how long this will take, but let's call it 100ms per image, and measure in
 * units of tenths of a millisecond.
 * @private @const {number}
 */
pagespeed.Mob.COST_PER_IMAGE_ = 1000;


/**
 * Mobilizes the current web page.
 * @private
 */
pagespeed.Mob.prototype.mobilizeSite_ = function() {
  if (this.pendingImageLoadCount_ == 0) {
    pagespeed.MobUtil.consoleLog('mobilizing site');
    // TODO(jmarantz): Remove this hack once we are compiling mob_logo.js in
    // the same module.
    if (!window.psNavMode || pagespeed.MobUtil.inFriendlyIframe()) {
      this.maybeRunLayout();
    }
  } else {
    this.mobilizeAfterImageLoad_ = true;
  }
};


/**
 * Called after theme computation is complete.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
// TODO(huibao): Make themeComplete_ and site-wide theme extraction to return
// multiple themes for one URL.
pagespeed.Mob.prototype.themeComplete_ = function(themeData) {
  --this.pendingCallbacks_;
  this.updateProgressBar(this.domElementCount_, 'extract theme');
  this.mobNav_ = new pagespeed.MobNav();
  this.mobNav_.Run(themeData);
  this.updateProgressBar(this.domElementCount_, 'navigation');
  this.maybeRunLayout();

  var masterPsMob = this.psMobForMasterWindow_();
  if (masterPsMob && masterPsMob.configNumUrlsToProcess_ >= 0) {
    if (this.inPsIframeWindow_()) {
      this.mobNav_.updateHeaderBar(this.masterWindow_(), themeData);
    } else {
      var iframe = document.createElement(goog.dom.TagName.IFRAME);
      iframe.id = pagespeed.Mob.CONFIG_IFRAME_ID_;
      iframe.hidden = true;
      document.body.appendChild(iframe);
    }
    masterPsMob.configThemes_.push(themeData);
    this.mobilizeNextUrl_(true /* finish on time */);
  }
};


/**
 * Called each time a single background image is loaded.
 * @param {!Element} img
 * @private
 */
pagespeed.Mob.prototype.backgroundImageLoaded_ = function(img) {
  this.imageMap_[img.src] = new pagespeed.MobUtil.Dimensions(
      img.width, img.height);
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
    this.imageMap_[image] = pagespeed.Mob.IN_TRANSIT_;
    var img = new Image();
    ++this.pendingImageLoadCount_;
    img.onload = goog.bind(this.backgroundImageLoaded_, this, img);
    img.onerror = img.onload;
    img.src = image;
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
  pagespeed.MobUtil.sendBeacon(pagespeed.MobUtil.BeaconEvents.LOAD_EVENT);
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
  if (window.psNavMode && pagespeed.MobUtil.inFriendlyIframe()) {
    this.totalWork_ += this.domElementCount_;  // logo
    this.totalWork_ += this.domElementCount_;  // nav
  }

  if (document.body != null) {
    // Iterate over the JSON responses and convert them into our
    // closure-compiler-safe Dimensions structure to avoid confusing
    // renaming of the JSON variables.
    for (var url in window.psMobStaticImageInfo) {
      var dims = window.psMobStaticImageInfo[url];
      this.imageMap_[url] = new pagespeed.MobUtil.Dimensions(
          dims['w'], dims['h']);
    }
    // Collect image information if we need it for relayout or theme detection.
    if (!pagespeed.MobTheme.precomputedThemeAvailable() ||
        window.psLayoutMode) {
      this.collectBackgroundImages_(document.body);
    }
  }
  this.totalWork_ += this.pendingImageLoadCount_ *
      pagespeed.Mob.COST_PER_IMAGE_;

  // Instructs our XHR hijack to call this.xhrSendHook and this.xhrResponseHook
  // whenever XHRs are sent and responses are received.
  if (window.psLayoutMode) {
    window['pagespeedXhrHijackSetListener'](this);
  }
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
    if (window.psLayoutMode) {
      this.layout_.computeAllSizingAndResynthesize();
    }
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
 * mobilization can begin.  The object returns has w:WIDTH, h:HEIGHT.
 *
 * Returns null if the image was not mapped.
 *
 * @param {string} url
 * @return {?pagespeed.MobUtil.Dimensions}
 */
pagespeed.Mob.prototype.findImageSize = function(url) {
  var values = this.imageMap_[url];
  if (values == pagespeed.Mob.IN_TRANSIT_) {
    values = null;
  }
  return values;
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
 * @param {!Element} element
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
  pagespeed.MobUtil.consoleLog(msg);
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


/**
 * Extract theme. After that, execute themeComplete_().
 * @private
 */
pagespeed.Mob.prototype.extractTheme_ = function() {
  if (window.psNavMode) {
    ++this.pendingCallbacks_;
    var paramName = pagespeed.Mob.CONFIG_QUERY_SITE_WIDE_PROCESS_;
    if (!this.inPsIframeWindow_() && window.document.body &&
        window.location.search.indexOf(paramName) != -1) {
      var urls = this.collectUrls_();
      this.psMobForMasterWindow_().configUrls_ = urls;
      var numString = prompt(urls.length + ' links have been found in this ' +
          'page. If you want to compute theme from them, enter the number of ' +
          'links below and press "OK". It may take a while to process them. ' +
          'Once processing completes, you will see another dialog.',
          urls.length.toString());
      if (numString) {  // numString is null if "Cancel" was chosen.
        var num = goog.string.parseInt(numString);
        if (num < 0) {
          this.configNumUrlsToProcess_ = 0;
        } else if (num > urls.length) {
          this.configNumUrlsToProcess_ = urls.length;
        } else {
          this.configNumUrlsToProcess_ = num;
        }
      }
    }
    pagespeed.MobTheme.extractTheme(
        this.mobLogo_, goog.bind(this.themeComplete_, this));
  }
};


/**
 * We need a global 'psMob' object for now, for use in compatibility
 * functions. This should eventually disappear.
 * @type {!pagespeed.Mob}
 */
window.psMob = new pagespeed.Mob();


/**
 * Scan the document for the top 5 logo candidates, computing their
 * background and foreground colors.  When that's done, pop up a dialog
 * letting the user choose a new logo & colors.
 */
pagespeed.Mob.prototype.showLogoCandidates = function() {
  if ((this.logoCandidates_ != null) && (this.logoCandidates_.length > 0)) {
    this.mobNav_.chooserShowCandidates(this.logoCandidates_);
  }
};


/**
 * Records the logo candidates for this page, popping up the logo
 * chooser, and setting the top-ranked logo in the theme.
 * @param {!pagespeed.MobTheme} theme
 * @param {!Array.<pagespeed.MobLogoCandidate>} candidates
 */
pagespeed.Mob.prototype.setLogoCandidatesAndShow = function(theme, candidates) {
  this.logoCandidates_ = candidates;
  theme.logoComplete(candidates);
  this.mobNav_.chooserShowCandidates(candidates);
};


// Start theme extraction and navigation re-synthesis when the document content
// is loaded.
window.addEventListener(goog.events.EventType.DOMCONTENTLOADED,
                        goog.bind(window.psMob.extractTheme_, window.psMob));


// Start layout re-synthesis if it has been configured.
window.addEventListener(goog.events.EventType.LOAD,
    goog.bind(window.psMob.initiateMobilization, window.psMob));


/**
 * Called by C++-created JavaScript.
 * @export
 */
function psSetDebugMode() {
  window.psMob.setDebugMode(true);
}


/**
 * Called by C++-created JavaScript.
 * @export
 */
function psRemoveProgressBar() {
  window.psMob.removeProgressBar();
}


/**
 * Called by anchor-tag.
 * @export
 */
function psPickLogo() {
  window.psMob.showLogoCandidates();
}


////////////////////////////////////////////////////////////////////////////////
// Code below is for configuration
////////////////////////////////////////////////////////////////////////////////


/**
 * Returns true if the current window is the hidden iframe which we created.
 * @private
 * @return {boolean}
 */
pagespeed.Mob.prototype.inPsIframeWindow_ = function() {
  return (pagespeed.MobUtil.inFriendlyIframe() &&
          goog.isDefAndNotNull(window.frameElement) &&
          window.frameElement.id == pagespeed.Mob.CONFIG_IFRAME_ID_);
};


/**
 * Returns the window which initiated site-wide processing.
 * @private
 * @return {!Object}
 */
pagespeed.Mob.prototype.masterWindow_ = function() {
  if (this.inPsIframeWindow_()) {
    return window.parent;
  } else {
    return window;
  }
};


/**
 * Returns the psMob object which stores the information used for site-wide
 * processing.
 * @private
 * @return {!pagespeed.Mob}
 */
pagespeed.Mob.prototype.psMobForMasterWindow_ = function() {
  return this.masterWindow_().psMob;
};


/**
 * Display the site-wide theme extraction results.
 * @private
 */
pagespeed.Mob.prototype.showSiteWideThemes_ = function() {
  var masterPsMob = this.psMobForMasterWindow_();
  if (!masterPsMob || !masterPsMob.configThemes_ ||
      masterPsMob.configThemes_.length == 0) {
    return;
  }

  // Find out the number of occurrence of the logo image and theme color.
  var map = {};
  var themeData;
  for (var i = 0; themeData = masterPsMob.configThemes_[i]; ++i) {
    var logoImage = themeData.logoImage();
    if (!logoImage) {
      continue;
    }

    // Include theme colors in the key because for the same logo image, the
    // background color in the HTML element may be different.
    var key = logoImage +
        pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor) +
        pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

    if (!map[key]) {
      map[key] = {themeData: themeData, count: 1};
    } else {
      ++map[key].count;
    }
  }

  // Sort the logo images in descending order.
  var list = goog.object.getValues(map);
  list.sort(function(a, b) {
    if (a.count < b.count) {
      return 1;
    }
    if (a.count > b.count) {
      return -1;
    }
    // Resolve a tie by comparing the logo URLs, so the order is stable.
    var aImage = a.themeData.logoImage();
    var bImage = b.themeData.logoImage();
    if (aImage < bImage) {
      return -1;
    } else if (aImage > bImage) {
      return 1;
    }
    return 0;
  });

  var message = '\nFinish site-wide theme extraction. ';
  if (list.length > 0) {
    message += 'Found ' + list.length +
        ' logo images. Details are shown below:\n';
    this.mobNav_.updateHeaderBar(this.masterWindow_(), list[0].themeData);

    var i;
    for (i in list) {
      themeData = list[i].themeData;
      message += '"' +
          pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor) +
          ' ' +
          pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor) +
          ' ' +
          themeData.logoImage() + '"' +
          ' COUNT: ' + list[i].count + '\n';
    }
    message += '\n';

    for (i in list) {
      themeData = list[i].themeData;
      message += 'ModPagespeedMobTheme "\n' +
          '    ' + goog.color.rgbArrayToHex(themeData.menuBackColor) + '\n' +
          '    ' + goog.color.rgbArrayToHex(themeData.menuFrontColor) + '\n' +
          '    ' + themeData.logoImage() + '"\n';
    }
  } else {
    message += 'No logo was found.';
  }
  message += '\n';
  pagespeed.MobUtil.consoleLog(message);
};


/**
 * Mobilize the next URL. Used for site-wide processing.
 * @param {boolean} finishOnTime True if theme extraction finished within
 *     CONFIG_CONFIG_MAX_TIME_MS_
 * @private
 */
pagespeed.Mob.prototype.mobilizeNextUrl_ = function(finishOnTime) {
  if (!finishOnTime) {
    pagespeed.MobUtil.consoleLog('Time out.');
  }

  var masterPsMob = this.psMobForMasterWindow_();
  var masterMobWindow = this.masterWindow_();
  if (masterPsMob) {
    masterMobWindow.clearTimeout(masterPsMob.configTimer_);
    masterPsMob.configTimer_ = null;

    var nextUrl = masterPsMob.configUrls_.pop();
    if (masterPsMob.configNumUrlsToProcess_ > 0 && nextUrl) {
      pagespeed.MobUtil.consoleLog('Next URL is ' + nextUrl + '. ' +
                                   masterPsMob.configNumUrlsToProcess_ +
                                   ' more to go.');
      var iframe = masterMobWindow.document.getElementById(
          pagespeed.Mob.CONFIG_IFRAME_ID_);
      if (iframe) {
        iframe.src = nextUrl;
        masterPsMob.configTimer_ = masterMobWindow.setTimeout(
            goog.bind(masterPsMob.mobilizeNextUrl_, masterPsMob),
            pagespeed.Mob.CONFIG_MAX_TIME_MS_);
      }
      --masterPsMob.configNumUrlsToProcess_;
    } else if (masterPsMob.configNumUrlsToProcess_ == 0) {
      this.showSiteWideThemes_();
      alert('All URLs have been processed. The header bar shows the ' +
            'best result. Details can be found in console log.');
      masterPsMob.configNumUrlsToProcess_ = -1;
    }
  }
};


/**
 * Collect links from the same origin in the current DOM.
 * @private
 * @return {!Array.<string>} urls
 */
pagespeed.Mob.prototype.collectUrls_ = function() {
  var urls = [];

  // Check whether the landing URL has 'ModPagespeedMobIframe'. If it does,
  // propagate this parameter to the collected URLs.
  var indexMobIframe = window.location.search.indexOf('ModPagespeedMobIframe');
  var mobIframeString = null;
  if (indexMobIframe != -1) {
    mobIframeString = window.location.search.substring(indexMobIframe);
    mobIframeString = mobIframeString.split('#')[0].split('&')[0];
    if (mobIframeString) {
      mobIframeString = '?' + mobIframeString;
    }
  }

  this.collectUrlsFromSubTree_(
      mobIframeString, /** @type {!Element} */ (window.document.body), urls);
  return urls;
};


/**
 * Collect links from the same origin in a sub-tree.
 * @private
 * @param {?string} mobIframeString String for the ModPagespeedMobIframe
 * @param {!Element} element Root element of the tree from which URLs will be
 *     collected
 * @param {!Array.<string>} urls
 */
pagespeed.Mob.prototype.collectUrlsFromSubTree_ = function(
    mobIframeString, element, urls) {
  if (urls.length >= pagespeed.Mob.CONFIG_MAX_NUM_LINKS_) {
    return;
  }

  var tag = element.tagName.toUpperCase();
  if (tag == goog.dom.TagName.A || tag == goog.dom.TagName.FORM) {
    var href = element.href;

    // Check if href is from the same origin but has a different path.
    if (href && !pagespeed.MobUtil.isCrossOrigin(href) &&
        goog.uri.utils.getPath(href).toLowerCase() !=
        window.location.pathname.toLowerCase()) {
      if (mobIframeString) {
        var indexSearch = href.indexOf('?');
        if (indexSearch == -1) {
          // Append '?ModPagespeedMobIframe=...' to href.
          href = href + mobIframeString;
        } else {
          // Replace '?' with '?ModPagespeedMobIframe=...&'.
          href = href.substring(0, indexSearch) + mobIframeString + '&' +
              href.substring(indexSearch + 1);
        }
      }

      if (urls.indexOf(href) == -1) {
        urls.push(href);
      }
    }
  }

  for (var childElement = element.firstElementChild;
       childElement;
       childElement = childElement.nextElementSibling) {
    this.collectUrlsFromSubTree_(mobIframeString, childElement, urls);
  }
};
