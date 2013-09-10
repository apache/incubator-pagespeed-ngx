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
}, getPosition:function(element) {
  for(var top = element.offsetTop, left = element.offsetLeft;element.offsetParent;) {
    element = element.offsetParent, top += element.offsetTop, left += element.offsetLeft
  }
  return{top:top, left:left}
}, getWindowSize:function() {
  var height = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width = window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth;
  return{height:height, width:width}
}, inViewport:function(element, windowSize) {
  var position = pagespeedutils.getPosition(element);
  return pagespeedutils.positionInViewport(position, windowSize)
}, positionInViewport:function(pos, windowSize) {
  return pos.top < windowSize.height && pos.left < windowSize.width
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.CriticalImagesBeacon = function(beaconUrl, htmlUrl, optionsHash, is_resize_using_rendered_dimensions_enabled, nonce) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.windowSize_ = pagespeedutils.getWindowSize();
  this.is_resize_using_rendered_dimensions_enabled_ = is_resize_using_rendered_dimensions_enabled;
  this.imgLocations_ = {}
};
pagespeed.CriticalImagesBeacon.prototype.elLocation_ = function(element) {
  var rect = element.getBoundingClientRect(), scroll_x, scroll_y;
  scroll_x = void 0 !== window.pageXOffset ? window.pageXOffset : (document.documentElement || document.body.parentNode || document.body).scrollLeft;
  scroll_y = void 0 !== window.pageYOffset ? window.pageYOffset : (document.documentElement || document.body.parentNode || document.body).scrollTop;
  return{top:rect.top + scroll_y, left:rect.left + scroll_x}
};
pagespeed.CriticalImagesBeacon.prototype.isCritical_ = function(element) {
  if(0 >= element.offsetWidth && 0 >= element.offsetHeight) {
    return!1
  }
  var elLocation = this.elLocation_(element), elLocationStr = elLocation.top.toString() + "," + elLocation.left.toString();
  if(this.imgLocations_.hasOwnProperty(elLocationStr)) {
    return!1
  }
  this.imgLocations_[elLocationStr] = !0;
  return elLocation.top <= this.windowSize_.height && elLocation.left <= this.windowSize_.width
};
pagespeed.CriticalImagesBeacon.prototype.checkCriticalImages_ = function() {
  for(var tags = ["img", "input"], critical_imgs = [], critical_imgs_keys = {}, i = 0;i < tags.length;++i) {
    for(var elements = document.getElementsByTagName(tags[i]), j = 0;j < elements.length;++j) {
      var key = elements[j].getAttribute("pagespeed_url_hash");
      key && elements[j].getBoundingClientRect && this.isCritical_(elements[j]) && !(key in critical_imgs_keys) && (critical_imgs.push(key), critical_imgs_keys[key] = !0)
    }
  }
  var is_data_available = !1, data = "oh=" + this.optionsHash_;
  if(0 != critical_imgs.length) {
    this.nonce_.empty() || (data += "&n=" + this.nonce_);
    data += "&ci=" + encodeURIComponent(critical_imgs[0]);
    for(i = 1;i < critical_imgs.length;++i) {
      var tmp = "," + encodeURIComponent(critical_imgs[i]);
      if(131072 < data.length + tmp.length) {
        break
      }
      data += tmp
    }
    is_data_available = !0
  }
  this.is_resize_using_rendered_dimensions_enabled_ && (data += "&rd=" + encodeURIComponent(JSON.stringify(this.getImageRenderedMap())), is_data_available = !0);
  is_data_available && (pagespeed.criticalImagesBeaconData = data, pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data))
};
pagespeed.CriticalImagesBeacon.prototype.getImageRenderedMap = function() {
  for(var renderedImageDimensions = {}, images = document.getElementsByTagName("img"), i = 0;i < images.length;++i) {
    var img = images[i], key = img.getAttribute("pagespeed_url_hash");
    if("undefined" == typeof img.naturalWidth || "undefined" == typeof img.naturalHeight || "undefined" == typeof key) {
      break
    }
    if("undefined" == typeof renderedImageDimensions[img.src] && 0 < img.width && 0 < img.height && 0 < img.naturalWidth && 0 < img.naturalHeight || "undefined" != typeof renderedImageDimensions[img.src] && img.width >= renderedImageDimensions[img.src].renderedWidth && img.height >= renderedImageDimensions[img.src].renderedHeight) {
      renderedImageDimensions[key] = {renderedWidth:img.width, renderedHeight:img.height, originalWidth:img.naturalWidth, originalHeight:img.naturalHeight}
    }
  }
  return renderedImageDimensions
};
pagespeed.criticalImagesBeaconInit = function(beaconUrl, htmlUrl, optionsHash, is_resize_using_rendered_dimensions_enabled, nonce) {
  var temp = new pagespeed.CriticalImagesBeacon(beaconUrl, htmlUrl, optionsHash, is_resize_using_rendered_dimensions_enabled, nonce), beacon_onload = function() {
    window.setTimeout(function() {
      temp.checkCriticalImages_()
    }, 0)
  };
  pagespeedutils.addHandler(window, "load", beacon_onload)
};
pagespeed.criticalImagesBeaconInit = pagespeed.criticalImagesBeaconInit;
})();
