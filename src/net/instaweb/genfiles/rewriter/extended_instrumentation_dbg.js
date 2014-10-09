(function(){var pagespeedutils = {MAX_POST_SIZE:131072, sendBeacon:function(a, b, d) {
  var e;
  if (window.XMLHttpRequest) {
    e = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        e = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (l) {
        try {
          e = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (m) {
        }
      }
    }
  }
  if (!e) {
    return!1;
  }
  var k = -1 == a.indexOf("?") ? "?" : "&";
  a = a + k + "url=" + encodeURIComponent(b);
  e.open("POST", a);
  e.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  e.send(d);
  return!0;
}, addHandler:function(a, b, d) {
  if (a.addEventListener) {
    a.addEventListener(b, d, !1);
  } else {
    if (a.attachEvent) {
      a.attachEvent("on" + b, d);
    } else {
      var e = a["on" + b];
      a["on" + b] = function() {
        d.call(this);
        e && e.call(this);
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
pagespeed.getResourceTimingData = function() {
  if (window.performance && (window.performance.getEntries || window.performance.webkitGetEntries)) {
    for (var a = 0, b = 0, d = 0, e = 0, l = 0, m = 0, k = 0, q = 0, n = 0, r = 0, p = 0, g = {}, h = window.performance.getEntries ? window.performance.getEntries() : window.performance.webkitGetEntries(), f = 0;f < h.length;f++) {
      var c = h[f].duration;
      0 < c && (a += c, ++d, b = Math.max(b, c));
      c = h[f].connectEnd - h[f].connectStart;
      0 < c && (m += c, ++k);
      c = h[f].domainLookupEnd - h[f].domainLookupStart;
      0 < c && (e += c, ++l);
      c = h[f].initiatorType;
      g[c] ? ++g[c] : g[c] = 1;
      c = h[f].requestStart - h[f].fetchStart;
      0 < c && (r += c, ++p);
      c = h[f].responseStart - h[f].requestStart;
      0 < c && (q += c, ++n);
    }
    return "&afd=" + (d ? Math.round(a / d) : 0) + "&nfd=" + d + "&mfd=" + Math.round(b) + "&act=" + (k ? Math.round(m / k) : 0) + "&nct=" + k + "&adt=" + (l ? Math.round(e / l) : 0) + "&ndt=" + l + "&abt=" + (p ? Math.round(r / p) : 0) + "&nbt=" + p + "&attfb=" + (n ? Math.round(q / n) : 0) + "&nttfb=" + n + (g.css ? "&rit_css=" + g.css : "") + (g.link ? "&rit_link=" + g.link : "") + (g.script ? "&rit_script=" + g.script : "") + (g.img ? "&rit_img=" + g.img : "");
  }
  return "";
};
pagespeed.getResourceTimingData = pagespeed.getResourceTimingData;
})();
