(function(){window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.getResourceTimingData = function() {
  if (window.performance && (window.performance.getEntries || window.performance.webkitGetEntries)) {
    for (var m = 0, l = 0, e = 0, n = 0, f = 0, p = 0, g = 0, q = 0, h = 0, r = 0, k = 0, c = {}, d = window.performance.getEntries ? window.performance.getEntries() : window.performance.webkitGetEntries(), b = 0;b < d.length;b++) {
      var a = d[b].duration;
      0 < a && (m += a, ++e, l = Math.max(l, a));
      a = d[b].connectEnd - d[b].connectStart;
      0 < a && (p += a, ++g);
      a = d[b].domainLookupEnd - d[b].domainLookupStart;
      0 < a && (n += a, ++f);
      a = d[b].initiatorType;
      c[a] ? ++c[a] : c[a] = 1;
      a = d[b].requestStart - d[b].fetchStart;
      0 < a && (r += a, ++k);
      a = d[b].responseStart - d[b].requestStart;
      0 < a && (q += a, ++h);
    }
    return "&afd=" + (e ? Math.round(m / e) : 0) + "&nfd=" + e + "&mfd=" + Math.round(l) + "&act=" + (g ? Math.round(p / g) : 0) + "&nct=" + g + "&adt=" + (f ? Math.round(n / f) : 0) + "&ndt=" + f + "&abt=" + (k ? Math.round(r / k) : 0) + "&nbt=" + k + "&attfb=" + (h ? Math.round(q / h) : 0) + "&nttfb=" + h + (c.css ? "&rit_css=" + c.css : "") + (c.link ? "&rit_link=" + c.link : "") + (c.script ? "&rit_script=" + c.script : "") + (c.img ? "&rit_img=" + c.img : "");
  }
  return "";
};
pagespeed.getResourceTimingData = pagespeed.getResourceTimingData;
})();
