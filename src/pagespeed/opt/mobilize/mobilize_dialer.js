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

goog.provide('pagespeed.MobDialer');

goog.require('goog.dom.TagName');
goog.require('goog.json');
goog.require('goog.net.Jsonp');
goog.require('pagespeed.MobUtil');



/**
 * Creates a phone dialer.
 * @param {?string} phoneNumber
 * @param {?string} conversionId
 * @param {?string} conversionLabel
 * @constructor
 */
pagespeed.MobDialer = function(phoneNumber, conversionId, conversionLabel) {
  /**
   * Call button in the header bar.
   * @private {Element}
   */
  this.callButton_ = null;

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
};


/**
 * GIF image of call button. To view this image, add prefix of
 * 'data:image/gif;base64,' to it.
 * @const {string}
 */
pagespeed.MobDialer.CALL_BUTTON =
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
 * Creates a phone-dialer button and returns it.  Returns null if there
 * is no fallback phone number passed into the constructor.
 * @return {Element}
 */
pagespeed.MobDialer.prototype.createButton = function() {
  if (this.phoneNumber_) {
    this.callButton_ = document.createElement(goog.dom.TagName.DIV);
    this.callButton_.id = 'psmob-phone-dialer';
    var phone = this.getPhoneNumberFromCookie_();
    var dialFn;
    if (phone) {
      this.phoneNumber_ = phone;
      dialFn = goog.bind(this.dialPhone_, this);
    } else {
      dialFn = goog.bind(this.requestPhoneNumberAndCall_, this);
    }
    this.callButton_.onclick = goog.partial(
        pagespeed.MobUtil.trackClick, 'psmob-phone-dialer', dialFn);
  }
  return this.callButton_;
};


/**
 * @param {string} heightString
 * @return {number}
 */
pagespeed.MobDialer.prototype.adjustHeight = function(heightString) {
  if (this.callButton_) {
    this.callButton_.style.width = heightString;
    return this.callButton_.parentNode.offsetHeight;
  }
  return 0;
};


/**
 * @param {!goog.color.Rgb} color
 */
pagespeed.MobDialer.prototype.setColor = function(color) {
  if (this.callButton_) {
    this.callButton_.style.backgroundImage = 'url(' +
        pagespeed.MobUtil.synthesizeImage(
            pagespeed.MobDialer.CALL_BUTTON, color) + ')';
  }
};


/**
 * Obtains a dynamic Google-Voice number to track conversions.  We only
 * do this when user clicks the phone icon.
 * @private
 */
pagespeed.MobDialer.prototype.requestPhoneNumberAndCall_ = function() {
  if (this.conversionLabel_ && this.conversionId_ && this.phoneNumber_) {
    // No request from cookie.
    var label = escape(this.conversionLabel_);
    var url = 'https://www.googleadservices.com/pagead/conversion/' +
        this.conversionId_ + '/wcm?cl=' + label +
        '&fb=' + escape(this.phoneNumber_);
    if (window.psDebugMode) {
      alert('requesting dynamic phone number: ' + url);
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
 * Dials a phone number.
 * @private
 */
pagespeed.MobDialer.prototype.dialPhone_ = function() {
  if (this.phoneNumber_) {
    location.href = 'tel:' + this.phoneNumber_;
  }
};


/**
 * Extracts a dynamic phone number from a jsonp response.  If successful, it
 * calls the phone, and saves the phone number in a cookie and also in the
 * window object.
 *
 * @private
 * @param {Object} json
 */
pagespeed.MobDialer.prototype.receivePhoneNumber_ = function(json) {
  var phone = null;
  if (json && json['wcm']) {
    var wcm = json['wcm'];
    if (wcm) {
      phone = wcm['mobile_number'];
    }
  }

  if (phone) {
    // Save the phone in a cookie to reduce server requests.
    var cookieValue = {
      'expires': wcm['expires'],
      'formatted_number': wcm['formatted_number'],
      'mobile_number': phone,
      'clabel': this.conversionLabel_,
      'fallback': this.phoneNumber_
    };
    cookieValue = goog.json.serialize(cookieValue);
    if (window.psDebugMode) {
      alert('saving phone in cookie: ' + cookieValue);
    }
    document.cookie = 'gwcm=' + escape(cookieValue) + ';path=/;max-age=' +
        (3600 * 24 * 90);

    // Save the phone number in the window object so it can be used in
    // dialPhone_().
    this.phoneNumber_ = phone;
  } else if (this.phoneNumber_) {
    // No ad was clicked.  Call the configured phone number, which will not
    // be conversion-tracked.
    if (window.psDebugMode) {
      alert('receivePhoneNumber: ' + goog.json.serialize(json));
    }
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
  var match = document.cookie.match(/(^|;| )gwcm=([^;]+)/);
  if (match) {
    var gwcmCookie = match[2];
    if (gwcmCookie) {
      var cookieData = goog.json.parse(unescape(match[2]));
      if ((cookieData['fallback'] == this.phoneNumber_) &&
          (cookieData['clabel'] == this.conversionLabel_)) {
        return cookieData['mobile_number'];
      }
    }
  }
  return null;
};
