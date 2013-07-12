(function(){var pagespeedutils = {sendBeacon:function(beaconUrl, htmlUrl, data) {
  var httpRequest;
  if(window.XMLHttpRequest) {
    httpRequest = new XMLHttpRequest
  }else {
    if(window.ActiveXObject) {
      try {
        httpRequest = new ActiveXObject("Msxml2.XMLHTTP")
      }catch(e) {
        try {
          httpRequest = new ActiveXObject("Microsoft.XMLHTTP")
        }catch(e2) {
        }
      }
    }
  }
  if(!httpRequest) {
    return!1
  }
  var query_param_char = -1 == beaconUrl.indexOf("?") ? "?" : "&", url = beaconUrl + query_param_char + "url=" + encodeURIComponent(htmlUrl);
  httpRequest.open("POST", url);
  httpRequest.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  httpRequest.send(data);
  return!0
}, addHandler:function(elem, eventName, func) {
  if(elem.addEventListener) {
    elem.addEventListener(eventName, func, !1)
  }else {
    if(elem.attachEvent) {
      elem.attachEvent("on" + eventName, func)
    }else {
      var oldHandler = elem["on" + eventName];
      elem["on" + eventName] = function() {
        func.call(this);
        oldHandler && oldHandler.call(this)
      }
    }
  }
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.DelayImages = function() {
  this.highResReplaced_ = this.lazyLoadHighResHandlersRegistered_ = !1
};
pagespeed.DelayImages.prototype.replaceElementSrc = function(elements) {
  for(var i = 0;i < elements.length;++i) {
    var src = elements[i].getAttribute("pagespeed_high_res_src");
    src && elements[i].setAttribute("src", src)
  }
};
pagespeed.DelayImages.prototype.replaceElementSrc = pagespeed.DelayImages.prototype.replaceElementSrc;
pagespeed.DelayImages.prototype.registerLazyLoadHighRes = function() {
  if(this.lazyLoadHighResHandlersRegistered_) {
    this.highResReplaced_ = !1
  }else {
    var elem = document.body, interval = 500, tapStart, tapEnd = 0, me = this;
    "ontouchstart" in elem ? (pagespeedutils.addHandler(elem, "touchstart", function() {
      tapStart = Date.now()
    }), pagespeedutils.addHandler(elem, "touchend", function(e) {
      tapEnd = Date.now();
      (null != e.changedTouches && 2 == e.changedTouches.length || null != e.touches && 2 == e.touches.length || tapEnd - tapStart < interval) && me.loadHighRes()
    })) : pagespeedutils.addHandler(window, "click", function() {
      me.loadHighRes()
    });
    pagespeedutils.addHandler(window, "load", function() {
      me.loadHighRes()
    });
    this.lazyLoadHighResHandlersRegistered_ = !0
  }
};
pagespeed.DelayImages.prototype.registerLazyLoadHighRes = pagespeed.DelayImages.prototype.registerLazyLoadHighRes;
pagespeed.DelayImages.prototype.loadHighRes = function() {
  this.highResReplaced_ || (this.replaceWithHighRes(), this.highResReplaced_ = !0)
};
pagespeed.DelayImages.prototype.replaceWithHighRes = function() {
  this.replaceElementSrc(document.getElementsByTagName("img"));
  this.replaceElementSrc(document.getElementsByTagName("input"))
};
pagespeed.DelayImages.prototype.replaceWithHighRes = pagespeed.DelayImages.prototype.replaceWithHighRes;
pagespeed.delayImagesInit = function() {
  var temp = new pagespeed.DelayImages;
  pagespeed.delayImages = temp
};
pagespeed.delayImagesInit = pagespeed.delayImagesInit;
})();
