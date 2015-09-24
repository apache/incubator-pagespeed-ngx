/*
 * Copyright 2015 Google Inc.
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

goog.provide('mob.NavPanel');

goog.require('goog.Uri');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.style');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ElementClass');
goog.require('mob.util.ElementId');



/**
 * Create mobile navigation menus.
 * @param {!Element} navPanelEl the nav panel element created in C++.
 * @param {!goog.color.Rgb} backgroundColor Color to style the buttons in the
 *     menu.
 * @constructor
 */
mob.NavPanel = function(navPanelEl, backgroundColor) {
  /**
   * Nav panel element. Inserted by C++.
   * @type {?Element}
   */
  this.el = navPanelEl;

  /**
   * @private {!goog.color.Rgb}
   */
  this.backgroundColor_ = backgroundColor;

  /**
   * Click detector div.
   * @private {?Element}
   */
  this.clickDetectorDiv_ = null;

  /**
   * The y coordinate of the previous move event, used to determine the
   * direction of scrolling.
   * @private {number}
   */
  this.lastScrollY_ = 0;

  this.initialize_();
};


/**
 * The width of the nav panel in CSS pixels. Must match the value in
 * mobilize.css.
 * @private {number}
 * @const
 */
mob.NavPanel.WIDTH_ = 350;


/**
 * GIF image of an arrow icon, used to indicate hierarchical menus.
 * @private @const {string}
 */
mob.NavPanel.ARROW_ICON_ =
    'R0lGODlhkACQAPABAP///wAAACH5BAEAAAEALAAAAACQAJAAAAL+jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8YhMKpfMpvMJjUqn1Kr1is1qt9yu9wsO' +
    'i8fksvmMTqvX7Lb7DY/L5/S6/Y7P6/f8vh8EAJATKIhFWFhziEiluBjT6AgFGdkySclkeZmS' +
    'qYnE2VnyCUokOhpSagqEmtqxytrjurnqFGtSSztLcvu0+9HLm+sbPPWbURx1XJGMPHyxLPXs' +
    'EA3dLDFNXP1wzZjNsF01/W31LH6VXG6YjZ7Vu651674VG8/l2s1mL2qXn4nHD6nn3yE+Al+5' +
    '+fcnQL6EBui1QcUwgb6IEvtRVGDporc/RhobKOooLRBIbSNLmjyJMqXKlSxbunwJM6bMmTRr' +
    '2ryJM6fOnTx7+vwJNKjQoUSLGj2KNKnSpUybOn0KVUcBADs=';


/**
 * Style the nav panel (which has been inserted server side), and register event
 * handlers for it.
 * @private
 */
mob.NavPanel.prototype.initialize_ = function() {
  // Make sure the navPanel is in the body of the document; we've seen it
  // moved elsewhere by JS on the page.
  document.body.appendChild(this.el);

  // If the navpanel's outer ul has the class 'open', replace it with
  // mob.util.ElementClass.OPEN (for compatibility with older releases
  // of the filter).
  // TODO(jmaessen): Remove after older releases are no longer running, should
  // be doable by early August.
  var opens = this.el.getElementsByClassName('open');
  for (var i = 0, n = opens.length; i < n; i++) {
    goog.dom.classlist.addRemove(opens[i], 'open', mob.util.ElementClass.OPEN);
  }

  this.addSubmenuArrows_();
  this.addClickDetectorDiv_();
  this.addButtonEvents_();

  // Track touch move events just in the nav panel so that scrolling can be
  // controlled. This is to work around overflow: hidden not working as we would
  // want when zoomed in (it does not totally prevent scrolling).
  this.el.addEventListener(
      goog.events.EventType.TOUCHMOVE, goog.bind(function(e) {
        if (!this.isOpen()) {
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
          var navPanelAtTop = (this.el.scrollTop == 0);
          // Add 1 pixel to account for rounding errors.
          var navPanelAtBottom =
              (this.el.scrollTop >=
               (this.el.scrollHeight - this.el.offsetHeight - 1));

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

  window.addEventListener(goog.events.EventType.SCROLL, goog.bind(function(e) {
    if (this.isOpen() && !this.el.contains(/** @type {?Node} */ (e.target))) {
      e.stopPropagation();
      e.preventDefault();
    }
  }, this));

  window.addEventListener(goog.events.EventType.TOUCHSTART,
                          goog.bind(function(e) {
                            this.lastScrollY_ = e.touches[0].clientY;
                          }, this), false);
};


/**
 * Redraw the nav panel based on current zoom level.
 * @param {number=} opt_marginTopHeight The amount to offset the nav panel
 * (normally the height of the header bar).
 */
mob.NavPanel.prototype.redraw = function(opt_marginTopHeight) {
  var fontSize = mob.util.getZoomLevel();

  // Make sure that the nav panel does not overflow the window on small screen
  // devices by capping the maximum font size.
  var bodyWidth =
      mob.util.pixelValue(window.getComputedStyle(document.body).width);
  if (bodyWidth) {
    fontSize = Math.min(fontSize, bodyWidth / mob.NavPanel.WIDTH_);
  }
  this.el.style.fontSize = fontSize + 'px';

  var xOffset = this.isOpen() ?
                    0 :
                    (-goog.style.getTransformedSize(this.el).width);

  this.el.style.top = window.scrollY + 'px';
  this.el.style.left = window.scrollX + xOffset + 'px';

  if (opt_marginTopHeight) {
    this.el.style.marginTop = opt_marginTopHeight + 'px';

    this.el.style.height =
        ((window.innerHeight - opt_marginTopHeight) / fontSize) + 'em';
  }

};


/**
 * Add a div for detecting clicks on the body in order to close the open nav
 * panel. This is to workaround JS that sends click events, which can be
 * difficult to differentiate from actual clicks from the user. In particular
 * https://github.com/DevinWalker/jflow/ causes this issue.
 * @private
 */
mob.NavPanel.prototype.addClickDetectorDiv_ = function() {
  this.clickDetectorDiv_ = document.createElement(goog.dom.TagName.DIV);
  this.clickDetectorDiv_.id = mob.util.ElementId.CLICK_DETECTOR_DIV;
  document.body.insertBefore(this.clickDetectorDiv_, this.el);

  this.clickDetectorDiv_.addEventListener(
      goog.events.EventType.CLICK, goog.bind(function(e) {
        if (this.isOpen()) {
          this.toggle();
        }
      }, this), false);
};


/**
 * Add properly themed arrow icons to the submenus of the nav menu.
 * @private
 */
mob.NavPanel.prototype.addSubmenuArrows_ = function() {
  var submenuTitleAs = this.el.querySelectorAll(goog.dom.TagName.DIV + ' > ' +
                                                goog.dom.TagName.A);
  var n = submenuTitleAs.length;
  if (n == 0) {
    return;
  }

  var arrowIcon =
      mob.util.synthesizeImage(mob.NavPanel.ARROW_ICON_, this.backgroundColor_);

  for (var i = 0; i < n; i++) {
    var icon = document.createElement(goog.dom.TagName.IMG);
    var submenu = submenuTitleAs[i];
    submenu.insertBefore(icon, submenu.firstChild);
    icon.setAttribute('src', arrowIcon);
    goog.dom.classlist.add(icon, mob.util.ElementClass.MENU_EXPAND_ICON);
  }
};


/**
 * Event handler for clicks on the hamburger menu. Toggles the state of the nav
 * panel so that is opens/closes.
 */
mob.NavPanel.prototype.toggle = function() {
  // We used to make a bunch of toggle calls here, but we
  // now use isOpen() as the source of truth and adjust the CSS
  // based upon it.
  var event = (this.isOpen() ? mob.util.BeaconEvents.MENU_BUTTON_CLOSE :
                               mob.util.BeaconEvents.MENU_BUTTON_OPEN);
  var action =
      (this.isOpen() ? goog.dom.classlist.remove : goog.dom.classlist.add);
  mob.util.sendBeaconEvent(event);
  action(this.el, mob.util.ElementClass.OPEN);
  action(this.clickDetectorDiv_, mob.util.ElementClass.OPEN);
  action(document.body, mob.util.ElementClass.NOSCROLL);
  this.redraw();
};


/**
 * Return destination url for onclick event handler for the given url if it
 * needs one, or null if it does not.  If the url points to
 * e.g. example-com.pagespeedmobilizer.com/foo (document.domain), we want to
 * navigate the iframe to example.com/foo (destDomain) instead.  Why not have
 * the link just point to example.com/foo ?  Because then if the JS doesn't run
 * for some reason the navigation that occurs is wrong (we'd want to have the
 * whole document navigate to the mobilized page).  Nonetheless, we try to deal
 * with that case gracefully.
 * Exposed for testability.
 * @param {string} href url of <a> href destination.
 * @param {string} destDomain destination domain for iframe links that point to
 *     document.domain.
 * @return {?string} destination url, or null if no event handler is required.
 */
mob.NavPanel.prototype.getMenuClickHandlerUrl = function(href, destDomain) {
  var dest = new goog.Uri(href);
  if (dest.getDomain() == document.domain ||
      dest.getDomain() == destDomain) {
    dest.setDomain(destDomain);
    return dest.toString();
  } else {
    return null;
  }
};


/**
 * Close all submenus of the main navigation menu.
 * @private
 */
mob.NavPanel.prototype.closeAllSubmenus_ = function() {
  var openSubmenus = this.el.querySelectorAll(goog.dom.TagName.LI + ' .' +
                                              mob.util.ElementClass.OPEN);
  for (var i = 0, element; element = openSubmenus[i]; ++i) {
    goog.dom.classlist.remove(element, mob.util.ElementClass.OPEN);
  }
};


/**
 * Add events to the buttons in the nav panel.
 * @private
 */
mob.NavPanel.prototype.addButtonEvents_ = function() {
  var navDivs = this.el.querySelectorAll(goog.dom.TagName.DIV);
  for (var i = 0, div; div = navDivs[i]; ++i) {
    div.addEventListener(goog.events.EventType.CLICK, function(e) {
      // These divs have href='#' (set by the server), so prevent default to
      // keep it from changing the URL.
      e.preventDefault();
      var target = e.currentTarget;
      // A click was registered on the div that has the hierarchical menu text
      // and icon. Open up the UL, which should be the next element.  Here we
      // use the open state of target.nextSibling as a source of truth.
      var isSubmenuOpen = goog.dom.classlist.contains(
          target.nextSibling, mob.util.ElementClass.OPEN);
      var event = (isSubmenuOpen ? mob.util.BeaconEvents.SUBMENU_CLOSE :
                                   mob.util.BeaconEvents.SUBMENU_OPEN);
      var action =
          (isSubmenuOpen ? goog.dom.classlist.remove : goog.dom.classlist.add);
      mob.util.sendBeaconEvent(event);
      action(target.nextSibling, mob.util.ElementClass.OPEN);
      // Also toggle the expand icon, which will be the first child of the P
      // tag, which is the first child of the target div.
      action(target.firstChild.firstChild, mob.util.ElementClass.OPEN);
    }, false);
  }

  // Setup the buttons in the nav panel so that they navigate the iframe instead
  // of the top level page.
  var iframe = document.getElementById(mob.util.ElementId.IFRAME);
  if (iframe) {
    var destDomain = (new goog.Uri(iframe.src)).getDomain();
    var aTags = this.el.querySelectorAll(goog.dom.TagName.LI + ' > ' +
                                         goog.dom.TagName.A);
    for (var i = 0, aTag; aTag = aTags[i]; i++) {
      var url = this.getMenuClickHandlerUrl(aTag.href, destDomain);
      if (url) {
        aTag.addEventListener(
            goog.events.EventType.CLICK,
            goog.bind(function(iframe, url, event) {
              mob.util.sendBeaconEvent(mob.util.BeaconEvents.MENU_NAV_CLICK);
              event.preventDefault();
              iframe.src = url;
              this.toggle();
              this.closeAllSubmenus_();
            }, this, iframe, url));
      } else {
        aTag.addEventListener(goog.events.EventType.CLICK, function() {
          mob.util.sendBeaconEvent(mob.util.BeaconEvents.MENU_NAV_CLICK);
        });
      }
    }
  }
};


/**
 * Returns true if the nav panel is open and visible.
 * @return {boolean}
 */
mob.NavPanel.prototype.isOpen = function() {
  return goog.dom.classlist.contains(this.el, mob.util.ElementClass.OPEN);
};
