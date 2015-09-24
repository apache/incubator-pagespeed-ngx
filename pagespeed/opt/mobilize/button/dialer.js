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

goog.provide('mob.button.Dialer');

goog.require('goog.json');
goog.require('goog.net.Cookies');  // abstraction gzipped size cost: 336 bytes.
goog.require('goog.net.Jsonp');
goog.require('mob.button.AbstractButton');
goog.require('mob.util');
goog.require('mob.util.BeaconEvents');
goog.require('mob.util.ElementId');


/**
 * Constants used for tracking the state of requesting a Google Voice number
 * and dialing.  This is needed to delay the dialing of the phone until
 * the GV request completes or times out.
 * @enum {number}
 * @private
 */
mob.button.DialerState_ = {
  IDLE: 0,
  REQUESTING: 1,
  DIAL_WHEN_REQUEST_COMPLETES: 2
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
 * @extends {mob.button.AbstractButton}
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

  /**
    * State whether we are currently requesting a google-voice number, so
    * we should wait for that request to complete before dialing a phone.
    * @private {!mob.button.DialerState_}
    */
  this.dialState_ = mob.button.DialerState_.IDLE;

  mob.button.Dialer.base(this, 'constructor', mob.util.ElementId.DIALER_BUTTON,
                         mob.button.Dialer.ICON_, color,
                         mob.button.Dialer.LABEL_);
};
goog.inherits(mob.button.Dialer, mob.button.AbstractButton);


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
mob.button.Dialer.MAX_WCM_COOKIE_LIFETIME_SEC_ = 3600 * 24 * 90;


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
  if (!this.googleVoicePhoneNumber_) {
    this.requestPhoneNumber_();
  }
};


/** @override */
mob.button.Dialer.prototype.clickHandler = function(e) {
  mob.util.sendBeaconEvent(mob.util.BeaconEvents.PHONE_BUTTON);
  if (this.dialState_ == mob.button.DialerState_.IDLE) {
    this.dialPhone_();
  } else {
    this.dialState_ = mob.button.DialerState_.DIAL_WHEN_REQUEST_COMPLETES;
  }
};


/**
 * Obtains a dynamic Google-Voice number to track conversions.  We do this
 * when loading a page.
 *
 * @private
 */
mob.button.Dialer.prototype.requestPhoneNumber_ = function() {
  if (this.dialState_ != mob.button.DialerState_.IDLE) {
    return;
  }
  var url = this.constructRequestPhoneNumberUrl_();
  if (url) {
    this.debugAlert_('requesting dynamic phone number: ' + url);
    var req = new goog.net.Jsonp(url);
    req.setRequestTimeout(2000 /* ms */);
    this.jsonpTime_ = goog.now();
    this.dialState_ = mob.button.DialerState_.REQUESTING;
    req.send(null, goog.bind(this.receivePhoneNumber_, this, true),
             goog.bind(this.receivePhoneNumber_, this, false));
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
    ev = mob.util.BeaconEvents.CALL_GV_NUMBER;
  } else {
    phoneNumber = this.fallbackPhoneNumber_;
    ev = mob.util.BeaconEvents.CALL_FALLBACK_NUMBER;
  }
  this.debugAlert_('Dialing phone: ' + phoneNumber + '(' + ev + ')');
  mob.util.sendBeaconEvent(
      ev, function() { goog.global.location = 'tel:' + phoneNumber; });
};


/**
 * Produce error description from wcm response.
 *
 * @private
 * @param {?number} backoff backoff value from wcm call.
 * @return {?string} description of error.
 **/
mob.button.Dialer.prototype.backoffErrorCode_ = function(backoff) {
  if (backoff) {
    switch (backoff) {
      case 300:
        return 'temporary-error';
      case 86400:
        return 'no-ad-click';
      case 86402:
        return 'not-tracked';
    }
    // Other cases are unknown errors.
    return 'error' + backoff;
  }
  return null;
};


/**
 * Extracts a dynamic phone number from a jsonp response, storing the phone
 * number in a cookie and in this.googleVoicePhoneNumber_.
 *
 * @private
 * @param {boolean} success True if the jsonp request succeeded.
 * @param {?Object} json
 */
mob.button.Dialer.prototype.receivePhoneNumber_ = function(success, json) {
  var responseTime = goog.now() - this.jsonpTime_;
  var wcm = json && json['wcm'];
  var phoneNumber = wcm && wcm['mobile_number'];
  var err = this.backoffErrorCode_(wcm && wcm['backoff']);
  var is_gv = !!(phoneNumber && phoneNumber != this.fallbackPhoneNumber_);
  mob.util.sendBeaconEvent(mob.util.BeaconEvents.CALL_CONVERSION_RESPONSE, null,
                           '&s=' + success.toString() + '&t=' + responseTime +
                               '&gv=' + is_gv.toString() +
                               (err ? '&err=' + err : ''));
  if (phoneNumber && phoneNumber != this.fallbackPhoneNumber_) {
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
    var expires = parseInt(wcm['expires'], 10);
    if (expires) {
      expires -= Math.floor(goog.now() / 1000);
      expires =
          Math.min(expires, mob.button.Dialer.MAX_WCM_COOKIE_LIFETIME_SEC_);
    } else {
      expires = mob.button.Dialer.MAX_WCM_COOKIE_LIFETIME_SEC_;
    }
    this.cookies_.set(mob.button.Dialer.WCM_COOKIE_,
                      window.encodeURIComponent(cookieValue), expires, '/');

    this.googleVoicePhoneNumber_ = phoneNumber;
  }
  if (err) {
    this.debugAlert_('WCM request: ' + err);
  }
  if (this.dialState_ == mob.button.DialerState_.DIAL_WHEN_REQUEST_COMPLETES) {
    this.dialPhone_();
  }
  this.dialState_ = mob.button.DialerState_.IDLE;
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
      var phoneNumber = cookieData['mobile_number'];
      this.debugAlert_('found phone number in cookie: ' + phoneNumber);
      return phoneNumber;
    }
  }
  this.debugAlert_('no phone number found in cookie');
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
