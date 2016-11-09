(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, c) {
  var d;
  if (window.XMLHttpRequest) {
    d = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        d = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          d = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (h) {
        }
      }
    }
  }
  if (!d) {
    return !1;
  }
  var f = -1 == a.indexOf("?") ? "?" : "&";
  a = a + f + "url=" + encodeURIComponent(b);
  d.open("POST", a);
  d.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  d.send(c);
  return !0;
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
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.LocalStorageCache = function() {
  this.regenerate_cookie_ = !0;
};
pagespeed.LocalStorageCache.prototype.hasExpired = function(a) {
  a = parseInt(a.substring(0, a.indexOf(" ")), 10);
  return !isNaN(a) && a <= pagespeedutils.now();
};
pagespeed.LocalStorageCache.prototype.hasExpired = pagespeed.LocalStorageCache.prototype.hasExpired;
pagespeed.LocalStorageCache.prototype.getData = function(a) {
  var b = a.indexOf(" "), b = a.indexOf(" ", b + 1);
  return a.substring(b + 1);
};
pagespeed.LocalStorageCache.prototype.getData = pagespeed.LocalStorageCache.prototype.getData;
pagespeed.LocalStorageCache.prototype.replaceLastScript = function(a) {
  var b = document.getElementsByTagName("script"), b = b[b.length - 1];
  b.parentNode.replaceChild(a, b);
};
pagespeed.LocalStorageCache.prototype.replaceLastScript = pagespeed.LocalStorageCache.prototype.replaceLastScript;
pagespeed.LocalStorageCache.prototype.inlineCss = function(a) {
  var b = window.localStorage.getItem("pagespeed_lsc_url:" + a), c = document.createElement(b ? "style" : "link");
  b && !this.hasExpired(b) ? (c.type = "text/css", c.appendChild(document.createTextNode(this.getData(b)))) : (c.rel = "stylesheet", c.href = a, this.regenerate_cookie_ = !0);
  this.replaceLastScript(c);
};
pagespeed.LocalStorageCache.prototype.inlineCss = pagespeed.LocalStorageCache.prototype.inlineCss;
pagespeed.LocalStorageCache.prototype.inlineImg = function(a, b) {
  var c = window.localStorage.getItem("pagespeed_lsc_url:" + a + " pagespeed_lsc_hash:" + b), d = document.createElement("img");
  c && !this.hasExpired(c) ? d.src = this.getData(c) : (d.src = a, this.regenerate_cookie_ = !0);
  for (var c = 2, f = arguments.length;c < f;++c) {
    var e = arguments[c].indexOf("=");
    d.setAttribute(arguments[c].substring(0, e), arguments[c].substring(e + 1));
  }
  this.replaceLastScript(d);
};
pagespeed.LocalStorageCache.prototype.inlineImg = pagespeed.LocalStorageCache.prototype.inlineImg;
pagespeed.LocalStorageCache.prototype.processTags_ = function(a, b, c) {
  a = document.getElementsByTagName(a);
  for (var d = 0, f = a.length;d < f;++d) {
    var e = a[d], h = e.getAttribute("data-pagespeed-lsc-hash"), g = e.getAttribute("data-pagespeed-lsc-url");
    if (h && g) {
      g = "pagespeed_lsc_url:" + g;
      b && (g += " pagespeed_lsc_hash:" + h);
      var k = e.getAttribute("data-pagespeed-lsc-expiry"), k = k ? (new Date(k)).getTime() : "", e = c(e);
      if (!e) {
        var l = window.localStorage.getItem(g);
        l && (e = this.getData(l));
      }
      e && (window.localStorage.setItem(g, k + " " + h + " " + e), this.regenerate_cookie_ = !0);
    }
  }
};
pagespeed.LocalStorageCache.prototype.saveInlinedData_ = function() {
  this.processTags_("img", !0, function(a) {
    return a.src;
  });
  this.processTags_("style", !1, function(a) {
    return a.firstChild ? a.firstChild.nodeValue : null;
  });
};
pagespeed.LocalStorageCache.prototype.generateCookie_ = function() {
  if (this.regenerate_cookie_) {
    for (var a = [], b = [], c = 0, d = pagespeedutils.now(), f = 0, e = window.localStorage.length;f < e;++f) {
      var h = window.localStorage.key(f);
      if (!h.indexOf("pagespeed_lsc_url:")) {
        var g = window.localStorage.getItem(h), k = g.indexOf(" "), l = parseInt(g.substring(0, k), 10);
        if (!isNaN(l)) {
          if (l <= d) {
            a.push(h);
            continue;
          } else {
            if (l < c || 0 == c) {
              c = l;
            }
          }
        }
        h = g.indexOf(" ", k + 1);
        g = g.substring(k + 1, h);
        b.push(g);
      }
    }
    d = "";
    c && (d = "; expires=" + (new Date(c)).toUTCString());
    document.cookie = "_GPSLSC=" + b.join("!") + d;
    f = 0;
    for (e = a.length;f < e;++f) {
      window.localStorage.removeItem(a[f]);
    }
    this.regenerate_cookie_ = !1;
  }
};
pagespeed.localStorageCacheInit = function() {
  if (window.localStorage) {
    var a = new pagespeed.LocalStorageCache;
    pagespeed.localStorageCache = a;
    pagespeedutils.addHandler(window, "load", function() {
      a.saveInlinedData_();
    });
    pagespeedutils.addHandler(window, "load", function() {
      a.generateCookie_();
    });
  }
};
pagespeed.localStorageCacheInit = pagespeed.localStorageCacheInit;
})();
