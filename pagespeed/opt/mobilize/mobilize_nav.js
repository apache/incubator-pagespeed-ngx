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

goog.provide('pagespeed.MobNav');

goog.require('goog.color');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.labs.userAgent.browser');
goog.require('goog.string');
goog.require('goog.structs.Set');
// goog.style adds ~400 bytes when using getSize and getTransformedSize.
goog.require('goog.style');
goog.require('mob.button.Dialer');
goog.require('mob.button.Map');
goog.require('mob.button.Menu');
goog.require('pagespeed.MobTheme');
goog.require('pagespeed.MobUtil');



/**
 * Create mobile navigation menus.
 * @constructor
 */
pagespeed.MobNav = function() {
  /**
   * The header bar element inserted at the top of the page. This is inserted by
   * C++.
   * @private {!Element}
   */
  this.headerBar_ =
      goog.dom.getRequiredElement(pagespeed.MobUtil.ElementId.HEADER_BAR);

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
  this.spacerDiv_ =
      goog.dom.getRequiredElement(pagespeed.MobUtil.ElementId.SPACER);

  /**
   * The span containing the logo.
   * @private {?Element}
   */
  this.logoSpan_ = null;

  /**
   * Menu button in the header bar. This can be null after configuration if no
   * nav section was inserted server side.
   * @private {?mob.button.Menu}
   */
  this.menuButton_ = null;

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
   * Side nav bar.
   * @private {?Element}
   */
  this.navPanel_ =
      document.getElementById(pagespeed.MobUtil.ElementId.NAV_PANEL);


  /**
   * Click detector div.
   * @private {?Element}
   */
  this.clickDetectorDiv_ = null;


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
   * The y coordinate of the previous move event, used to determine the
   * direction of scrolling.
   * @private {number}
   */
  this.lastScrollY_ = 0;

  /**
   * Bool to track state of the nav bar.
   * @private {boolean}
   */
  this.isNavPanelOpen_ = false;

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


  // From https://dev.opera.com/articles/opera-mini-and-javascript/
  this.isOperaMini_ = (navigator.userAgent.indexOf('Opera Mini') > -1);

  /**
   * Popup dialog for logo choices.  This is actually 2 different things:
   *   1. A table of logo choices and colors that can be clicked to change them.
   *   2. A PRE tag showing the config snippet for the chosen logo/colors.
   * @private {?Element}
   */
  this.logoChoicePopup_ = null;
};


/**
 * GIF image of an arrow icon, used to indicate hierarchical menus.
 * @private @const {string}
 */
pagespeed.MobNav.ARROW_ICON_ =
    'R0lGODlhkACQAPABAP///wAAACH5BAEAAAEALAAAAACQAJAAAAL+jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8YhMKpfMpvMJjUqn1Kr1is1qt9yu9wsO' +
    'i8fksvmMTqvX7Lb7DY/L5/S6/Y7P6/f8vh8EAJATKIhFWFhziEiluBjT6AgFGdkySclkeZmS' +
    'qYnE2VnyCUokOhpSagqEmtqxytrjurnqFGtSSztLcvu0+9HLm+sbPPWbURx1XJGMPHyxLPXs' +
    'EA3dLDFNXP1wzZjNsF01/W31LH6VXG6YjZ7Vu651674VG8/l2s1mL2qXn4nHD6nn3yE+Al+5' +
    '+fcnQL6EBui1QcUwgb6IEvtRVGDporc/RhobKOooLRBIbSNLmjyJMqXKlSxbunwJM6bMmTRr' +
    '2ryJM6fOnTx7+vwJNKjQoUSLGj2KNKnSpUybOn0KVUcBADs=';


/**
 * PNG image of a swap icon (drawn by hand).
 * TODO(huibao): optimize this image.
 * @private @const {string}
 */
pagespeed.MobNav.SWAP_ICON_ =
    'data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAIYAAABaCAAAAABY7nEZAAAABG' +
    'dBTUEAALGPC/xhBQAAAcpJREFUaN7t2ItxgkAQgOHtADvADkwH2oHpADrADlKCdoAdaAekA0' +
    'qgBEowYIZRE7jbe+xjktsCnE/m54CFm4qBxEgMIkavgwHlVQUDYHVoNTCGWZ86DYxhdudeA2' +
    'OYgExiMgIyicvwziQ6wy8TCoZHJkQM10zIGG6ZUDIcMiFmYDOhZ6AymWd8ogdwY8sEvP+j6x' +
    'gz4WMYM2FlLGfCzBgzqTUwiov81djWvXgb+bFzvFO26MEasqqVPkWXguBlLAbByDAFwcWwBM' +
    'HDsAbBwMAEQc1ABkHKwAdByHAJgorhGAQJI/8IWC5EYmRF2KolCmN/uYX+TDBjU0dYw4UyQo' +
    'JI69n/wmhKeUZ3WAMIM/rT2/34EGWc36fjXI7RlKvHU0WIcQ8CZBlTEKKMRxByjJcghBg/g5' +
    'BgzATBz5gNgpmxFAQnwxAEG8McBBPDFsTzAhY7rgxEED7jxMAFQctAB0HLqPeggTFcj+NGA2' +
    'Oso8o1MMZ7pcg0MFwyabDjeZgjM2F4tGEy4XnQWzNhe+0xZ8L4EmjKhPeVeDET9g+E+UwkPp' +
    'dmMpH5ePyVidin9GsmkouFp0yE1yxTJuJLp+9MNKzg2ipPC8nE+LuMLwqlrYBVqy8VAAAAAE' +
    'lFTkSuQmCC';


/**
 * Do a pre-pass over all the elements on the page and find out those that
 * need to add offset. The elements include
 * (a) all elements with 'position' specified as 'fixed' and 'top' specified in
 *     pixels.
 * (b) all elements with 'position' specified as 'absolute' or 'relative',
 *     and 'top' specified in pixels, with all ancestors up to document.body
 *     have 'static' 'position'.
 *
 * @param {!Element} element
 * @param {boolean} fixedPositionOnly
 * @private
 */
pagespeed.MobNav.prototype.findElementsToOffsetHelper_ = function(
    element, fixedPositionOnly) {
  if (!element.className ||
      (element.className != pagespeed.MobUtil.ElementId.PROGRESS_SCRIM &&
       !goog.string.startsWith(element.className, 'psmob-') &&
       !goog.string.startsWith(element.id, 'psmob-'))) {
    var style = window.getComputedStyle(element);
    var position = style.getPropertyValue('position');
    if (position != 'static') {
      var top = pagespeed.MobUtil.pixelValue(style.getPropertyValue('top'));
      if (top != null && (position == 'fixed' ||
                          (!fixedPositionOnly && position == 'absolute'))) {
        this.elementsToOffset_.add(element);
      }
      fixedPositionOnly = true;
    }

    for (var childElement = element.firstElementChild; childElement;
         childElement = childElement.nextElementSibling) {
      this.findElementsToOffsetHelper_(childElement, fixedPositionOnly);
    }
  }
};


/**
 * Do a pre-pass over all the elements on the page and find out those that
 * need to add offset.
 *
 * TODO(jud): This belongs in mobilize.js instead of mobilize_nav.js.
 * @private
 */
pagespeed.MobNav.prototype.findElementsToOffset_ = function() {
  if (window.document.body) {
    this.findElementsToOffsetHelper_(window.document.body,
        false /* search for elements with all allowed positions */);
  }
};


/**
 * Do a pre-pass over all the nodes on the page and clamp their z-index to
 * 999997.
 * TODO(jud): This belongs in mobilize.js instead of mobilize_nav.js.
 * @private
 */
pagespeed.MobNav.prototype.clampZIndex_ = function() {
  var elements = document.querySelectorAll('*');
  for (var i = 0, element; element = elements[i]; i++) {
    var id = element.id;
    if (id && (id == pagespeed.MobUtil.ElementId.PROGRESS_SCRIM ||
               id == pagespeed.MobUtil.ElementId.HEADER_BAR ||
               id == pagespeed.MobUtil.ElementId.NAV_PANEL)) {
      continue;
    }
    var style = window.getComputedStyle(element);
    // Set to 999997 because the click detector div is set to 999998 and the
    // menu bar and nav panel are set to 999999. This function runs before those
    // elements are added, so it won't modify their z-index.
    if (style.getPropertyValue('z-index') >= 999998) {
      pagespeed.MobUtil.consoleLog(
          'Element z-index exceeded 999998, setting to 999997.');
      element.style.zIndex = 999997;
    }
  }
};


/**
 * Redraw the header after scrolling or zooming finishes.
 * @private
 */
pagespeed.MobNav.prototype.redrawHeader_ = function() {
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

  var scale = 1;
  if (window.psDeviceType != 'desktop') {
    // screen.width does not update on rotation on ios, but it does on android,
    // so compensate for that here.
    if ((Math.abs(window.orientation) == 90) &&
        (screen.height > screen.width)) {
      scale = (window.innerHeight / screen.width);
    } else {
      scale = window.innerWidth / screen.width;
    }
  }
  // Android browser does not seem to take the pixel ratio into account in the
  // values it returns for screen.width and screen.height.
  if (this.isAndroidBrowser_) {
    scale *= goog.dom.getPixelRatio();
  }
  var scaleTransform = 'scale(' + scale.toString() + ')';
  this.headerBar_.style['-webkit-transform'] = scaleTransform;
  this.headerBar_.style.transform = scaleTransform;

  var width = window.innerWidth;
  if (window.getComputedStyle(document.body).getPropertyValue('overflow-y') !=
      'hidden') {
    // Subtract the width of the scrollbar from the viewport size only if the
    // scrollbars are visible (goog.style.getScrollbarWidth still returns the
    // scrollbar size if they are hidden).
    width -= goog.style.getScrollbarWidth();
  }
  this.headerBar_.style.width = (width / scale) + 'px';

  // Restore visibility since the bar was hidden while scrolling and zooming.
  goog.dom.classlist.remove(this.headerBar_, 'hide');

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
    var top = pagespeed.MobUtil.pixelValue(style.getPropertyValue('top'));

    if (position != 'static' && top != null) {
      if (this.redrawNavCalled_) {
        var oldTop = el.style.top;
        oldTop = pagespeed.MobUtil.pixelValue(el.style.top);
        if (oldTop != null) {
          el.style.top = String(oldTop + (newHeight - oldHeight)) + 'px';
        }
      } else {
        var elTop = pagespeed.MobUtil.boundingRect(el).top;
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
  this.headerBar_.style.top = window.scrollY + 'px';
  this.headerBar_.style.left = window.scrollX + 'px';
  this.redrawNavCalled_ = true;
};


/**
 * Redraw the nav panel based on current zoom level.
 * @private
 */
pagespeed.MobNav.prototype.redrawNavPanel_ = function() {
  if (!this.navPanel_) {
    return;
  }

  var scale = window.innerWidth / goog.dom.getViewportSize().width;

  var transform = 'scale(' + scale + ')';
  this.navPanel_.style['-webkit-transform'] = transform;
  this.navPanel_.style.transform = transform;

  var xOffset = goog.dom.classlist.contains(this.navPanel_, 'open') ?
                    0 :
                    (-goog.style.getTransformedSize(this.navPanel_).width);

  this.navPanel_.style.top = window.scrollY + 'px';
  this.navPanel_.style.left = window.scrollX + xOffset + 'px';


  var headerBarBox = this.headerBar_.getBoundingClientRect();
  this.navPanel_.style.marginTop = headerBarBox.height + 'px';

  // Opera mini does not support the css3 scale transformation nor the touch
  // events that we use heavily here. As a workaround, we don't set the height
  // here which allows the nav panel to fit the content. The user is then able
  // to pinch zoom and see the whole menu, rather than scrolling the menu div.
  if (!this.isOperaMini_) {
    this.navPanel_.style.height =
        ((window.innerHeight - headerBarBox.height) / scale) + 'px';
  }
};


/**
 * Add events for capturing header bar resize and calling the appropriate redraw
 * events after scrolling and zooming.
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBarResizeEvents_ = function() {
  // Draw the header bar initially.
  this.redrawHeader_();
  this.redrawNavPanel_();

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
        this.redrawNavPanel_();
        this.redrawHeader_();
      }
      this.scrollTimer_ = null;
    }, this), 200);
  };

  // Don't redraw the header bar unless there has not been a scroll event for 50
  // ms and there are no touches currently on the screen. This keeps the
  // redrawing from happening until scrolling is finished.
  var scrollHandler = function(e) {
    if (!this.isNavPanelOpen_) {
      goog.dom.classlist.add(this.headerBar_, 'hide');
    }

    resetScrollTimer.call(this);

    if (this.navPanel_ && this.isNavPanelOpen_ &&
        !this.navPanel_.contains(/** @type {?Node} */ (e.target))) {
      e.stopPropagation();
      e.preventDefault();
    }
  };

  window.addEventListener(goog.events.EventType.SCROLL,
                          goog.bind(scrollHandler, this), false);

  // Keep track of number of touches currently on the screen so that we don't
  // redraw until scrolling and zooming is finished.
  window.addEventListener(goog.events.EventType.TOUCHSTART,
                          goog.bind(function(e) {
                            this.currentTouches_ = e.touches.length;
                            this.lastScrollY_ = e.touches[0].clientY;
                          }, this), false);

  window.addEventListener(goog.events.EventType.TOUCHMOVE,
                          goog.bind(function(e) {
                            // The default android browser is unreliable about
                            // firing touch events if preventDefault is not
                            // called in touchstart (see note above). Therefore,
                            // we don't hide the nav panel here on the android
                            // browser, otherwise the bar is not redrawn if a
                            // user tries to scroll up past the top of the page,
                            // since neither a touchend event nor a scroll event
                            // fires to redraw the header.
                            if (!this.isNavPanelOpen_) {
                              if (!this.isAndroidBrowser_) {
                                goog.dom.classlist.add(this.headerBar_, 'hide');
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

  // TODO(jud): Note that we don't currently handle orientation change quite
  // right, since the window height and width don't switch unless the page is
  // reloaded on some browsers.
  window.addEventListener(goog.events.EventType.ORIENTATIONCHANGE,
                          goog.bind(function() {
                            this.redrawNavPanel_();
                            this.redrawHeader_();
                          }, this), false);
};


/**
 * Insert a header bar to the top of the page. For this goal, firstly we
 * insert an empty div so all contents, except those with fixed position,
 * are pushed down. Then we insert the header bar. The header bar may contain
 * a hamburger icon, a logo image, and a call button.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addHeaderBar_ = function(themeData) {
  // Move the header bar and spacer div back to the top of the body, in case
  // some other JS on the page inserted some elements.
  document.body.insertBefore(this.spacerDiv_, document.body.childNodes[0]);
  document.body.insertBefore(this.headerBar_, this.spacerDiv_);
  if (window.psLabeledMode) {
    goog.dom.classlist.add(this.headerBar_,
                           pagespeed.MobUtil.ElementClass.LABELED);
  }

  // Add menu button.
  if (this.navPanel_ && !window.psLabeledMode) {
    this.menuButton_ = new mob.button.Menu(
        themeData.menuFrontColor, goog.bind(this.toggleNavPanel_, this));
    this.headerBar_.appendChild(this.menuButton_.el);
  }

  // Add the logo.
  this.logoSpan_ = themeData.anchorOrSpan;
  this.headerBar_.appendChild(themeData.anchorOrSpan);
  this.headerBar_.style.borderBottomColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
  this.headerBar_.style.backgroundColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

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

  if (window.psDeviceType == 'mobile' || window.psDeviceType == 'tablet') {
    goog.dom.classlist.add(this.headerBar_, 'mobile');
    goog.dom.classlist.add(this.spacerDiv_, 'mobile');
    if (this.navPanel_) {
      goog.dom.classlist.add(this.navPanel_, 'mobile');
    }
  }

  // If we are in labeled mode or only 1 button is configured, then show the
  // text next to it.
  if (window.psLabeledMode || (this.dialer_ && !this.mapButton_) ||
      (!this.dialer_ && this.mapButton_)) {
    goog.dom.classlist.add(this.headerBar_,
                           pagespeed.MobUtil.ElementClass.SHOW_BUTTON_TEXT);
  }

  this.addHeaderBarResizeEvents_();
  this.addThemeColor_(themeData);
};


/**
 * Insert a style tag at the end of the head with the theme colors. The member
 * bool useDetectedThemeColor controls whether we use the color detected from
 * mob_logo.js, or a predefined color.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addThemeColor_ = function(themeData) {
  // Remove any prior style block.
  if (this.styleTag_) {
    this.styleTag_.parentNode.removeChild(this.styleTag_);
  }

  var backgroundColor =
      pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);
  var color = pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
  var css = '#' + pagespeed.MobUtil.ElementId.HEADER_BAR +
            ' { background-color: ' + backgroundColor + '; }\n' +
            '#' + pagespeed.MobUtil.ElementId.HEADER_BAR + ' * ' +
            ' { color: ' + color + '; }\n' +
            '#' + pagespeed.MobUtil.ElementId.NAV_PANEL +
            ' { background-color: ' + color + '; }\n' +
            '#' + pagespeed.MobUtil.ElementId.NAV_PANEL + ' * ' +
            ' { color: ' + backgroundColor + '; }\n';
  this.styleTag_ = document.createElement(goog.dom.TagName.STYLE);
  this.styleTag_.type = 'text/css';
  this.styleTag_.appendChild(document.createTextNode(css));
  document.head.appendChild(this.styleTag_);
};


/**
 * Add a div for detecting clicks on the body in order to close the open nav
 * panel. This is to workaround JS that sends click events, which can be
 * difficult to differentiate from actual clicks from the user. In particular
 * https://github.com/DevinWalker/jflow/ causes this issue.
 * @private
 */
pagespeed.MobNav.prototype.addClickDetectorDiv_ = function() {
  this.clickDetectorDiv_ = document.createElement(goog.dom.TagName.DIV);
  this.clickDetectorDiv_.id = pagespeed.MobUtil.ElementId.CLICK_DETECTOR_DIV;
  document.body.insertBefore(this.clickDetectorDiv_, this.navPanel_);

  this.clickDetectorDiv_.addEventListener(
      goog.events.EventType.CLICK, goog.bind(function(e) {
        if (goog.dom.classlist.contains(this.navPanel_, 'open')) {
          this.toggleNavPanel_();
        }
      }, this), false);
};


/**
 * Add properly themed arrow icons to the submenus of the nav menu.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addSubmenuArrows_ = function(themeData) {
  var submenuTitleAs = this.navPanel_.querySelectorAll(
      goog.dom.TagName.DIV + ' > ' + goog.dom.TagName.A);
  var n = submenuTitleAs.length;
  if (n == 0) {
    return;
  }
  var arrowIcon = pagespeed.MobUtil.synthesizeImage(
      pagespeed.MobNav.ARROW_ICON_, themeData.menuBackColor);
  if (!arrowIcon) {
    return;
  }
  for (var i = 0; i < n; i++) {
    var icon = document.createElement(goog.dom.TagName.IMG);
    var submenu = submenuTitleAs[i];
    submenu.insertBefore(icon, submenu.firstChild);
    icon.setAttribute('src', arrowIcon);
    goog.dom.classlist.add(icon,
                           pagespeed.MobUtil.ElementClass.MENU_EXPAND_ICON);
  }
};


/**
 * Style the nav panel (which has been inserted server side), and register event
 * handlers for it.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 * @private
 */
pagespeed.MobNav.prototype.addNavPanel_ = function(themeData) {
  // TODO(jud): Make sure we have tests covering the redraw flow and the events
  // called here.
  this.addSubmenuArrows_(themeData);
  this.addClickDetectorDiv_();

  // Track touch move events just in the nav panel so that scrolling can be
  // controlled. This is to work around overflow: hidden not working as we would
  // want when zoomed in (it does not totally prevent scrolling).
  this.navPanel_.addEventListener(
      goog.events.EventType.TOUCHMOVE, goog.bind(function(e) {
        if (!this.isNavPanelOpen_) {
          return;
        }

        var currentY = e.touches[0].clientY;
        // If the event is not scrolling (pinch zoom for exaple), then prevent
        // it while the nav panel is open.
        if (e.touches.length != 1) {
          e.preventDefault();
        } else {
          // Check if we are scrolling horizontally or scrolling up past the top
          // or below the bottom. If so, stop the scroll event from happening
          // since otherwise the body behind the nav panel will also scroll.
          var scrollUp = currentY > this.lastScrollY_;
          var navPanelAtTop = (this.navPanel_.scrollTop == 0);
          // Add 1 pixel to account for rounding errors.
          var navPanelAtBottom =
              (this.navPanel_.scrollTop >=
               (this.navPanel_.scrollHeight - this.navPanel_.offsetHeight - 1));

          if (e.cancelable && ((scrollUp && navPanelAtTop) ||
                               (!scrollUp && navPanelAtBottom))) {
            e.preventDefault();
          }
          // Keep other touchmove events from happening. This function is not
          // supported on the android 2.3 stock browser.
          if (e.stopImmediatePropagation) {
            e.stopImmediatePropagation();
          }
          this.lastScrollY_ = currentY;
        }
      }, this), false);
  this.redrawNavPanel_();
};


/**
 * Event handler for clicks on the hamburger menu. Toggles the state of the nav
 * panel so that is opens/closes.
 * @private
 */
pagespeed.MobNav.prototype.toggleNavPanel_ = function() {
  pagespeed.MobUtil.sendBeacon(
      (goog.dom.classlist.contains(this.navPanel_, 'open') ?
           pagespeed.MobUtil.BeaconEvents.MENU_BUTTON_CLOSE :
           pagespeed.MobUtil.BeaconEvents.MENU_BUTTON_OPEN));
  goog.dom.classlist.toggle(this.headerBar_, 'open');
  goog.dom.classlist.toggle(this.navPanel_, 'open');
  goog.dom.classlist.toggle(this.clickDetectorDiv_, 'open');
  goog.dom.classlist.toggle(document.body, 'noscroll');
  this.isNavPanelOpen_ = !this.isNavPanelOpen_;
  this.redrawNavPanel_();
};


/**
 * Add events to the buttons in the nav panel.
 * @private
 */
pagespeed.MobNav.prototype.addNavButtonEvents_ = function() {
  var navDivs = document.querySelectorAll(
      '#' + pagespeed.MobUtil.ElementId.NAV_PANEL + ' div');
  for (var i = 0, div; div = navDivs[i]; ++i) {
    div.addEventListener(goog.events.EventType.CLICK, function(e) {
      // These divs have href='#' (set by the server), so prevent default to
      // keep it from changing the URL.
      e.preventDefault();
      var target = e.currentTarget;
      // A click was registered on the div that has the hierarchical menu text
      // and icon. Open up the UL, which should be the next element.
      goog.dom.classlist.toggle(target.nextSibling, 'open');
      // Also toggle the expand icon, which will be the first child of the P
      // tag, which is the first child of the target div.
      goog.dom.classlist.toggle(target.firstChild.firstChild, 'open');
    }, false);
  }
};


/**
 * Main entry point of nav mobilization. Should be called when logo detection is
 * finished.
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobNav.prototype.Run = function(themeData) {
  // Don't insert the header bar inside of an iframe.
  if (pagespeed.MobUtil.inFriendlyIframe()) {
    return;
  }
  this.clampZIndex_();
  this.findElementsToOffset_();
  this.addHeaderBar_(themeData);

  // Don't insert nav stuff if there are no navigational sections on the page.
  if (this.navPanel_) {
    // Make sure the navPanel is in the body of the document; we've seen it
    // moved elsewhere by JS on the page.
    document.body.appendChild(this.navPanel_);

    this.addNavPanel_(themeData);
    this.addNavButtonEvents_();
  }

  pagespeed.MobUtil.sendBeacon(pagespeed.MobUtil.BeaconEvents.NAV_DONE);

  window.addEventListener(goog.events.EventType.LOAD,
      goog.bind(this.redrawHeader_, this));
};


/**
 * Updates header bar using the theme data.
 * @param {!Object} mobWindow
 * @param {!pagespeed.MobUtil.ThemeData} themeData
 */
pagespeed.MobNav.prototype.updateHeaderBar = function(mobWindow, themeData) {
  var logoImage = themeData.logoImage();
  if (logoImage) {
    var frontColor =
        pagespeed.MobUtil.colorNumbersToString(themeData.menuFrontColor);
    var backColor =
        pagespeed.MobUtil.colorNumbersToString(themeData.menuBackColor);

    var logoElement = mobWindow.document.getElementById(
        pagespeed.MobUtil.ElementId.LOGO_IMAGE);
    if (logoElement) {
      logoElement.parentNode.replaceChild(themeData.logoElement, logoElement);
      //logoElement.src = logoImage;
      if (this.headerBar_) {
        this.headerBar_.style.backgroundColor = backColor;
      }
      this.logoSpan_.style.backgroundColor = backColor;
      //logoElement.style.backgroundColor = backColor;
    }

    var hamburgerLines = mobWindow.document.getElementsByClassName(
        pagespeed.MobUtil.ElementClass.HAMBURGER_LINE);
    for (var i = 0, line; line = hamburgerLines[i]; ++i) {
      line.style.backgroundColor = frontColor;
    }
  }
};


// TODO(jmarantz): All the .chooser* methods should be loaded only with
// ?PageSpeedMobConfig=on, plus SWAP_ICON declared above.


/**
 * @param {!Array.<!pagespeed.MobLogoCandidate>} candidates
 */
pagespeed.MobNav.prototype.chooserShowCandidates = function(candidates) {
  if (this.logoChoicePopup_) {
    this.chooserDismissLogoChoicePopup_();
    return;
  }

  var table = document.createElement(goog.dom.TagName.TABLE);
  table.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_TABLE;

  var thead = document.createElement(goog.dom.TagName.THEAD);
  table.appendChild(thead);
  var trow = document.createElement(goog.dom.TagName.TR);
  thead.appendChild(trow);
  function addData() {
    var td = document.createElement(goog.dom.TagName.TD);
    trow.appendChild(td);
    return td;
  }
  trow.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_COLUMN_HEADER;
  addData().textContent = 'Logo';
  addData().textContent = 'Foreground';
  addData().textContent = '';
  addData().textContent = 'Background';

  var tbody = document.createElement(goog.dom.TagName.TBODY);
  table.appendChild(tbody);
  for (var i = 0; i < candidates.length; ++i) {
    trow = document.createElement(goog.dom.TagName.TR);
    trow.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_CHOICE;
    tbody.appendChild(trow);
    var candidate = candidates[i];
    var themeData = pagespeed.MobTheme.synthesizeLogoSpan(
        candidate.logoRecord, candidate.background, candidate.foreground);
    addData().appendChild(themeData.anchorOrSpan);
    var img = themeData.logoElement;
    img.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_IMAGE;
    img.onclick = goog.bind(this.chooserSetLogo_, this, candidate);

    var foreground = addData();
    foreground.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.foreground);
    foreground.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_COLOR;

    var swapTd = addData();
    swapTd.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_COLOR;
    var swapImg = document.createElement(goog.dom.TagName.IMG);
    swapImg.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_SWAP;
    swapImg.src = pagespeed.MobNav.SWAP_ICON_;
    swapTd.appendChild(swapImg);

    var background = addData();
    background.style.backgroundColor =
        goog.color.rgbArrayToHex(candidate.background);
    background.className = pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_SWAP;

    swapTd.onclick = goog.bind(this.chooserSwapColors_, this, candidate,
        foreground, background);
  }

  this.chooserDisplayPopup_(table);
};


/**
 * @param {!Element} popup
 * @private
 */
pagespeed.MobNav.prototype.chooserDisplayPopup_ = function(popup) {
  // The natural width of the table is about 350px, and we'll
  // want it to occupy 2/3 of the screen.  We'll add it to the DOM
  // hidden so we can get the width computed by the browser, and
  // thereby know how to center it.
  popup.style.visibility = 'hidden';
  document.body.appendChild(popup);

  var naturalWidth = popup.offsetWidth;
  var fractionOfScreen = 2.0 / 3.0;
  var scale = window.innerWidth * fractionOfScreen / naturalWidth;
  var offset = Math.round(0.5 * (1 - fractionOfScreen) * window.innerWidth) +
      'px';

  var transform =
      'scale(' + scale + ')' +
      ' translate(' + offset + ',' + offset + ')';
  popup.style['-webkit-transform'] = transform;
  popup.style.transform = transform;

  // Now that we have transformed it, make it show up.
  popup.style.visibility = 'visible';

  if (this.logoChoicePopup_ != null) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
  }
  this.logoChoicePopup_ = popup;
};


/**
 * Sets the logo in response to clicking on an image in the logo chooser
 * popup.
 * @param {!pagespeed.MobLogoCandidate} candidate
 * @private
 */
pagespeed.MobNav.prototype.chooserSetLogo_ = function(candidate) {
  var themeData = pagespeed.MobTheme.createThemeData(
      candidate.logoRecord, candidate.background, candidate.foreground);
  pagespeed.MobTheme.installLogo(themeData);
  this.updateHeaderBar(window, themeData);
  this.addThemeColor_(themeData);

  var configSnippet = document.createElement(goog.dom.TagName.PRE);
  configSnippet.className =
      pagespeed.MobUtil.ElementClass.LOGO_CHOOSER_CONFIG_FRAGMENT;

  // TODO(jmarantz): Generate nginx syntax as needed.
  configSnippet.textContent =
      'ModPagespeedMobTheme "\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuBackColor) + '\n' +
      '    ' + goog.color.rgbArrayToHex(themeData.menuFrontColor) + '\n' +
      '    ' + themeData.logoElement.src + '"';
  this.chooserDisplayPopup_(configSnippet);

  // TODO(jmarantz): consider adding a note to this popup that about how
  // you can touch the logo to bring it up the chooser again.
};


/**
 * Swaps the background and colors for a logo candidate.
 * @param {!pagespeed.MobLogoCandidate} candidate
 * @param {!Element} foregroundTd table data element (TD) for the foreground
 * @param {!Element} backgroundTd table data element (TD) for the background
 * @private
 */
pagespeed.MobNav.prototype.chooserSwapColors_ = function(
    candidate, foregroundTd, backgroundTd) {
  // TODO(jmarantz): we probably only want to swap the fg/bg for the menus,
  // and not for the header bar.  The logo background computation is generally
  // correct, as far as I can tell, and it's only a question of whether the
  // menus would look better in reverse video.
  var tmp = candidate.background;
  candidate.background = candidate.foreground;
  candidate.foreground = tmp;
  tmp = foregroundTd.style['background-color'];
  foregroundTd.style['background-color'] =
      backgroundTd.style['background-color'];
  backgroundTd.style['background-color'] = tmp;
};


/**
 * Dismisses any logo-choice pop.
 * @private
 */
pagespeed.MobNav.prototype.chooserDismissLogoChoicePopup_ = function() {
  if (this.logoChoicePopup_) {
    this.logoChoicePopup_.parentNode.removeChild(this.logoChoicePopup_);
    this.logoChoicePopup_ = null;
  }
};
