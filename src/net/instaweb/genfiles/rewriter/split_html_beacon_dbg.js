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
}, CriticalXPaths:function(viewWidth, viewHeight, document) {
  this.hasIdsReused = !1;
  this.document_ = document
}};
pagespeedutils.CriticalXPaths.prototype.getNonCriticalPanelXPaths = function() {
  this.findNonCriticalPanelBoundaries(this.document_.body);
  this.hasIdsReused && (this.xpathPairs = []);
  return this.xpathPairs
};
pagespeedutils.CriticalXPaths.prototype.xpath = function(node) {
  for(var xpathUnits = [];node != this.document_.body;) {
    if(node.hasAttribute("id")) {
      xpathUnits.unshift(node.tagName.toLowerCase() + '[@id="' + node.getAttribute("id") + '"]');
      1 != this.document_.querySelectorAll('[id="' + node.getAttribute("id") + '"]').length && (this.hasIdsReused = !0);
      break
    }else {
      for(var i = 0, sibling = node;sibling;sibling = sibling.previousElementSibling) {
        "SCRIPT" !== sibling.tagName && ("NOSCRIPT" !== sibling.tagName && "STYLE" !== sibling.tagName && "LINK" !== sibling.tagName) && ++i
      }
      xpathUnits.unshift(node.tagName.toLowerCase() + "[" + i + "]")
    }
    node = node.parentNode
  }
  return xpathUnits.length ? xpathUnits.join("/") : ""
};
pagespeedutils.CriticalXPaths.prototype.findNonCriticalPanelBoundaries = function(node) {
  for(var nodeIsCritical = this.inViewPort(node), prevChildIsCritical = nodeIsCritical, startChildXPath = "", endChildXPath = "", currChild = this.nextRenderedSibling(node.firstChild);null != currChild;currChild = this.nextRenderedSibling(currChild.nextSibling)) {
    var currChildIsCritical = this.findNonCriticalPanelBoundaries(currChild);
    if(currChildIsCritical != prevChildIsCritical) {
      if(currChildIsCritical) {
        if(!nodeIsCritical) {
          var firstChild = this.nextRenderedSibling(node.firstChild);
          firstChild != currChild && (startChildXPath = this.xpath(firstChild));
          nodeIsCritical = !0
        }
        ret = this.xpath(currChild);
        endChildXPath = ret.xpath;
        this.hasIdsReused = ret.hasIdsReused;
        startChildXPath && this.addXPathPair(startChildXPath, endChildXPath);
        startChildXPath = ""
      }else {
        var ret = this.xpath(currChild), startChildXPath = ret.xpath;
        this.hasIdsReused = ret.hasIdsReused;
        endChildXPath = ""
      }
      prevChildIsCritical = currChildIsCritical
    }
  }
  startChildXPath && this.addXPathPair(startChildXPath, endChildXPath);
  return nodeIsCritical
};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.SplitHtmlBeacon = function(beaconUrl, htmlUrl, optionsHash) {
  this.xpathPairs = [];
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.windowSize_ = this.getWindowSize_()
};
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  var criticalXPaths = new pagespeedutils.CriticalXPaths(this.windowSize_.width, this.windowSize_.height, document);
  this.xpathPairs = criticalXPaths.getNonCriticalPanelXPaths();
  if(0 != this.xpathPairs.length) {
    for(var data = "oh=" + this.optionsHash_, data = data + ("&xp=" + encodeURIComponent(this.xpathPairs[0])), i = 1;i < this.xpathPairs.length;++i) {
      var tmp = "," + encodeURIComponent(this.xpathPairs[i]);
      if(131072 < data.length + tmp.length) {
        break
      }
      data += tmp
    }
    pagespeed.splitHtmlBeaconData = data;
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, data)
  }
};
pagespeed.addHandler = function(elem, ev, func) {
  if(elem.addEventListener) {
    elem.addEventListener(ev, func, !1)
  }else {
    if(elem.attachEvent) {
      elem.attachEvent("on" + ev, func)
    }else {
      var oldHandler = elem["on" + ev];
      elem["on" + ev] = function() {
        func.call(this);
        oldHandler && oldHandler.call(this)
      }
    }
  }
};
pagespeed.splitHtmlBeaconInit = function(beaconUrl, htmlUrl, optionsHash, nonce) {
  var temp = new pagespeed.SplitHtmlBeacon(beaconUrl, htmlUrl, optionsHash, nonce), beacon_onload = function() {
    temp.checkSplitHtml_()
  };
  pagespeed.addHandler(window, "load", beacon_onload)
};
pagespeed.splitHtmlBeaconInit = pagespeed.splitHtmlBeaconInit;
})();
