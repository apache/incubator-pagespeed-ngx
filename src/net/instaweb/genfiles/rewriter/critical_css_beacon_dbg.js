(function(){var pagespeedutils = {sendBeacon:function(beaconUrl, htmlUrl, data) {
  var httpRequest;
  if (window.XMLHttpRequest) {
    httpRequest = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        httpRequest = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          httpRequest = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (e2) {
        }
      }
    }
  }
  if (!httpRequest) {
    return!1;
  }
  var query_param_char = -1 == beaconUrl.indexOf("?") ? "?" : "&", url = beaconUrl + query_param_char + "url=" + encodeURIComponent(htmlUrl);
  httpRequest.open("POST", url);
  httpRequest.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  httpRequest.send(data);
  return!0;
}, addHandler:function(elem, eventName, func) {
  if (elem.addEventListener) {
    elem.addEventListener(eventName, func, !1);
  } else {
    if (elem.attachEvent) {
      elem.attachEvent("on" + eventName, func);
    } else {
      var oldHandler = elem["on" + eventName];
      elem["on" + eventName] = function() {
        func.call(this);
        oldHandler && oldHandler.call(this);
      };
    }
  }
}, getPosition:function(element) {
  for (var top = element.offsetTop, left = element.offsetLeft;element.offsetParent;) {
    element = element.offsetParent, top += element.offsetTop, left += element.offsetLeft;
  }
  return{top:top, left:left};
}, getWindowSize:function() {
  var height = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width = window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth;
  return{height:height, width:width};
}, inViewport:function(element, windowSize) {
  var position = pagespeedutils.getPosition(element);
  return pagespeedutils.positionInViewport(position, windowSize);
}, positionInViewport:function(pos, windowSize) {
  return pos.top < windowSize.height && pos.left < windowSize.width;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.CriticalCssBeacon = function(beaconUrl, htmlUrl, optionsHash, nonce, selectors) {
  this.MAXITERS_ = 250;
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.selectors_ = selectors;
  this.criticalSelectors_ = [];
  this.idx_ = 0;
};
pagespeed.CriticalCssBeacon.prototype.sendBeacon_ = function() {
  for (var data = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, data = data + "&cs=", i = 0;i < this.criticalSelectors_.length;++i) {
    var tmp = 0 < i ? "," : "", tmp = tmp + encodeURIComponent(this.criticalSelectors_[i]);
    if (131072 < data.length + tmp.length) {
      break;
    }
    data += tmp;
  }
  pagespeed.criticalCssBeaconData = data;
  pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data);
};
pagespeed.CriticalCssBeacon.prototype.checkCssSelectors_ = function(callback) {
  for (var i = 0;i < this.MAXITERS_ && this.idx_ < this.selectors_.length;++i, ++this.idx_) {
    try {
      null != document.querySelector(this.selectors_[this.idx_]) && this.criticalSelectors_.push(this.selectors_[this.idx_]);
    } catch (e) {
    }
  }
  this.idx_ < this.selectors_.length ? window.setTimeout(this.checkCssSelectors_.bind(this), 0, callback) : callback();
};
pagespeed.criticalCssBeaconInit = function(beaconUrl, htmlUrl, optionsHash, nonce, selectors) {
  if (document.querySelector && Function.prototype.bind) {
    var temp = new pagespeed.CriticalCssBeacon(beaconUrl, htmlUrl, optionsHash, nonce, selectors), beacon_onload = function() {
      window.setTimeout(function() {
        temp.checkCssSelectors_(function() {
          temp.sendBeacon_();
        });
      }, 0);
    };
    pagespeedutils.addHandler(window, "load", beacon_onload);
  }
};
pagespeed.criticalCssBeaconInit = pagespeed.criticalCssBeaconInit;
})();
