(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, c) {
  var d = -1 == a.indexOf("?") ? "?" : "&";
  a = a + d + "url=" + encodeURIComponent(b);
  return window.navigator && window.navigator.sendBeacon ? (window.navigator.sendBeacon(a, c), !0) : window.XMLHttpRequest ? (b = new XMLHttpRequest, b.open("POST", a), b.setRequestHeader("Content-Type", "text/plain;charset=UTF-8"), b.send(c), !0) : !1;
}, addHandler:function(a, b, c) {
  if (a.addEventListener) {
    a.addEventListener(b, c, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + b, c);
    } else {
      var d = a["on" + b];
      a["on" + b] = function() {
        c.call(this);
        d && d.call(this);
      };
    }
  }
}, getPosition:function(a) {
  for (var b = a.offsetTop, c = a.offsetLeft;a.offsetParent;) {
    a = a.offsetParent, b += a.offsetTop, c += a.offsetLeft;
  }
  return {top:b, left:c};
}, getWindowSize:function() {
  return {height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, b) {
  var c = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(c, b);
}, positionInViewport:function(a, b) {
  return a.top < b.height && a.left < b.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
pagespeedutils.now = Date.now || function() {
  return +new Date;
};
pagespeedutils.CriticalXPaths = function(a, b, c) {
  this.windowSize_ = {height:b, width:a};
  this.xpathPairs_ = [];
  this.document_ = c;
};
pagespeedutils.CriticalXPaths.prototype.getNonCriticalPanelXPathPairs = function() {
  this.findNonCriticalPanelBoundaries_(this.document_.body);
  return this.xpathPairs_;
};
pagespeedutils.CriticalXPaths.prototype.addXPathPair_ = function(a, b) {
  if (a) {
    var c = a;
    b && (c += ":" + b);
    this.xpathPairs_.push(c);
  }
};
pagespeedutils.CriticalXPaths.prototype.findNonCriticalPanelBoundaries_ = function(a) {
  for (var b = pagespeedutils.inViewport(a, this.windowSize_), c = b, d = "", e = "", f = a = this.visibleNodeOrSibling_(a.firstChild);null != f;f = this.visibleNodeOrSibling_(f.nextSibling)) {
    var g = this.findNonCriticalPanelBoundaries_(f);
    g != c && (g ? (b || (a != f && (d = pagespeedutils.generateXPath(a, this.document_)), b = !0), (e = pagespeedutils.generateXPath(f, this.document_)) || (d = ""), d && this.addXPathPair_(d, e), d = "") : (d = pagespeedutils.generateXPath(f, this.document_), e = ""), c = g);
  }
  d && this.addXPathPair_(d, e);
  return b;
};
pagespeedutils.generateXPath = function(a, b) {
  for (var c = [];a != b.body;) {
    var d = a.getAttribute("id");
    if (d && 1 == b.querySelectorAll("#" + d).length) {
      c.unshift(a.tagName.toLowerCase() + '[@id="' + a.getAttribute("id") + '"]');
      break;
    } else {
      for (var d = 0, e = a;e;e = e.previousElementSibling) {
        "SCRIPT" !== e.tagName && "NOSCRIPT" !== e.tagName && "STYLE" !== e.tagName && "LINK" !== e.tagName && ++d;
      }
      c.unshift(a.tagName.toLowerCase() + "[" + d + "]");
    }
    a = a.parentNode;
  }
  return c.length ? c.join("/") : "";
};
pagespeedutils.CriticalXPaths.prototype.visibleNodeOrSibling_ = function(a) {
  for (;null != a;a = a.nextSibling) {
    if (a.nodeType == Node.ELEMENT_NODE && (0 != a.offsetWidth || 0 != a.offsetHeight) && (a.offsetParent || "BODY" == a.tagName || "fixed" == a.style.position)) {
      return a;
    }
  }
  return null;
};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.SplitHtmlBeacon = function(a, b, c, d) {
  this.btfNodes_ = [];
  this.beaconUrl_ = a;
  this.htmlUrl_ = b;
  this.optionsHash_ = c;
  this.nonce_ = d;
  this.windowSize_ = pagespeedutils.getWindowSize();
};
pagespeed.SplitHtmlBeacon.prototype.walkDom_ = function(a) {
  for (var b = !0, c = [], d = a.firstChild;null != d;d = d.nextSibling) {
    d.nodeType === Node.ELEMENT_NODE && "SCRIPT" != d.tagName && "NOSCRIPT" != d.tagName && "STYLE" != d.tagName && "LINK" != d.tagName && (this.walkDom_(d) ? c.push(d) : b = !1);
  }
  if (b && !pagespeedutils.inViewport(a, this.windowSize_)) {
    return !0;
  }
  for (a = 0;a < c.length;++a) {
    this.btfNodes_.push(pagespeedutils.generateXPath(c[a], window.document));
  }
  return !1;
};
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  this.walkDom_(document.body);
  if (0 != this.btfNodes_.length) {
    for (var a = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, a = a + ("&xp=" + encodeURIComponent(this.btfNodes_[0])), b = 1;b < this.btfNodes_.length;++b) {
      var c = "," + encodeURIComponent(this.btfNodes_[b]);
      if (a.length + c.length > pagespeedutils.MAX_POST_SIZE) {
        break;
      }
      a += c;
    }
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, a);
  }
};
pagespeed.splitHtmlBeaconInit = function(a, b, c, d) {
  var e = new pagespeed.SplitHtmlBeacon(a, b, c, d);
  pagespeedutils.addHandler(window, "load", function() {
    e.checkSplitHtml_();
  });
};
pagespeed.splitHtmlBeaconInit = pagespeed.splitHtmlBeaconInit;
})();
