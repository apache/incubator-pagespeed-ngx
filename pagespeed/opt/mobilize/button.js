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
 *
 * Author: jmarantz@google.com (Joshua Marantz)
 */

// TODO(jud): These should be split up into a different files and in a 'button'
// subdirectory.
goog.provide('mob.button.Dialer');
goog.provide('mob.button.Map');
goog.provide('mob.button.Menu');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.json');
goog.require('goog.net.Cookies');  // abstraction gzipped size cost: 336 bytes.
goog.require('goog.net.Jsonp');
goog.require('pagespeed.MobUtil');



/**
 * Base class for buttons.
 * @param {!pagespeed.MobUtil.ElementId} id
 * @param {string} iconImage base64 encoded image.
 * @param {!goog.color.Rgb} color for the image icon.
 * @param {?string} labelText Optional text for label.
 * @constructor @private
 */
mob.button.Base_ = function(id, iconImage, color, labelText) {
  /**
   * Top level element.
   * @type {!Element}
   */
  this.el = document.createElement(goog.dom.TagName.A);

  /**
   * @private {!pagespeed.MobUtil.ElementId}
   */
  this.id_ = id;

  /**
   * Base64 encoded gif image.
   * @private {string}
   */
  this.iconImage_ = iconImage;

  /**
   * Color to use for the image.
   * @private {!goog.color.Rgb}
   */
  this.color_ = color;

  /**
   * Text to add next to the button. Can be null if no text should be displayed.
   * @private {?string}
   */
  this.labelText_ = labelText;

  this.createButton();
};


/**
 * Initialize the button. If overridden by a subclass, this should still be
 * called.
 * @protected
 */
mob.button.Base_.prototype.createButton = function() {
  this.el.id = this.id_;

  goog.dom.classlist.add(this.el, pagespeed.MobUtil.ElementClass.BUTTON);
  this.el.onclick = goog.bind(this.clickHandler, this);

  var icon = document.createElement(goog.dom.TagName.DIV);
  goog.dom.classlist.add(icon, pagespeed.MobUtil.ElementClass.BUTTON_ICON);
  // We set the image using backgroundImage because if it's a foreground image
  // then dialing fails on a Samsung Galaxy Note 2.
  icon.style.backgroundImage =
      'url(' + pagespeed.MobUtil.synthesizeImage(this.iconImage_, this.color_) +
      ')';
  this.el.appendChild(icon);

  if (this.labelText_) {
    var label = document.createElement(goog.dom.TagName.P);
    goog.dom.classlist.add(label, pagespeed.MobUtil.ElementClass.BUTTON_TEXT);
    this.el.appendChild(label);
    label.appendChild(document.createTextNode(this.labelText_));
  }
};


/**
 * Function to be called when button is clicked.
 * @protected
 */
mob.button.Base_.prototype.clickHandler = goog.abstractMethod;



/**
 * Menu button.
 * @param {!goog.color.Rgb} color
 * @param {!Function} clickHandlerFn
 * @constructor
 * @extends {mob.button.Base_}
 */
mob.button.Menu = function(color, clickHandlerFn) {
  this.clickHandlerFn_ = clickHandlerFn;

  mob.button.Menu.base(this, 'constructor',
                       pagespeed.MobUtil.ElementId.MENU_BUTTON,
                       mob.button.Menu.ICON_, color, null);
};
goog.inherits(mob.button.Menu, mob.button.Base_);


/**
 * GIF icon for the menu button.
 * https://www.gstatic.com/images/icons/material/system/2x/menu_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Menu.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEALAAAAABgAGAAAAK6jI+py+0Po5y02ouz3rz7' +
    'D4biSJbmiabqyrbuC8fyTNf2jef6zvf+DwwKh8Si8agCKJfMpvMJjUqnTg71is1qldat97vt' +
    'gsfkp7iMJp/T7PCmDXdr4vQr8o7P6/f8vv8PGCg4SHhTdxi1hriouHjY6EgHGQk3SclmeYmW' +
    'qalW+AkaKjpKWmp6ipqqatH5+NYq+QpbKTuLWWu7iZvrOcebxvlrt0pcbHyMnKy8zNzs/Awd' +
    'LT1NXW19TVoAADs=';


/** @override */
mob.button.Menu.prototype.clickHandler = function() {
  this.clickHandlerFn_();
};



/**
 * Map button.
 * @param {!goog.color.Rgb} color
 * @param {string} mapLocation
 * @param {?string} conversionId
 * @param {?string} conversionLabel
 * @constructor
 * @extends {mob.button.Base_}
 */
mob.button.Map = function(color, mapLocation, conversionId, conversionLabel) {
  /**
   * Map location to navigate to when the button is clicked. Passed to the "q"
   * query param of the google maps url.
   * @private {string}
   */
  this.mapLocation_ = mapLocation;


  /**
   * Adwords conversion ID.
   * @private {?string}
   */
  this.conversionId_ = conversionId;

  /**
   * Adwords map conversion label.
   * @private {?string}
   */
  this.conversionLabel_ = conversionLabel;

  mob.button.Map.base(this, 'constructor',
                      pagespeed.MobUtil.ElementId.MAP_BUTTON,
                      mob.button.Map.ICON_, color, mob.button.Map.LABEL_);
};
goog.inherits(mob.button.Map, mob.button.Base_);


/**
 * GIF icon for the map button.
 * https://www.gstatic.com/images/icons/material/system/2x/place_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Map.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAv6Mj6nL7Q+jnLTai7PevPsPhuJIluaJpuo6Au4Ls3IA1/Y7l/fO5x8P7Pk0weJt' +
    'eDEqj0jJ8llrQqBUnJRRzQKuCq2We/B6wWIxtzyWos3IdbntTufi7zldLmMj9Cx895tX5SQY' +
    'CEVB2Ed1qJhoWIGYAjnoqMJoYRlJ+aiJgrn41LiE4XlCOim6YhqhSiK5ytoCiyUr4upgW0q7' +
    'pxuSdQpaiGvgV0nMF3rHW5vs68OsHPuM6iw9TV0dpIadrb3N1O0ddRYeQ0ZuZU4OFna+zu7t' +
    '/r4dLy9NX/98j3+nv8vc7y8ZwIBxBhJ0Y/BgnYT7ADFs2OwhRE4SaRyrOMwhRjeF1jZOBOJx' +
    'FrCQf0aSTGDyJEojKm9xa9kAJEyXO2Y+qGnzpricOl3wHPQzqNChRIsaPYo0acUCADs=';


/**
 * The text to insert next to the map button.
 * @private @const {string}
 */
mob.button.Map.LABEL_ = 'GET DIRECTIONS';


/** @override */
mob.button.Map.prototype.clickHandler = function() {
  pagespeed.MobUtil.sendBeaconEvent(pagespeed.MobUtil.BeaconEvents.MAP_BUTTON,
                                    goog.bind(this.openMap_, this));
};


/**
 * Gets a map URL based on the location.
 * @private
 * @return {string}
 */
mob.button.Map.prototype.getMapUrl_ = function() {
  // TODO(jmarantz): test/fix this in iOS safari/chrome with/without
  // Google Maps installed.  Probably use 'http://maps.apple.com/?='
  //
  // I don't know the best way to do this but I asked the question on
  // Stack Overflow: http://goo.gl/0g8kEV
  //
  // Other links to explore:
  // https://developer.apple.com/library/iad/featuredarticles/iPhoneURLScheme_Reference/MapLinks/MapLinks.html
  // https://developers.google.com/maps/documentation/ios/urlscheme
  // http://stackoverflow.com/questions/17915901/is-there-an-android-equivalent-to-google-maps-url-scheme-for-ios
  var mapUrl =
      'https://maps.google.com/maps?q=' + encodeURIComponent(this.mapLocation_);
  return mapUrl;
};


/**
 * Loads a tracking pixel that triggers a conversion event if the conversion
 * label is set, and then navigates to a map.  Note that we navigate to the map
 * whether the conversion succeeds or fails.
 * @private
 */
mob.button.Map.prototype.openMap_ = function() {
  // We use goog.global here so that we can override it in tests to prevent the
  // page from actually navigating.
  if (!this.conversionId_ || !this.conversionLabel_) {
    // No conversion id or label specified; go straight to the map.
    goog.global.location = this.getMapUrl_();
    return;
  }

  // We will visit the map only after we get the onload/onerror for the
  // tracking pixel.
  var trackingPixel = new Image();
  trackingPixel.onload =
      goog.bind(function() { goog.global.location = this.getMapUrl_(); }, this);

  // The user comes first so he gets to the map even if we can't track it.
  trackingPixel.onerror = trackingPixel.onload;

  // With the handlers set up, we can load the magic pixel to track the
  // conversion.
  //
  // TODO(jmarantz): Is there a better API to report a conversion?  Should
  // we really use script=0, since this is obviously a script?  In any case
  // we should use <a ping> when available.
  trackingPixel.src = '//www.googleadservices.com/pagead/conversion/' +
                      this.conversionId_ + '/?label=' + this.conversionLabel_ +
                      '&amp;guid=ON&amp;script=0';
};



/**
 * Creates a phone dialer.
 *
 * This constructor has just one non-test call-site which is called with
 * data populated from our C++ server via externs.  The phone information
 * may not be present in the configuration for the site, in which case the
 * variables will be null, but they will always be passed in.
 *
 * @param {!goog.color.Rgb} color
 * @param {string} phoneNumber
 * @param {?string} conversionId
 * @param {?string} conversionLabel
 * @constructor
 * @extends {mob.button.Base_}
 */
mob.button.Dialer = function(color, phoneNumber, conversionId,
                             conversionLabel) {
  /**
   * The fallback phone number to be used if no google voice number from adwords
   * is generated.
   * @private {string}
   */
  this.fallbackPhoneNumber_ = phoneNumber;


  /**
   * The google voice number either requested from adwords, or set in a cookie
   * from a previous request.
   * @private {?string}
   */
  this.googleVoicePhoneNumber_ = null;

  /**
   * Adwords conversion ID.
   * @private {?string}
   */
  this.conversionId_ = conversionId;

  /**
   * Adwords phone conversion label.
   * @private {?string}
   */
  this.conversionLabel_ = conversionLabel;

  /** @private {!goog.net.Cookies} */
  this.cookies_ = new goog.net.Cookies(document);

  /**
   * Track how long the request to get the conversion number takes.
   * @private {?number}
   */
  this.jsonpTime_ = null;

  mob.button.Dialer.base(
      this, 'constructor', pagespeed.MobUtil.ElementId.DIALER_BUTTON,
      mob.button.Dialer.ICON_, color, mob.button.Dialer.LABEL_);

};
goog.inherits(mob.button.Dialer, mob.button.Base_);


/**
 * GIF image of dial button. To view this image, add prefix of
 * 'data:image/gif;base64,' to it.
 * https://www.gstatic.com/images/icons/material/system/2x/call_black_48dp.png
 * Generated with 'curl $url | convert - gif:- | base64'
 * @const {string}
 * @private
 */
mob.button.Dialer.ICON_ =
    'R0lGODlhYABgAPAAAAAAAAAAACH5BAEAAAEAIf8LSW1hZ2VNYWdpY2sHZ2FtbWE9MQAsAAAA' +
    'AGAAYAAAAv6Mj6nL7Q+jnLTai7PevPsPhuJIluaJpurKtu4Lx/KsAvaN5zcd6r7P8/yGuSCH' +
    'iAQYN0niUtMcPjPR3xRTBV4tWd2W28V9K2HxeFI2nyNp21rSVr4h8Tm9bX/E5fnFvt9QB+iH' +
    'N6ggaIiAmHiQxngY9giZJTkZVUnYhJmJtMmQ5PnpFMrpRSqqdWpZpFqq1tpoBZsgNRsraxtQ' +
    'mzvKu2v7O9sLHAw7bHzc2ulbrLpM3IzGegV6UW10Le0Ik3232KLp/cfdvfoNHm5+7pJ+u7dD' +
    'c2nwbhpPv81zX/akT8nf324JwGdTBkbDZnDamITw3jDsY9BQP0b3MK171IVZqhBcHDt6/Agy' +
    'pMiRJEuaDFUAADs=';


/**
 * The text to insert next to the dial button.
 * @private @const {string}
 */
mob.button.Dialer.LABEL_ = 'CALL US';


/**
 * Cookie to use to store phone dialer information to reduce query volume
 * to googleadservices.com.
 * @private @const {string}
 */
mob.button.Dialer.WCM_COOKIE_ = 'psgwcm';


/**
 * Cookie lifetime in seconds.
 * @private @const {number}
 */
mob.button.Dialer.WCM_COOKIE_LIFETIME_SEC_ = 3600 * 24 * 90;


/**
 * Prefix for constructing URLs for generating Google Voice numbers used to
 * help track durations of phone calls in responses to phone-dialer clicks.
 * @private @const {string}
 */
mob.button.Dialer.CONVERSION_HANDLER_ =
    'https://www.googleadservices.com/pagead/conversion/';


/** @override */
mob.button.Dialer.prototype.createButton = function() {
  mob.button.Dialer.base(this, 'createButton');
  this.googleVoicePhoneNumber_ = this.getPhoneNumberFromCookie_();
};


/** @override */
mob.button.Dialer.prototype.clickHandler = function(e) {
  pagespeed.MobUtil.sendBeaconEvent(
      pagespeed.MobUtil.BeaconEvents.PHONE_BUTTON);
  if (this.googleVoicePhoneNumber_) {
    this.dialPhone_();
  } else {
    this.requestPhoneNumberAndDial_();
  }
};


/**
 * Obtains a dynamic Google-Voice number to track conversions.  We only
 * do this when user clicks the phone icon.
 *
 * @private
 */
mob.button.Dialer.prototype.requestPhoneNumberAndDial_ = function() {
  var url = this.constructRequestPhoneNumberUrl_();
  if (url) {
    this.debugAlert_('requesting dynamic phone number: ' + url);
    var req = new goog.net.Jsonp(url);
    this.jsonpTime_ = Date.now();
    req.send(null, goog.bind(this.receivePhoneNumber_, this, true),
             goog.bind(this.receivePhoneNumber_, this, false));
  } else {
    this.dialPhone_();
  }
};


/**
 * Constructs a URL to request a Google Voice phone number to track
 * call-conversions.  Returns null on failure.
 *
 * @return {?string}
 * @private
 */
mob.button.Dialer.prototype.constructRequestPhoneNumberUrl_ = function() {
  // The protocol for requesting the gvoice number from googleadservices
  // requires that we pass the fallback phone number into the URL.  So
  // to request a gvoice number we must have a fallback phone number already.
  if (this.conversionLabel_ && this.conversionId_) {
    var label = window.encodeURIComponent(this.conversionLabel_);
    return mob.button.Dialer.CONVERSION_HANDLER_ +
           window.encodeURIComponent(this.conversionId_) + '/wcm?cl=' + label +
           '&fb=' + window.encodeURIComponent(this.fallbackPhoneNumber_);
  }
  return null;
};


/**
 * Dials a phone number.
 *
 * @private
 */
mob.button.Dialer.prototype.dialPhone_ = function() {
  var phoneNumber;
  var ev;
  // Always prefer the google voice number if it is available, otherwise use the
  // fallback number.
  if (this.googleVoicePhoneNumber_) {
    phoneNumber = this.googleVoicePhoneNumber_;
    ev = pagespeed.MobUtil.BeaconEvents.CALL_GV_NUMBER;
  } else {
    phoneNumber = this.fallbackPhoneNumber_;
    ev = pagespeed.MobUtil.BeaconEvents.CALL_FALLBACK_NUMBER;
  }
  pagespeed.MobUtil.sendBeaconEvent(ev, function() {
    goog.global.location = 'tel:' + phoneNumber;
  });
};


/**
 * Extracts a dynamic phone number from a jsonp response.  If successful, it
 * dials the phone, and saves the phone number in a cookie and also in
 * this.googleVoicePhoneNumber_.
 *
 * @private
 * @param {boolean} success True if the jsonp request succeeded.
 * @param {?Object} json
 */
mob.button.Dialer.prototype.receivePhoneNumber_ = function(success, json) {
  var responseTime = Date.now() - this.jsonpTime_;
  pagespeed.MobUtil.sendBeaconEvent(
      pagespeed.MobUtil.BeaconEvents.CALL_CONVERSION_RESPONSE, null,
      '&s=' + success.toString() + '&t=' + responseTime);
  var wcm = json && json['wcm'];
  var phoneNumber = wcm && wcm['mobile_number'];
  if (phoneNumber) {
    // Save the phoneNumber in a cookie to reduce server requests.
    // TODO(jud): Use localstorage instead of a cookie.
    var cookieValue = {
      'expires': wcm['expires'],
      'formatted_number': wcm['formatted_number'],
      'mobile_number': phoneNumber,
      'clabel': this.conversionLabel_,
      'fallback': this.fallbackPhoneNumber_
    };
    cookieValue = goog.json.serialize(cookieValue);
    this.debugAlert_('saving phoneNumber in cookie: ' + cookieValue);
    this.cookies_.set(mob.button.Dialer.WCM_COOKIE_,
                      window.encodeURIComponent(cookieValue),
                      mob.button.Dialer.WCM_COOKIE_LIFETIME_SEC_, '/');

    this.googleVoicePhoneNumber_ = phoneNumber;
  }
  this.dialPhone_();
};


/**
 * Attempts to get a static phone number, either as a debug
 * fallback or from a cookie, returning null if we don't have
 * the right phone number available.
 *
 * @private
 * @return {?string}
 */
mob.button.Dialer.prototype.getPhoneNumberFromCookie_ = function() {
  // Check to see if the phone number we want was previously saved
  // in a valid cookie, with matching fallback number and conversion label.
  var gwcmCookie = this.cookies_.get(mob.button.Dialer.WCM_COOKIE_);
  if (gwcmCookie) {
    var cookieData = goog.json.parse(window.decodeURIComponent(gwcmCookie));
    if ((cookieData['fallback'] == this.fallbackPhoneNumber_) &&
        (cookieData['clabel'] == this.conversionLabel_)) {
      return cookieData['mobile_number'];
    }
  }
  return null;
};


/**
 * Pops up an alert if the page is viewed in debug mode by requesting it
 * with ?PageSpeedFilters=+debug.
 *
 * We debug with alert() here because it is hard to debug on the
 * physical phone with console.log.  And while most of our code
 * can be debugged on the chrome emulator, this code only works
 * on actual phones.
 *
 * @param {string} message
 *
 * @private
 */
mob.button.Dialer.prototype.debugAlert_ = function(message) {
  if (window.psDebugMode) {
    window.alert(message);
  }
};
