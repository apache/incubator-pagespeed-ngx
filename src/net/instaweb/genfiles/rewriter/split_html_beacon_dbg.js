(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, d) {
  var c;
  if (window.XMLHttpRequest) {
    c = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        c = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          c = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (g) {
        }
      }
    }
  }
  if (!c) {
    return!1;
  }
  var f = -1 == a.indexOf("?") ? "?" : "&";
  a = a + f + "url=" + encodeURIComponent(b);
  c.open("POST", a);
  c.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  c.send(d);
  return!0;
}, addHandler:function(a, b, d) {
  if (a.addEventListener) {
    a.addEventListener(b, d, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + b, d);
    } else {
      var c = a["on" + b];
      a["on" + b] = function() {
        d.call(this);
        c && c.call(this);
      };
    }
  }
}, getPosition:function(a) {
  for (var b = a.offsetTop, d = a.offsetLeft;a.offsetParent;) {
    a = a.offsetParent, b += a.offsetTop, d += a.offsetLeft;
  }
  return{top:b, left:d};
}, getWindowSize:function() {
  return{height:window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width:window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth};
}, inViewport:function(a, b) {
  var d = pagespeedutils.getPosition(a);
  return pagespeedutils.positionInViewport(d, b);
}, positionInViewport:function(a, b) {
  return a.top < b.height && a.left < b.width;
}, getRequestAnimationFrame:function() {
  return window.requestAnimationFrame || window.webkitRequestAnimationFrame || window.mozRequestAnimationFrame || window.oRequestAnimationFrame || window.msRequestAnimationFrame || null;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.SplitHtmlBeacon = function(a, b, d, c) {
  this.btfNodes_ = [];
  this.beaconUrl_ = a;
  this.htmlUrl_ = b;
  this.optionsHash_ = d;
  this.nonce_ = c;
  this.windowSize_ = pagespeedutils.getWindowSize();
};
pagespeed.SplitHtmlBeacon.prototype.walkDom_ = function(a) {
  for (var b = !0, d = [], c = a.firstChild;null != c;c = c.nextSibling) {
    c.nodeType === Node.ELEMENT_NODE && "SCRIPT" != c.tagName && "NOSCRIPT" != c.tagName && "STYLE" != c.tagName && "LINK" != c.tagName && (this.walkDom_(c) ? d.push(c) : b = !1);
  }
  if (b && !pagespeedutils.inViewport(a, this.windowSize_)) {
    return!0;
  }
  for (a = 0;a < d.length;++a) {
    this.btfNodes_.push(pagespeedutils.generateXPath(d[a], window.document));
  }
  return!1;
};
pagespeed.SplitHtmlBeacon.prototype.checkSplitHtml_ = function() {
  this.walkDom_(document.body);
  if (0 != this.btfNodes_.length) {
    for (var a = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, a = a + ("&xp=" + encodeURIComponent(this.btfNodes_[0])), b = 1;b < this.btfNodes_.length;++b) {
      var d = "," + encodeURIComponent(this.btfNodes_[b]);
      if (a.length + d.length > pagespeedutils.MAX_POST_SIZE) {
        break;
      }
      a += d;
    }
    pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, a);
  }
};
pagespeed.splitHtmlBeaconInit = function(a, b, d, c) {
  var e = new pagespeed.SplitHtmlBeacon(a, b, d, c);
  pagespeedutils.addHandler(window, "load", function() {
    e.checkSplitHtml_();
  });
};
pagespeed.splitHtmlBeaconInit = pagespeed.splitHtmlBeaconInit;
})();
