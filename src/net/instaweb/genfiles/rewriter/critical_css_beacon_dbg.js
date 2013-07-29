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
pagespeed.computeCriticalSelectors = function(selectors) {
  for(var critical_selectors = [], i = 0;i < selectors.length;++i) {
    try {
      0 < document.querySelectorAll(selectors[i]).length && critical_selectors.push(selectors[i])
    }catch(e) {
    }
  }
  return critical_selectors
};
pagespeed.computeCriticalSelectors = pagespeed.computeCriticalSelectors;
pagespeed.CriticalCssBeacon = function(beaconUrl, htmlUrl, optionsHash, nonce, selectors) {
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.nonce_ = nonce;
  this.selectors_ = selectors
};
pagespeed.CriticalCssBeacon.prototype.checkCssSelectors_ = function() {
  for(var critical_selectors = pagespeed.computeCriticalSelectors(this.selectors_), data = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, data = data + "&cs=", i = 0;i < critical_selectors.length;++i) {
    var tmp = 0 < i ? "," : "", tmp = tmp + encodeURIComponent(critical_selectors[i]);
    if(131072 < data.length + tmp.length) {
      break
    }
    data += tmp
  }
  pagespeed.criticalCssBeaconData = data;
  pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data)
};
pagespeed.criticalCssBeaconInit = function(beaconUrl, htmlUrl, optionsHash, nonce, selectors) {
  if(document.querySelectorAll) {
    var temp = new pagespeed.CriticalCssBeacon(beaconUrl, htmlUrl, optionsHash, nonce, selectors), beacon_onload = function() {
      window.setTimeout(function() {
        temp.checkCssSelectors_()
      }, 0)
    };
    pagespeedutils.addHandler(window, "load", beacon_onload)
  }
};
pagespeed.criticalCssBeaconInit = pagespeed.criticalCssBeaconInit;
})();
