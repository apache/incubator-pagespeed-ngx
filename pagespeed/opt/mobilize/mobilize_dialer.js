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

goog.provide('pagespeed.MobDialer');

goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.json');
goog.require('goog.net.Cookies');  // abstraction gzipped size cost: 336 bytes.
goog.require('goog.net.Jsonp');
goog.require('pagespeed.MobUtil');



/**
 * Creates a phone dialer.
 *
 * TODO(jud): This button has some common behavior with the map button,
 * so they should share helper-methods or a base-class.
 *
 * This constructor has just one non-test call-site which is called with
 * data populated from our C++ server via externs.  The phone information
 * may not be present in the configuration for the site, in which case the
 * variables will be null, but they will always be passed in.
 *
 * @param {?string} phoneNumber
 * @param {?string} conversionId
 * @param {?string} conversionLabel
 * @constructor
 */
pagespeed.MobDialer = function(phoneNumber, conversionId, conversionLabel) {
  /**
   * Dial button in the header bar.
   * @private {?Element}
   */
  this.dialButton_ = null;


  /**
   * Dial button icon.
   * @private {?Element}
   */
  this.dialButtonIcon_ = null;

  /**
   * Phone number to dial.  Note that this will initially be whatever was
   * passed into the constructor, but a dynamic Google-Voice number can be
   * requested from Adwords.
   * @private {?string}
   */
  this.phoneNumber_ = phoneNumber;

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

  /** @private {goog.net.Cookies} */
  this.cookies_ = new goog.net.Cookies(document);
};


/**
 * GIF image of dial button. To view this image, add prefix of
 * 'data:image/gif;base64,' to it.
 * @const {string}
 * @private
 */
pagespeed.MobDialer.DIAL_BUTTON_IMG_BASE64_ =
    'R0lGODlhgACAAPAAAAAAAAAAACH5BAEAAAEALAAAAACAAIAAAAL+jI+pCL0Po5y02vuaBrj7' +
    'D4bMtonmiXrkyqXuC7MsTNefLNv6nuE4D9z5fMFibEg0KkVI5PLZaTah1IlUWs0qrletl9v1' +
    'VsFcMZQMNivRZHWRnXbz4G25jY621/B1vYuf54cCyCZ4QlhoGIIYqKjC2Oh4AZkoaUEZaWmF' +
    '2acpwZnpuQAaKjpCWmbag5qqmsAa53oK6yT7SjtkO4r7o7vLS+K7Cuwg/EtsDIGcrMzLHOH8' +
    'DM0qvUlabY2JXaG9zc3ojYEYLk5IXs53Pgmovo7XfskOTyE//1lv3/yeP53Or0/nH8CAAo/B' +
    'KTjsIMJb/hYewOcwAMSF5iIamEixYcTMihY5bsRY0GNGkP9Ejtx3poUbk0GCrSR5Z8VLmDRy' +
    'qBnXMokYnEJq7WT5J8wXni86ZQF3JJYWpCkILiXKBOUYpouAGqEU1eobSCCwHvXqDmxKrmHF' +
    'PuH07drUbv3UUgHVFtVXuFuijVVLrNjbvLTm8pW79q/bu4LZ7i2M1i9isoEXQz3smObVyBqH' +
    'UlZ483Kpn5qxCOrs+TNonYZG27RkuoSo1HpXj7YFWtjlZJGlId72l9wy3bjmweI3OJ/hkFqB' +
    'O7U4KzTyuDKXaykAADs=';


/**
 * The text to insert next to the dial button.
 * @private @const {string}
 */
pagespeed.MobDialer.DIAL_TEXT_ = 'CALL US';


/**
 * Cookie to use to store phone dialer information to reduce query volume
 * to googleadservices.com.
 * @private @const {string}
 */
pagespeed.MobDialer.WCM_COOKIE_ = 'psgwcm';


/**
 * Cookie lifetime in seconds.
 * @private @const {number}
 */
pagespeed.MobDialer.WCM_COOKIE_LIFETIME_SEC_ = 3600 * 24 * 90;


/**
 * Prefix for constructing URLs for generating Google Voice numbers used to
 * help track durations of phone calls in responses to phone-dialer clicks.
 * @private @const {string}
 */
pagespeed.MobDialer.CONVERSION_HANDLER_ =
    'https://www.googleadservices.com/pagead/conversion/';


/**
 * Creates a phone-dialer button and returns it.  Returns null if there
 * is no fallback phone number passed into the constructor.
 * @return {?Element}
 */
pagespeed.MobDialer.prototype.createButton = function() {
  if (!this.phoneNumber_) {
    return null;
  }
  this.dialButton_ = document.createElement(goog.dom.TagName.A);
  goog.dom.classlist.add(this.dialButton_,
                         pagespeed.MobUtil.ElementClass.BUTTON);

  this.dialButtonIcon_ = document.createElement(goog.dom.TagName.DIV);
  goog.dom.classlist.add(this.dialButtonIcon_,
                         pagespeed.MobUtil.ElementClass.BUTTON_ICON);
  this.dialButton_.appendChild(this.dialButtonIcon_);

  var dialText = document.createElement(goog.dom.TagName.P);
  this.dialButton_.appendChild(dialText);
  goog.dom.classlist.add(dialText, pagespeed.MobUtil.ElementClass.BUTTON_TEXT);
  dialText.appendChild(document.createTextNode(pagespeed.MobDialer.DIAL_TEXT_));
  var phoneNumberFromCookie = this.getPhoneNumberFromCookie_();
  var dialFn;
  if (phoneNumberFromCookie) {
    this.phoneNumber_ = phoneNumberFromCookie;
    dialFn = goog.bind(this.dialPhone_, this);
  } else {
    dialFn = goog.bind(this.requestPhoneNumberAndDial_, this);
  }
  // TODO(jmarantz): rename pagespeed.MobUtil.sendBeacon to sendPing or track,
  // as not to confuse with specific implementation
  // (https://developer.mozilla.org/en-US/docs/Web/API/Navigator/sendBeacon).
  this.dialButton_.onclick =
      goog.partial(pagespeed.MobUtil.sendBeacon,
                   pagespeed.MobUtil.BeaconEvents.PHONE_BUTTON, dialFn);

  return this.dialButton_;
};


/**
 * Rewrites the phone-dialer icon based on the passed-in color and sets
 * the dialButton_ to that as a backgroundImage.  Note that we must use
 * a backgroundImage here; if it's a foreground image then the dialing
 * fails on a Samsung Galaxy Note 2.
 *
 * @param {!goog.color.Rgb} color
 */
pagespeed.MobDialer.prototype.setIcon = function(color) {
  if (this.dialButtonIcon_) {
    this.dialButtonIcon_.style.backgroundImage = 'url(' +
        pagespeed.MobUtil.synthesizeImage(
            pagespeed.MobDialer.DIAL_BUTTON_IMG_BASE64_, color) + ')';
  }
};


/**
 * Obtains a dynamic Google-Voice number to track conversions.  We only
 * do this when user clicks the phone icon.
 *
 * @private
 */
pagespeed.MobDialer.prototype.requestPhoneNumberAndDial_ = function() {
  var url = this.constructRequestPhoneNumberUrl_();
  if (url) {
    if (window.psDebugMode) {
      // We debug with alert() here because it is hard to debug on the
      // physical phone with console.log.  And while most of our code
      // can be debugged on the chrome emulator, this code only works
      // on actual phones.
      window.alert('requesting dynamic phone number: ' + url);
    }
    var req = new goog.net.Jsonp(url, 'callback');
    req.send(null,
             goog.bind(this.receivePhoneNumber_, this),
             goog.bind(this.dialPhone_, this));
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
pagespeed.MobDialer.prototype.constructRequestPhoneNumberUrl_ = function() {
  // The protocol for requesting the gvoice number from googleadservices
  // requires that we pass the fallback phone number into the URL.  So
  // to request a gvoice number we must have a default phoneNumber_ already.
  if (this.conversionLabel_ && this.conversionId_ && this.phoneNumber_) {
    var label = window.encodeURIComponent(this.conversionLabel_);
    return pagespeed.MobDialer.CONVERSION_HANDLER_ +
        window.encodeURIComponent(this.conversionId_) +
        '/wcm?cl=' + label +
        '&fb=' + window.encodeURIComponent(this.phoneNumber_);
  }
  return null;
};


/**
 * Dials a phone number.
 *
 * @private
 */
pagespeed.MobDialer.prototype.dialPhone_ = function() {
  if (this.phoneNumber_) {
    window.location.href = 'tel:' + this.phoneNumber_;
  }
};


/**
 * Extracts a dynamic phone number from a jsonp response.  If successful, it
 * dials the phone, and saves the phone number in a cookie and also in this.
 *
 * @private
 * @param {?Object} json
 */
pagespeed.MobDialer.prototype.receivePhoneNumber_ = function(json) {
  var wcm = json && json['wcm'];
  var phoneNumber = wcm && wcm['mobile_number'];
  if (phoneNumber) {
    // Save the phoneNumber in a cookie to reduce server requests.
    var cookieValue = {
      'expires': wcm['expires'],
      'formatted_number': wcm['formatted_number'],
      'mobile_number': phoneNumber,
      'clabel': this.conversionLabel_,
      'fallback': this.phoneNumber_
    };
    cookieValue = goog.json.serialize(cookieValue);
    if (window.psDebugMode) {
      window.alert('saving phoneNumber in cookie: ' + cookieValue);
    }
    this.cookies_.set(pagespeed.MobDialer.WCM_COOKIE_,
                      window.encodeURIComponent(cookieValue),
                      pagespeed.MobDialer.WCM_COOKIE_LIFETIME_SEC_, '/');

    // Save the phone number in the window object so it can be used in
    // dialPhone_().
    this.phoneNumber_ = phoneNumber;
  } else if (this.phoneNumber_ && window.psDebugMode) {
    // No ad was clicked.  Dial the configured phone number, which will not
    // be conversion-tracked.
    window.alert('receivePhoneNumber: ' + goog.json.serialize(json));
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
pagespeed.MobDialer.prototype.getPhoneNumberFromCookie_ = function() {
  // When debugging mobilization with static references
  // to uncompiled JS files, the conversion-tracking flow does
  // not seem to work, so just dial the configured phone directly.
  //
  // Naturally if there is no configured conversion label, we can't
  // get a conversion-tracked phone-number either.
  if (window.psStaticJs || !this.conversionLabel_) {
    return this.phoneNumber_;
  }

  // Check to see if the phone number we want was previously saved
  // in a valid cookie, with matching fallback number and conversion label.
  var gwcmCookie = this.cookies_.get(pagespeed.MobDialer.WCM_COOKIE_);
  if (gwcmCookie) {
    var cookieData = goog.json.parse(window.decodeURIComponent(gwcmCookie));
    if ((cookieData['fallback'] == this.phoneNumber_) &&
        (cookieData['clabel'] == this.conversionLabel_)) {
      return cookieData['mobile_number'];
    }
  }
  return null;
};
