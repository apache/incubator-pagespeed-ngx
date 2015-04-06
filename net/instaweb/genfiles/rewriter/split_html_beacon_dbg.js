(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, c, d) {
  var b;
  if (window.XMLHttpRequest) {
    b = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        b = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          b = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (f) {
        }
      }
    }
  }
  if (!b) {
    return !1;
  }
  var g = -1 == a.indexOf("?") ? "?" : "&";
  a = a + g + "url=" + encodeURIComponent(c);
  b.open("POST", a);
  b.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  b.send(d);
  return !0;
}, addHandler:function(a, c, d) {
  if (a.addEventListener) {
    a.addEventListener(c, d, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + c, d);
    } else {
      var b = a["on" + c];
      a["on" + c] = function() {
        d.call(this);
        b && b.call(this);
      };
    }
  }
}, getPosition:function(a) {
  for (var c = a.offsetTop, d = a.offsetLeft;a.offsetParent;) {
    a = a.offsetParent, c += a.offsetTop, d += a.offsetLeft;
  }
  return {top:c, left:d};
}, getWindowSize:function() {
  return {height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, c) {
  var d = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(d, c);
}, positionInViewport:function(a, c) {
  return a.top < c.height && a.left < c.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
pagespeedutils.CriticalXPaths = function(a, c, d) {
  this.windowSize_ = {height:c, width:a};
  this.xpathPairs_ = [];
  this.document_ = d;
};
pagespeedutils.CriticalXPaths.prototype.getNonCriticalPanelXPathPairs = function() {
  this.findNonCriticalPanelBoundaries_(this.document_.body);
  return this.xpathPairs_;
};
pagespeedutils.CriticalXPaths.prototype.addXPathPair_ = function(a, c) {
  if (a) {
    var d = a;
    c && (d += ":" + c);
    this.xpathPairs_.push(d);
  }
};
pagespeedutils.CriticalXPaths.prototype.findNonCriticalPanelBoundaries_ = function(a) {
  for (var c = pagespeedutils.inViewport(a, this.windowSize_), d = c, b = "", e = "", f = a = this.visibleNodeOrSibling_(a.firstChild);null != f;f = this.visibleNodeOrSibling_(f.nextSibling)) {
    var g = this.findNonCriticalPanelBoundaries_(f);
    g != d && (g ? (c || (a != f && (b = pagespeedutils.generateXPath(a, this.document_)), c = !0), (e = pagespeedutils.generateXPath(f, this.document_)) || (b = ""), b && this.addXPathPair_(b, e), b = "") : (b = pagespeedutils.generateXPath(f, this.document_), e = ""), d = g);
  }
  b && this.addXPathPair_(b, e);
  return c;
};
pagespeedutils.generateXPath = function(a, c) {
  for (var d = [];a != c.body;) {
    var b = a.getAttribute("id");
    if (b && 1 == c.querySelectorAll("#" + b).length) {
      d.unshift(a.tagName.toLowerCase() + '[@id="' + a.getAttribute("id") + '"]');
      break;
    } else {
      for (var b = 0, e = a;e;e = e.previousElementSibling) {
        "SCRIPT" !== e.tagName && "NOSCRIPT" !== e.tagName && "STYLE" !== e.tagName && "LINK" !== e.tagName && ++b;
      }
      d.unshift(a.tagName.toLowerCase() + "[" + b + "]");
    }
    a = a.parentNode;
  }
  return d.length ? d.join("/") : "";
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
pagespeed.SplitHtmlBeacon = function(a, c, d, b) {
  this.btfNodes_ = [];
  this.beaconUrl_ = a;
  this.htmlUrl_ = c;
  this.optionsHash_ = d;
  this.nonce_ = b;
  this.windowSize_ = pagespeedutils.getWindowSize();
};
pagespeed.SplitHtmlBeacon.prototype.walkDom_ = function(a) {
  for (var c = !0, d = [], b = a.firstChild;null != b;b = b.nextSibling) {
    b.nodeType === Node.ELEMENT_NODE && "SCRIPT" != b.tagName && "NOSCRIPT" != b.tagName && "STYLE" != b.tagName && "LINK" != b.tagName && (this.walkDom_(b) ? d.push(b) : c = !1);
  }
  if (c && !pagespeedutils.inViewport(a, this.windowSize_)) {
    return !0;
  }
  for (a = 0;a < d.length;++a) {
    this.btfNodes_.push(pagespeedutils.generateXPath(d[a], window.document));
  }
  return !1;
};
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  this.walkDom_(document.body);
  if (0 != this.btfNodes_.length) {
    for (var a = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, a = a + ("&xp=" + encodeURIComponent(this.btfNodes_[0])), c = 1;c < this.btfNodes_.length;++c) {
      var d = "," + encodeURIComponent(this.btfNodes_[c]);
      if (a.length + d.length > pagespeedutils.MAX_POST_SIZE) {
        break;
      }
      a += d;
    }
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, a);
  }
};
pagespeed.splitHtmlBeaconInit = function(a, c, d, b) {
  var e = new pagespeed.SplitHtmlBeacon(a, c, d, b);
  pagespeedutils.addHandler(window, "load", function() {
    e.checkSplitHtml_();
  });
};
pagespeed.splitHtmlBeaconInit = pagespeed.splitHtmlBeaconInit;
})();
