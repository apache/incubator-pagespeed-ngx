(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, c) {
  var d;
  if (window.XMLHttpRequest) {
    d = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        d = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (f) {
        try {
          d = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (g) {
        }
      }
    }
  }
  if (!d) {
    return !1;
  }
  var e = -1 == a.indexOf("?") ? "?" : "&";
  a = a + e + "url=" + encodeURIComponent(b);
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
pagespeed.CriticalCssBeacon = function(a, b, c, d, e) {
  this.MAXITERS_ = 250;
  this.beaconUrl_ = a;
  this.htmlUrl_ = b;
  this.optionsHash_ = c;
  this.nonce_ = d;
  this.selectors_ = e;
  this.criticalSelectors_ = [];
  this.idx_ = 0;
};
pagespeed.CriticalCssBeacon.prototype.sendBeacon_ = function() {
  for (var a = "oh=" + this.optionsHash_ + "&n=" + this.nonce_, a = a + "&cs=", b = 0;b < this.criticalSelectors_.length;++b) {
    var c = 0 < b ? "," : "", c = c + encodeURIComponent(this.criticalSelectors_[b]);
    if (a.length + c.length > pagespeedutils.MAX_POST_SIZE) {
      break;
    }
    a += c;
  }
  pagespeed.criticalCssBeaconData = a;
  pagespeedutils.sendBeacon(this.beaconUrl_, this.htmlUrl_, a);
};
pagespeed.CriticalCssBeacon.prototype.checkCssSelectors_ = function(a) {
  for (var b = 0;b < this.MAXITERS_ && this.idx_ < this.selectors_.length;++b, ++this.idx_) {
    try {
      null != document.querySelector(this.selectors_[this.idx_]) && this.criticalSelectors_.push(this.selectors_[this.idx_]);
    } catch (c) {
    }
  }
  this.idx_ < this.selectors_.length ? window.setTimeout(this.checkCssSelectors_.bind(this), 0, a) : a();
};
pagespeed.criticalCssBeaconInit = function(a, b, c, d, e) {
  if (document.querySelector && Function.prototype.bind) {
    var f = new pagespeed.CriticalCssBeacon(a, b, c, d, e);
    pagespeedutils.addHandler(window, "load", function() {
      window.setTimeout(function() {
        f.checkCssSelectors_(function() {
          f.sendBeacon_();
        });
      }, 0);
    });
  }
};
pagespeed.criticalCssBeaconInit = pagespeed.criticalCssBeaconInit;
})();
