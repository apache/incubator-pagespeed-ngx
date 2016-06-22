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
 */

// TODO(jud): Test on a wider range of browsers and restrict to only modern
// browsers. This uses a few modern web features, like css3 animations, that are
// not supported on opera mini for example.

goog.provide('mob.Nav');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.labs.userAgent.browser');
goog.require('goog.structs.Set');
// goog.style adds ~400 bytes when using getSize and getTransformedSize.
goog.require('goog.style');
goog.require('mob.HelpPanel');
goog.require('mob.button.Dialer');
goog.require('mob.button.Map');
goog.require('mob.button.Menu');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ElementClass');
goog.require('mob.util.ElementId');



/**
 * Create mobile navigation menus.
 * @constructor
 */
mob.Nav = function() {
  /**
   * The header bar element inserted at the top of the page. This is inserted by
   * C++.
   * @private {!Element}
   */
  this.headerBar_ = goog.dom.getRequiredElement(mob.util.ElementId.HEADER_BAR);

  /**
   * The style tag used to style the nav elements.
   * @private {?Element}
   */
  this.styleTag_ = null;

  /**
   * Spacer div element inserted at the top of the page to push the rest of the
   * content down. This is inserted in C++.
   * @private {!Element}
   */
  this.spacerDiv_ = goog.dom.getRequiredElement(mob.util.ElementId.SPACER);

  /**
   * @private {?mob.button.Dialer}
   */
  this.dialer_ = null;

  /**
   * Map button in the header bar.
   * @private {?mob.button.Map}
   */
  this.mapButton_ = null;

  /**
   * Menu button in the header bar.
   * @private {?mob.button.Menu}
   */
  this.menuButton_ = null;

  /**
   * Pop up help panel.
   * @private {?mob.HelpPanel}
   */
  this.helpPanel_ = new mob.HelpPanel(
      goog.dom.getRequiredElement(mob.util.ElementId.IFRAME).src);

  /**
   * Tracks time since last scroll to figure out when scrolling is finished.
   * Will be null when not scrolling.
   * @private {?number}
   */
  this.scrollTimer_ = null;


  /**
   * Tracks number of current touches to know when scrolling is finished.
   * @private {number}
   */
  this.currentTouches_ = 0;


  /**
   * Track's the header bar height so we can tell if it changes between redraws.
   * -1 indicates that it has not been calculated before.
   * @private {number}
   */
  this.headerBarHeight_ = -1;

  /**
   * Tracks the elements which need to be adjusted after zooming to account for
   * the offset of the spacer div. This includes fixed position elements, and
   * absolute position elements rooted at the body.
   * @private {!goog.structs.Set}
   */
  this.elementsToOffset_ = new goog.structs.Set();

  /**
   * Tracks if the redrawNav function has been called yet or not, to enable
   * slightly different behavior on first vs subsequent calls.
   * @private {boolean}
   */
  this.redrawNavCalled_ = false;


  /**
   * Tracks if we are on the stock android browser. Touch events are broken on
   * old version of the android browser, so we have to provide some fallback
   * functionality for them.
   * @private {boolean}
   */
  this.isAndroidBrowser_ = goog.labs.userAgent.browser.isAndroidBrowser();

  /**
   * Use position fixed on browsers that have broken scroll event behavior.
   * @private {boolean}
   */
  this.isIosWebview_ = ((window.navigator.userAgent.indexOf('CriOS') > -1) ||
                        (window.navigator.userAgent.indexOf('GSA') > -1) ||
                        goog.labs.userAgent.browser.isIosWebview());
};


/**
 * The minimum font size required for the header bar to work. If the browser
 * enforces a minimum font size larger than this then we don't display the
 * header bar.
 * TODO(jud): Set this to something higher and scale down the em values in
 * mobilize.css
 * @const @private {number}
 */
mob.Nav.MIN_FONT_SIZE_ = 1;


/**
 * Redraw the header after scrolling or zooming finishes.
 * @private
 */
mob.Nav.prototype.redraw_ = function() {
  // We don't actually expect this to be called without headerBar_ being set,
  // but getTransformedSize requires a non-null param, so this coerces the
  // closure compiler into recognizing that.
  if (!this.headerBar_) {
    return;
  }

  // Use getTransformedSize to take into account the scale transformation.
  var oldHeight =
      this.headerBarHeight_ == -1 ?
          Math.round(goog.style.getTransformedSize(this.headerBar_).height) :
          this.headerBarHeight_;

  var fontSize = mob.util.getZoomLevel();
  this.headerBar_.style.fontSize = fontSize + 'px';

  var width = window.innerWidth;
  if (window.getComputedStyle(document.body).getPropertyValue('overflow-y') !=
      'hidden') {
    // Subtract the width of the scrollbar from the viewport size only if the
    // scrollbars are visible (goog.style.getScrollbarWidth still returns the
    // scrollbar size if they are hidden).
    width -= goog.style.getScrollbarWidth();
  }
  this.headerBar_.style.width = (width / fontSize) + 'em';

  // Restore visibility since the bar was hidden while scrolling and zooming.
  goog.dom.classlist.remove(this.headerBar_, mob.util.ElementClass.HIDE);

  var newHeight =
      Math.round(goog.style.getTransformedSize(this.headerBar_).height);

  // Update the size of the spacer div to take into account the changed relative
  // size of the header. Changing the size of the spacer div will also move the
  // rest of the content on the page. For example, when zooming in, we shrink
  // the size of the header bar, which causes the page to move up slightly. To
  // compensate, we adjust the scroll amount by the difference between the old
  // and new sizes of the spacer div.
  this.spacerDiv_.style.height = newHeight + 'px';
  this.headerBarHeight_ = newHeight;

  // Add offset to the elements which need to be moved. On the first run of this
  // function, they are offset by the size of the spacer div. On subsequent
  // runs, they are offset by the difference between the old and the new size of
  // the spacer div.
  var offsets = this.elementsToOffset_.getValues();
  for (var i = 0; i < offsets.length; i++) {
    var el = offsets[i];
    var style = window.getComputedStyle(el);
    var position = style.getPropertyValue('position');
    var top = mob.util.pixelValue(style.getPropertyValue('top'));

    if (position != 'static' && top != null) {
      if (this.redrawNavCalled_) {
        var oldTop = el.style.top;
        oldTop = mob.util.pixelValue(el.style.top);
        if (oldTop != null) {
          el.style.top = String(oldTop + (newHeight - oldHeight)) + 'px';
        }
      } else {
        var elTop = mob.util.boundingRect(el).top;
        el.style.top = String(elTop + newHeight) + 'px';
      }
    }
  }

  // Calling scrollBy will trigger a scroll event, but we've already updated
  // the size of everything, so set a flag that we should ignore the next
  // scroll event. Due to rounding errors from getTransformedSize returning a
  // float, newHeight and oldHeight can differ by 1 pixel, which will cause this
  // routine to get stuck firing in a loop as it tries and fails to compensate
  // for the 1 pixel difference.
  if (this.redrawNavCalled_ && newHeight != oldHeight) {
    window.scrollBy(0, (newHeight - oldHeight));
  }

  // Redraw the bar to the top of page by offsetting by the amount scrolled.
  if (!this.isIosWebview_) {
    this.headerBar_.style.top = window.scrollY + 'px';
    this.headerBar_.style.left = window.scrollX + 'px';
  }
  this.redrawNavCalled_ = true;
  this.helpPanel_.redraw();
};


/**
 * Add events for capturing header bar resize and calling the appropriate redraw
 * events after scrolling and zooming.
 * @private
 */
mob.Nav.prototype.addHeaderBarResizeEvents_ = function() {
  // Draw the header bar initially.
  this.redraw_();

  // Setup a 200ms delay to redraw the header and nav panel. The timer gets
  // reset upon each touchend and scroll event to ensure that the redraws happen
  // after scrolling and zooming are finished.
  var resetScrollTimer = function() {
    if (this.scrollTimer_ != null) {
      window.clearTimeout(this.scrollTimer_);
      this.scrollTimer_ = null;
    }
    this.scrollTimer_ = window.setTimeout(goog.bind(function() {
      // Stock android browser has a longstanding bug where touchend events are
      // not fired unless preventDefault is called in touchstart. However, doing
      // so prevents scrolling (which is the default behavior). Since those
      // events aren't fired, currentTouches_ does not get updated correctly. To
      // workaround, we fallback to the slightly jankier behavior of just
      // redrawing after a scroll event, even if there are potentially touches
      // still happening.
      // https://code.google.com/p/android/issues/detail?id=19827 for details on
      // the bug in question.
      if (this.isAndroidBrowser_ || this.currentTouches_ == 0) {
        this.redraw_();
      }
      this.scrollTimer_ = null;
    }, this), 200);
  };

  // Don't redraw the header bar unless there has not been a scroll event for 50
  // ms and there are no touches currently on the screen. This keeps the
  // redrawing from happening until scrolling is finished.
  var scrollHandler = function(e) {
    resetScrollTimer.call(this);
    goog.dom.classlist.add(this.headerBar_, mob.util.ElementClass.HIDE);
  };

  window.addEventListener(goog.events.EventType.SCROLL,
                          goog.bind(scrollHandler, this), false);

  // Keep track of number of touches currently on the screen so that we don't
  // redraw until scrolling and zooming is finished.
  window.addEventListener(goog.events.EventType.TOUCHSTART,
                          goog.bind(function(e) {
                            this.currentTouches_ = e.touches.length;
                          }, this), false);

  window.addEventListener(
      goog.events.EventType.TOUCHMOVE, goog.bind(function(e) {
        // The default android browser is unreliable about firing touch events
        // if preventDefault is not called in touchstart (see note above).
        // Therefore, we don't hide the nav panel here on the android browser,
        // otherwise the bar is not redrawn if a user tries to scroll up past
        // the top of the page, since neither a touchend event nor a scroll
        // event fires to redraw the header.
        if (!this.helpPanel_.isOpen()) {
          if (!this.isAndroidBrowser_) {
            goog.dom.classlist.add(this.headerBar_, mob.util.ElementClass.HIDE);
          }
        } else {
          e.preventDefault();
        }
      }, this), false);

  window.addEventListener(goog.events.EventType.TOUCHEND,
                          goog.bind(function(e) {
                            this.currentTouches_ = e.touches.length;
                            // Redraw the header bar if there are no more
                            // current touches.
                            if (this.currentTouches_ == 0) {
                              resetScrollTimer.call(this);
                            }
                          }, this), false);

  window.addEventListener(goog.events.EventType.ORIENTATIONCHANGE,
                          goog.bind(function() { this.redraw_(); }, this),
                          false);

  window.addEventListener(goog.events.EventType.RESIZE, goog.bind(function() {
    this.redraw_();
  }, this), false);
};


/**
 * Insert a header bar to the top of the page. The header bar may contain a call
 * button, a navigation button and a dot menu.
 * @param {!mob.util.ThemeData} themeData
 * @private
 */
mob.Nav.prototype.addHeaderBar_ = function(themeData) {
  if (this.isIosWebview_) {
    goog.dom.classlist.add(this.headerBar_, mob.util.ElementClass.IOS_WEBVIEW);
  }

  this.headerBar_.style.borderBottomColor =
      mob.util.colorNumbersToString(themeData.menuFrontColor);
  this.headerBar_.style.backgroundColor =
      mob.util.colorNumbersToString(themeData.menuBackColor);

  // Add call button.
  if (window.psPhoneNumber) {
    this.dialer_ = new mob.button.Dialer(
        themeData.menuFrontColor, window.psPhoneNumber, window.psConversionId,
        window.psPhoneConversionLabel);
    this.headerBar_.appendChild(this.dialer_.el);
  }

  // Add the map button.
  if (window.psMapLocation) {
    this.mapButton_ =
        new mob.button.Map(themeData.menuFrontColor, window.psMapLocation,
                           window.psConversionId, window.psMapConversionLabel);
    this.headerBar_.appendChild(this.mapButton_.el);
  }

  this.menuButton_ = new mob.button.Menu(
      themeData.menuFrontColor,
      goog.bind(this.helpPanel_.toggle, this.helpPanel_));
  this.headerBar_.appendChild(this.menuButton_.el);

  this.addHeaderBarResizeEvents_();
  this.addThemeColor_(themeData);
};


/**
 * Insert a style tag at the end of the head with the theme colors. The member
 * bool useDetectedThemeColor controls whether we use the color detected from
 * mob_logo.js, or a predefined color.
 * @param {!mob.util.ThemeData} themeData
 * @private
 */
mob.Nav.prototype.addThemeColor_ = function(themeData) {
  // Remove any prior style block.
  if (this.styleTag_) {
    this.styleTag_.parentNode.removeChild(this.styleTag_);
  }

  var backgroundColor = mob.util.colorNumbersToString(themeData.menuBackColor);
  var color = mob.util.colorNumbersToString(themeData.menuFrontColor);
  var css = '#' + mob.util.ElementId.HEADER_BAR + ' { background-color: ' +
      backgroundColor + '; }\n' +
      '#' + mob.util.ElementId.HEADER_BAR + ' * ' +
      ' { color: ' + color + '; }\n';
  this.styleTag_ = document.createElement(goog.dom.TagName.STYLE);
  this.styleTag_.type = 'text/css';
  this.styleTag_.appendChild(document.createTextNode(css));
  document.head.appendChild(this.styleTag_);
};


/**
 * Check if the browser has a minimum font size set. This can cause issues
 * because font-size and em units are used to scale the header bar.
 * @private
 * @return {boolean}
 */
mob.Nav.prototype.isMinimumFontSizeSet_ = function() {
  // TODO(jud): Insert this div in c++ to prevent forcing a style recalculation
  // here.
  var testDiv = document.createElement(goog.dom.TagName.DIV);
  document.body.appendChild(testDiv);
  testDiv.style.fontSize = '1px';
  var fontSize = window.getComputedStyle(testDiv).getPropertyValue('font-size');
  fontSize = mob.util.pixelValue(fontSize);
  document.body.removeChild(testDiv);
  return (!fontSize || fontSize > mob.Nav.MIN_FONT_SIZE_);
};


/**
 * Main entry point of nav mobilization. Should be called when logo detection is
 * finished.
 * @param {!mob.util.ThemeData} themeData
 */
mob.Nav.prototype.run = function(themeData) {
  if (this.isMinimumFontSizeSet_()) {
    return;
  }
  // Don't insert the header bar inside of an iframe.
  if (mob.util.inFriendlyIframe()) {
    return;
  }
  this.addHeaderBar_(themeData);

  mob.util.sendBeaconEvent(mob.util.BeaconEvents.NAV_DONE);

  window.addEventListener(goog.events.EventType.LOAD,
                          goog.bind(this.redraw_, this));
};
