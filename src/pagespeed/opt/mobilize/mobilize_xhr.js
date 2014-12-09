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


goog.provide('pagespeed.XhrHijack');



/**
 * @fileoverview Hijacks the construction of XHR objects, for two reasons:
 *
 * 1. Provides a notification callback whenever any XHR is sent or completes
 * on a page.
 *
 * 2. Provides an opportunity to alter the XHR request URL to correct a domain,
 * which will likely be needed when using a ProxySuffix for sites that form
 * XHR requests using absolute domains.
 *
 * So far we only need this for the notification callback.
 *
 * This JS file should be loaded ahead of any other scripts, preferably inline.
 * Post-compile it's 1366 bytes uncompressed, 521 bytes compressed as of now.
 * Its interface is that another JavaScript file can call
 *    window.pagespeedXhrHijackSetListener(listener)
 * where listener is a map that has two function properties: xhrSendHook() and
 * xhrResponseHook(http_status_code).  No other external API is provided.
 *
 * This file will hijack all XHR requests starting immediately when it's loaded.
 * When a listener is registered, even if it's much later in the page load,
 * it will receive any pending callback notifications in order.
 */



/**
 * Object to hijack XHR XMLHttpRequests.  We use this for tracking active
 * XHR requests.
 * @param {XMLHttpRequest} xhr The XHR request.
 * @constructor
 *
 * TODO(jmarantz): considering creating a subclass of goog.net.XhrLike (see
 * goog.net.IeCorsXhrAdapter in
 * http://docs.closure-library.googlecode.com/git/local_closure_goog_net_corsxmlhttpfactory.js.source.html
 * That will at least give us type annotations to copy.
 */
pagespeed.XhrHijack = function(xhr) {
  this.xhr = xhr || new this.XMLHttpRequestConstructor_();
  this.onreadystatechange = null;
  this.readyState = 0;
  this.responseText = '';
  this.statusText = '';
  this.xhr.onreadystatechange = this.readyCallback.bind(this);
  this.xhr.onload = this.onloadCallback.bind(this);
};


/**
 * Array of objects representing any send/response events that transpired
 * prior to the listener being established.  Send events are represented with
 * an 's' and response events are represented with the numeric http status
 * code. When a listener is established, any pending send/response events are
 * immediately sent to it and queuedHooks is cleared.
 *
 * @private
 */
pagespeed.XhrHijack.queuedEvents_ = [];


/**
 * Object to get notified whenever XHR activity occurs.  This is set globally
 * for the class because the listener doesn't have control of when XHRs are
 * constructed.
 *
 * @private
 */
pagespeed.XhrHijack.listener_ = {};


/**
 * @const
 * @private
 */
pagespeed.XhrHijack.SEND_EVENT_CHAR_ = 's';


/**
 * Default handler for xhrSendHook.
 */
pagespeed.XhrHijack.listener_['xhrSendHook'] = function() {
  pagespeed.XhrHijack.queuedEvents_.push(pagespeed.XhrHijack.SEND_EVENT_CHAR_);
};



/**
 * Default handler for xhrResponseHook.
 * @param {number} http_status
 */
pagespeed.XhrHijack.listener_['xhrResponseHook'] = function(http_status) {
  pagespeed.XhrHijack.queuedEvents_.push(http_status);
};



/**
 * Establishes a listener instance for XHR events.  If any XHR events were
 * initiated before the listener is established, it immediately receives
 * the queued events.
 *
 * The listener must be a class instance that defines these methods:
 *    listener_.xhrSendHook()
 *    listener_.xhrResponseHook(http_status_code)
 *
 * @param {!Object} listener
 *
 * TODO(jmarantz): Use an at-interface with the above methods defined, and
 * then use the type annotation to ensure they are defined correctly.
 */
pagespeed.XhrHijack.setListener = function(listener) {
  pagespeed.XhrHijack.listener_ = listener;
  for (var i = 0; i < pagespeed.XhrHijack.queuedEvents_.length; ++i) {
    var event = pagespeed.XhrHijack.queuedEvents_[i];
    if (event == pagespeed.XhrHijack.SEND_EVENT_CHAR_) {
      pagespeed.XhrHijack.listener_['xhrSendHook']();
    } else {
      pagespeed.XhrHijack.listener_['xhrResponseHook'](event);
    }
  }
  pagespeed.XhrHijack.queuedEvents_ = null;  // Will not be used again.
};
window['pagespeedXhrHijackSetListener'] = pagespeed.XhrHijack.setListener;

/**
 * Captures the native XMLHttpRequest constructor, so we can insert
 * our own hooks to run after the normal callback runs.
 * @private
 */
pagespeed.XhrHijack.prototype.XMLHttpRequestConstructor_ = XMLHttpRequest;


/**
 * Callback to run onload, transferring the responseText, and calling
 * client onload function.
 */
pagespeed.XhrHijack.prototype.onloadCallback = function() {
  if (this.xhr.responseText) {
    this.responseText = this.xhr.responseText;
  }
  if (this.onload) {
    this.onload();
  }
};


/**
 * Wrapped onreadystatechange callback.
 */
pagespeed.XhrHijack.prototype.readyCallback = function() {
  // hijack more: http://www.w3.org/TR/2006/WD-XMLHttpRequest-20060405/
  this.readyState = this.xhr.readyState;
  this.status = this.xhr.status;
  this.responseText = this.xhr.responseText;
  if (this.xhr.statusText) {
    this.statusText = this.xhr.statusText;
  }
  if (this.onreadystatechange) {
    this.onreadystatechange();
  }
  if (this.readyState == 4) {
    pagespeed.XhrHijack.listener_['xhrResponseHook'](this.status);
  }
};


/**
 * Wrapped abort callback.
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['abort'] = function() {
  this.xhr.abort();
};


/**
 * Hijacked open call.  Strictly delegates for now, but this
 * gives us an opportunity to domain-correct for proxy-suffix
 * as needed.
 * @param {string} a
 * @param {string} b
 * @param {?boolean} c
 * @param {?string} d
 * @param {?string} e
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['open'] = function(a, b, c, d, e) {
  this.xhr.open(a, b, c, d, e);
};


/**
 * Hijacked send call, helping us track outstanding XHRs.
 * @param {ArrayBuffer|ArrayBufferView|Blob|Document|FormData|null|string} x
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['send'] = function(x) {
  pagespeed.XhrHijack.listener_['xhrSendHook']();
  this.xhr.send(x);
};


/**
 * Hijacked.
 * @param {string} x
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['overrideMimeType'] = function(x) {
  this.xhr.overrideMimeType(x);
};


/**
 * Hijacked.
 * @param {string} name The name of the request header.
 * @param {string} value The value of the requets header.
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['setRequestHeader'] = function(name, value) {
  this.xhr.setRequestHeader(name, value);
};


/**
 * Hijacked.
 * @return {string}
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['getAllResponseHeaders'] = function() {
  return this.xhr.getAllResponseHeaders();
};


/**
 * Hijacked.
 * @param {string} name
 * @return {string}
 * @this {pagespeed.XhrHijack}
 *
 * Note: this usage of at-this is deployed in lieu of at-export because of
 * much smaller output file sizes for this module, which is loaded blocking
 * in head.
 */
pagespeed.XhrHijack.prototype['getResponseHeader'] = function(name) {
  return this.xhr.getResponseHeader(name);
};


/**
 * Hijack XMLHttpRequest with pagespeed.XhrHijack.
 */
window.XMLHttpRequest = /** @type {function (new:XMLHttpRequest)} */
    (pagespeed.XhrHijack);
