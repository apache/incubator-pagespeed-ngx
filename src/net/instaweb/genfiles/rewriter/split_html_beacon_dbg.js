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
pagespeedutils.CriticalXPaths = function(viewportWidth, viewportHeight, document) {
  this.windowSize_ = {height:viewportHeight, width:viewportWidth};
  this.xpathPairs_ = [];
  this.areIdsReused_ = !1;
  this.document_ = document
};
pagespeedutils.CriticalXPaths.prototype.getNonCriticalPanelXPathPairs = function() {
  this.findNonCriticalPanelBoundaries_(this.document_.body);
  return this.areIdsReused_ ? [] : this.xpathPairs_
};
pagespeedutils.CriticalXPaths.prototype.generateXPath_ = function(node) {
  for(var xpathUnits = [];node != this.document_.body;) {
    if(node.hasAttribute("id")) {
      xpathUnits.unshift(node.tagName.toLowerCase() + '[@id="' + node.getAttribute("id") + '"]');
      1 != this.document_.querySelectorAll('[id="' + node.getAttribute("id") + '"]').length && (this.areIdsReused_ = !0);
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
pagespeedutils.CriticalXPaths.prototype.addXPathPair_ = function(startXpath, endXpath) {
  var xpathPair = startXpath;
  endXpath && (xpathPair += ":" + endXpath);
  this.xpathPairs_.push(xpathPair)
};
pagespeedutils.CriticalXPaths.prototype.findNonCriticalPanelBoundaries_ = function(node) {
  for(var nodeIsCritical = pagespeedutils.inViewport(node, this.windowSize_), prevChildIsCritical = nodeIsCritical, startChildXPath = "", endChildXPath = "", firstChild = this.visibleNodeOrSibling_(node.firstChild), currNode = firstChild;null != currNode;currNode = this.visibleNodeOrSibling_(currNode.nextSibling)) {
    var currNodeIsCritical = this.findNonCriticalPanelBoundaries_(currNode);
    currNodeIsCritical != prevChildIsCritical && (currNodeIsCritical ? (nodeIsCritical || (firstChild != currNode && (startChildXPath = this.generateXPath_(firstChild)), nodeIsCritical = !0), endChildXPath = this.generateXPath_(currNode), startChildXPath && this.addXPathPair_(startChildXPath, endChildXPath), startChildXPath = "") : (startChildXPath = this.generateXPath_(currNode), endChildXPath = ""), prevChildIsCritical = currNodeIsCritical)
  }
  startChildXPath && this.addXPathPair_(startChildXPath, endChildXPath);
  return nodeIsCritical
};
pagespeedutils.CriticalXPaths.prototype.visibleNodeOrSibling_ = function(node) {
  for(;null != node;node = node.nextSibling) {
    if(node.nodeType == node.ELEMENT_NODE && (0 != node.offsetWidth || 0 != node.offsetHeight) && (node.offsetParent || "BODY" == node.tagName)) {
      return node
    }
  }
  return null
};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.SplitHtmlBeacon = function(beaconUrl, htmlUrl, optionsHash) {
  this.xpathPairs = [];
  this.beaconUrl_ = beaconUrl;
  this.htmlUrl_ = htmlUrl;
  this.optionsHash_ = optionsHash;
  this.windowSize_ = pagespeedutils.getWindowSize()
};
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  var criticalXPaths = new pagespeedutils.CriticalXPaths(this.windowSize_.width, this.windowSize_.height, document);
  this.xpathPairs = criticalXPaths.getNonCriticalPanelXPathPairs();
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
pagespeed.splitHtmlBeaconInit = function(beaconUrl, htmlUrl, optionsHash, nonce) {
  var temp = new pagespeed.SplitHtmlBeacon(beaconUrl, htmlUrl, optionsHash, nonce), beacon_onload = function() {
    temp.checkSplitHtml_()
  };
  pagespeedutils.addHandler(window, "load", beacon_onload)
};
pagespeed.splitHtmlBeaconInit = pagespeed.splitHtmlBeaconInit;
})();
