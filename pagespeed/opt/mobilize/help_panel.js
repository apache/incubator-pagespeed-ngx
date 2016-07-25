goog.provide('mob.HelpPanel');

goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.events.EventType');
goog.require('goog.style');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ElementClass');
goog.require('mob.util.ElementId');



/**
 * Create mobile help menu.
 * @param {string} originalUrl the original web site URL.
 * @constructor
 */
mob.HelpPanel = function(originalUrl) {
  /**
   * Help panel element.
   */
  this.el = goog.dom.createElement(goog.dom.TagName.A);
  this.el.id = mob.util.ElementId.HELP_PANEL;

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

  this.originalUrl_ = originalUrl;

  this.initialize_();
};


/**
 * The width of the help panel in CSS pixels. Must match the value in
 * mobilize.css.
 * @private @const {number}
 */
mob.HelpPanel.WIDTH_ = 270;


/**
 * GIF icon for the original website menu.
 * https://www.gstatic.com/images/icons/material/system/2x/open_in_new_grey600_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @private @const {string}
 */
mob.HelpPanel.VIEW_ORIGINAL_SITE_ICON_ =
    'R0lGODlhYABgAPAAAHV1dQAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAv6Mj6nL7Q+jnLTai7PevPsPhuJIluaJpurKtu4Lx/LcAvaN5/p+M/wPBHyCRKCv' +
    'iPQgl7kj08h5Sp3SXafKpGJx0W1y4dV1w0FteEz+mb1oLi3w1fDey3Zvlt3MZU+7jd+nZwVT' +
    '5Sf0UrYnN1gDpZjxqFJmEHlReTJJyQi5iZKpKSYYmvIJ2iR66gmVcFnRGlJ68Doxq7SqUBuR' +
    'i9rJ2muxy6nmEEz8aztsPCqcCnvbUAx9zLus3IwRLRHbWG14B9i9mAx+LZ5dcr7mRjdNsR2T' +
    '7jvOHu7+TF9uPw/crt1v/Q9CPATx3rkK+CCdwYP19OVLeI/ZOonf/O0zNxEbQqhTGy0+dJgR' +
    'YEOKf6iVFBjRkEqUF690VMdyYMyQ/EbCSYnsI61/LYe8BBPwZ02dHmkCrYhOqDyiBE9iUtrU' +
    'JqsVMjkifWPVqUaphKDK8mqi6k2uiMCOZdqVrEitWM8aBXkVq1ixJOaarWuW7gi7armhhZgm' +
    'cCGTggvj3Gk48ULEihu/Zeg4ss/IjZ1RNoz3Mpm2nDt7/gw6tOjRpEubPo06terVrFuHKAAA' +
    'Ow==';


/**
 * GIF icon for the contact bar by google menu.
 * https://www.gstatic.com/images/icons/material/system/2x/open_in_new_grey600_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @private @const {string}
 */
mob.HelpPanel.CONTACT_BAR_BY_GOOGLE_ICON_ =
    'R0lGODlhYABgAPAAAHV1dQAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAv6Mj6nL7Q+jnLTai7PevPsPhuJIluaJpuoqAu4Lu+wa1zY8t/fO5xwPBPouwaJw' +
    'GDEqj8jF8slsGqDUnTRQzVqH2q7N5w3jZuIygGwOo9PdNTvrflNz6mSR3q708PDMF1zlMcYH' +
    'BSLDVXj1Mae4+NTouAQZqTTZkWi58Zj5I8mpsfnp5ymKQVpKVIlqqjoqNzgRyvr6YnE6S3um' +
    '1+qaa8uLS/tr1JmrS3Gbajx8p2l8HAus7LtL7Lxc3Qz6zBxUTI2cnC3cHTVNPm5eLneuTiNt' +
    'B58iDiH7bt3ufkIfj69iH47fCIAB5ekwKAHTPoEJCYZQGMzfQYQFKfay2JARJVKM0TR+Y5jO' +
    'YUeIl/KEBJmPJIM6E5+ZbOnS48CYgVDQVEniJseNOv/E6QnrJ9ArQGspKmrppihuq16tSpDm' +
    'aYOXUut5q4o1q9atXLt6/Qo27KoCADs=';


/**
 * The text of first menu item.
 * @private @const {string}
 */
mob.HelpPanel.VIEW_ORIGINAL_SITE_TEXT_ = 'View original site';


/**
 * The text of second menu item.
 * @private @const {string}
 */
mob.HelpPanel.CONTACT_BAR_BY_GOOGLE_TEXT_ = 'Contact bar by Google';


/**
 * The text of the learn more in the menu.
 * @private @const {string}
 */
mob.HelpPanel.LEARN_MORE_TEXT_ = 'Learn more';


/**
 * The mobilization help page URL.
 * @private @const {string}
 */
mob.HelpPanel.HELP_PAGE_URL_ = 'https://support.google.com/ads/answer/7016176';


/**
 * Insert a menu item to the menu element.
 * @param {!Element} menuElement
 * @param {string} itemId
 * @param {string} menuText
 * @param {string} iconImage base64 encoded image.
 * @param {string} redirectUrl
 * @param {boolean} forLearnMore indicates the purpose of the redirect URL.
 * @private
 */
mob.HelpPanel.prototype.createMenuItem_ = function(
    menuElement, itemId, menuText, iconImage, redirectUrl, forLearnMore) {

  var li = goog.dom.createElement(goog.dom.TagName.LI);
  menuElement.appendChild(li);
  li.id = itemId;
  var menuItem = li;
  if (!forLearnMore) {
    var a = goog.dom.createElement(goog.dom.TagName.A);
    a.href = redirectUrl;
    li.appendChild(a);
    menuItem = a;
  }

  var icon = goog.dom.createElement(goog.dom.TagName.SPAN);
  // We set the image using backgroundImage because if it's a foreground image
  // then dialing fails on a Samsung Galaxy Note 2.
  var imageUrl = 'data:image/gif;base64,' + iconImage;
  icon.style.backgroundImage = 'url(' + imageUrl + ')';
  menuItem.appendChild(icon);
  var label = goog.dom.createElement(goog.dom.TagName.P);
  menuItem.appendChild(label);

  if (forLearnMore) {
    goog.dom.classlist.add(label, mob.util.ElementClass.LEARN_MORE);
    var labelText = goog.dom.createElement(goog.dom.TagName.P);
    label.appendChild(labelText);
    goog.dom.classlist.add(labelText, mob.util.ElementClass.LEARN_MORE_TEXT);
    labelText.appendChild(document.createTextNode(menuText));
    var a = goog.dom.createElement(goog.dom.TagName.A);
    a.href = redirectUrl;
    label.appendChild(a);
    goog.dom.classlist.add(a, mob.util.ElementClass.LEARN_MORE_LINK);
    a.appendChild(document.createTextNode(mob.HelpPanel.LEARN_MORE_TEXT_));
  } else {
    label.appendChild(document.createTextNode(menuText));
  }
};


/**
 * Style the help panel, and register event handlers for it.
 * @private
 */
mob.HelpPanel.prototype.initialize_ = function() {
  document.body.appendChild(this.el);

  // Add menu items.
  var ul = goog.dom.createElement(goog.dom.TagName.UL);
  this.el.appendChild(ul);

  // Create the first menu item to view original site.
  this.createMenuItem_(
      ul, 'psmob-help-panel-0', mob.HelpPanel.VIEW_ORIGINAL_SITE_TEXT_,
      mob.HelpPanel.VIEW_ORIGINAL_SITE_ICON_, this.originalUrl_, false);

  // Create the second menu time to indicate the contact bar by google.
  this.createMenuItem_(
      ul, 'psmob-help-panel-1', mob.HelpPanel.CONTACT_BAR_BY_GOOGLE_TEXT_,
      mob.HelpPanel.CONTACT_BAR_BY_GOOGLE_ICON_, mob.HelpPanel.HELP_PAGE_URL_,
      true);

  this.addClickDetectorDiv_();
  this.addButtonEvents_();

  // Track touch move events just in the help panel so that scrolling can be
  // controlled. This is to work around overflow: hidden not working as we would
  // want when zoomed in (it does not totally prevent scrolling).
  this.el.addEventListener(
      goog.events.EventType.TOUCHMOVE, goog.bind(function(e) {
        if (!this.isOpen()) {
          return;
        }

        var currentY = e.touches[0].clientY;
        // If the event is not scrolling (pinch zoom for example), then prevent
        // it while the help panel is open.
        if (e.touches.length != 1) {
          e.preventDefault();
        } else {
          // Check if we are scrolling horizontally or scrolling up past the top
          // or below the bottom. If so, stop the scroll event from happening
          // since otherwise the body behind the help panel will also scroll.
          var scrollUp = currentY > this.lastScrollY_;
          var helpPanelAtTop = (this.el.scrollTop == 0);
          // Add 1 pixel to account for rounding errors.
          var helpPanelAtBottom =
              (this.el.scrollTop >=
               (this.el.scrollHeight - this.el.offsetHeight - 1));

          if (e.cancelable && ((scrollUp && helpPanelAtTop) ||
                               (!scrollUp && helpPanelAtBottom))) {
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

  window.addEventListener(
      goog.events.EventType.TOUCHSTART, goog.bind(function(e) {
        this.lastScrollY_ = e.touches[0].clientY;
      }, this), false);
};


/**
 * Redraw the help panel based on current zoom level.
 */
mob.HelpPanel.prototype.redraw = function() {
  var fontSize = mob.util.getZoomLevel();

  // Make sure that the help panel does not overflow the window on small screen
  // devices by capping the maximum font size.
  var bodyWidth =
      mob.util.pixelValue(window.getComputedStyle(document.body).width);
  if (bodyWidth) {
    fontSize = Math.min(fontSize, bodyWidth / mob.HelpPanel.WIDTH_);
  }
  this.el.style.fontSize = fontSize + 'px';
  this.el.style.top = window.scrollY + 'px';
  var headerBarWidth = window.innerWidth;
  if (window.getComputedStyle(document.body).getPropertyValue('overflow-y') !=
      'hidden') {
    headerBarWidth -= goog.style.getScrollbarWidth();
  }
  this.el.style.right = bodyWidth - window.scrollX - headerBarWidth +
      10 * fontSize + 'px';
};


/**
 * Add a div for detecting clicks on the body in order to close the open help
 * panel. This is to workaround JS that sends click events, which can be
 * difficult to differentiate from actual clicks from the user. In particular
 * https://github.com/DevinWalker/jflow/ causes this issue.
 * TODO(xiaoyongt): Check if it can be reverted to onClick handler on the above
 * site.
 * @private
 */
mob.HelpPanel.prototype.addClickDetectorDiv_ = function() {
  this.clickDetectorDiv_ = goog.dom.createElement(goog.dom.TagName.DIV);
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
 * Event handler for clicks on the hamburger menu. Toggles the state of the help
 * panel so that is opens/closes.
 */
mob.HelpPanel.prototype.toggle = function() {
  // We used to make a bunch of toggle calls here, but we
  // now use isOpen() as the source of truth and adjust the CSS
  // based upon it.
  var event =
      (this.isOpen() ? mob.util.BeaconEvents.MENU_BUTTON_CLOSE :
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
 * Add events to the buttons in the help panel.
 * @private
 */
mob.HelpPanel.prototype.addButtonEvents_ = function() {
  // Setup the buttons in the panel so that they navigate the top level page.
  var aTags = this.el.querySelectorAll(
      goog.dom.TagName.LI + ' > ' + goog.dom.TagName.A);
  for (var i = 0, aTag; aTag = aTags[i]; i++) {
    // The URLs are absolute.
    var url = aTag.href;
    aTag.addEventListener(
        goog.events.EventType.CLICK, goog.bind(function(url, event) {
          mob.util.sendBeaconEvent(mob.util.BeaconEvents.MENU_NAV_CLICK);
          event.preventDefault();
          this.toggle();
          goog.global.location = url;
        }, this, url));
  }
};


/**
 * Returns true if the help panel is open and visible.
 * @return {boolean}
 */
mob.HelpPanel.prototype.isOpen = function() {
  return goog.dom.classlist.contains(this.el, mob.util.ElementClass.OPEN);
};
