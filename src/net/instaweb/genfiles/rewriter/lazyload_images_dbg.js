(function(){var pagespeedutils = {sendBeacon:function(beaconUrl, htmlUrl, data) {
  var httpRequest;
  if (window.XMLHttpRequest) {
    httpRequest = new XMLHttpRequest;
  } else {
    if (window.ActiveXObject) {
      try {
        httpRequest = new ActiveXObject("Msxml2.XMLHTTP");
      } catch (e) {
        try {
          httpRequest = new ActiveXObject("Microsoft.XMLHTTP");
        } catch (e2) {
        }
      }
    }
  }
  if (!httpRequest) {
    return!1;
  }
  var query_param_char = -1 == beaconUrl.indexOf("?") ? "?" : "&", url = beaconUrl + query_param_char + "url=" + encodeURIComponent(htmlUrl);
  httpRequest.open("POST", url);
  httpRequest.setRequestHeader("Content-Type", "application/x-www-form-urlencoded");
  httpRequest.send(data);
  return!0;
}, addHandler:function(elem, eventName, func) {
  if (elem.addEventListener) {
    elem.addEventListener(eventName, func, !1);
  } else {
    if (elem.attachEvent) {
      elem.attachEvent("on" + eventName, func);
    } else {
      var oldHandler = elem["on" + eventName];
      elem["on" + eventName] = function() {
        func.call(this);
        oldHandler && oldHandler.call(this);
      };
    }
  }
}, getPosition:function(element) {
  for (var top = element.offsetTop, left = element.offsetLeft;element.offsetParent;) {
    element = element.offsetParent, top += element.offsetTop, left += element.offsetLeft;
  }
  return{top:top, left:left};
}, getWindowSize:function() {
  var height = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight, width = window.innerWidth || document.documentElement.clientWidth || document.body.clientWidth;
  return{height:height, width:width};
}, inViewport:function(element, windowSize) {
  var position = pagespeedutils.getPosition(element);
  return pagespeedutils.positionInViewport(position, windowSize);
}, positionInViewport:function(pos, windowSize) {
  return pos.top < windowSize.height && pos.left < windowSize.width;
}};
window.pagespeed = window.pagespeed || {};
var pagespeed = window.pagespeed;
pagespeed.LazyloadImages = function(blankImageSrc) {
  this.deferred_ = [];
  this.buffer_ = 0;
  this.force_load_ = !1;
  this.blank_image_src_ = blankImageSrc;
  this.scroll_timer_ = null;
  this.last_scroll_time_ = 0;
  this.min_scroll_time_ = 200;
  this.onload_done_ = !1;
};
pagespeed.LazyloadImages.prototype.viewport_ = function() {
  var scrollY = 0;
  "number" == typeof window.pageYOffset ? scrollY = window.pageYOffset : document.body && document.body.scrollTop ? scrollY = document.body.scrollTop : document.documentElement && document.documentElement.scrollTop && (scrollY = document.documentElement.scrollTop);
  var height = window.innerHeight || document.documentElement.clientHeight || document.body.clientHeight;
  return{top:scrollY, bottom:scrollY + height, height:height};
};
pagespeed.LazyloadImages.prototype.compute_top_ = function(element) {
  var position = element.getAttribute("pagespeed_lazy_position");
  if (position) {
    return parseInt(position, 0);
  }
  var position = element.offsetTop, parent = element.offsetParent;
  parent && (position += this.compute_top_(parent));
  position = Math.max(position, 0);
  element.setAttribute("pagespeed_lazy_position", position);
  return position;
};
pagespeed.LazyloadImages.prototype.offset_ = function(element) {
  var top_position = this.compute_top_(element);
  return{top:top_position, bottom:top_position + element.offsetHeight};
};
pagespeed.LazyloadImages.prototype.getStyle_ = function(element, property) {
  if (element.currentStyle) {
    return element.currentStyle[property];
  }
  if (document.defaultView && document.defaultView.getComputedStyle) {
    var style = document.defaultView.getComputedStyle(element, null);
    if (style) {
      return style.getPropertyValue(property);
    }
  }
  return element.style && element.style[property] ? element.style[property] : "";
};
pagespeed.LazyloadImages.prototype.isVisible_ = function(element) {
  if (!this.onload_done_ && (0 == element.offsetHeight || 0 == element.offsetWidth)) {
    return!1;
  }
  var element_position = this.getStyle_(element, "position");
  if ("relative" == element_position) {
    return!0;
  }
  var viewport = this.viewport_(), rect = element.getBoundingClientRect(), top_diff, bottom_diff;
  if (rect) {
    top_diff = rect.top - viewport.height, bottom_diff = rect.bottom;
  } else {
    var position = this.offset_(element);
    top_diff = position.top - viewport.bottom;
    bottom_diff = position.bottom - viewport.top;
  }
  return top_diff <= this.buffer_ && 0 <= bottom_diff + this.buffer_;
};
pagespeed.LazyloadImages.prototype.loadIfVisible = function(element) {
  this.overrideAttributeFunctionsInternal_(element);
  var context = this;
  window.setTimeout(function() {
    var data_src = element.getAttribute("pagespeed_lazy_src");
    if (null != data_src) {
      if ((context.force_load_ || context.isVisible_(element)) && -1 != element.src.indexOf(context.blank_image_src_)) {
        var parent_node = element.parentNode, next_sibling = element.nextSibling;
        parent_node && parent_node.removeChild(element);
        element._getAttribute && (element.getAttribute = element._getAttribute);
        element.removeAttribute("onload");
        element.removeAttribute("pagespeed_lazy_src");
        element.removeAttribute("pagespeed_lazy_replaced_functions");
        parent_node && parent_node.insertBefore(element, next_sibling);
        element.src = data_src;
      } else {
        context.deferred_.push(element);
      }
    }
  }, 0);
};
pagespeed.LazyloadImages.prototype.loadIfVisible = pagespeed.LazyloadImages.prototype.loadIfVisible;
pagespeed.LazyloadImages.prototype.loadAllImages = function() {
  this.force_load_ = !0;
  this.loadVisible_();
};
pagespeed.LazyloadImages.prototype.loadAllImages = pagespeed.LazyloadImages.prototype.loadAllImages;
pagespeed.LazyloadImages.prototype.loadVisible_ = function() {
  var old_deferred = this.deferred_, len = old_deferred.length;
  this.deferred_ = [];
  for (var i = 0;i < len;++i) {
    this.loadIfVisible(old_deferred[i]);
  }
};
pagespeed.LazyloadImages.prototype.hasAttribute_ = function(element, attribute) {
  return element.getAttribute_ ? null != element.getAttribute_(attribute) : null != element.getAttribute(attribute);
};
pagespeed.LazyloadImages.prototype.overrideAttributeFunctions = function() {
  for (var images = document.getElementsByTagName("img"), i = 0;i < images.length;++i) {
    var element = images[i];
    this.hasAttribute_(element, "pagespeed_lazy_src") && this.overrideAttributeFunctionsInternal_(element);
  }
};
pagespeed.LazyloadImages.prototype.overrideAttributeFunctions = pagespeed.LazyloadImages.prototype.overrideAttributeFunctions;
pagespeed.LazyloadImages.prototype.overrideAttributeFunctionsInternal_ = function(element) {
  var context = this;
  this.hasAttribute_(element, "pagespeed_lazy_replaced_functions") || (element._getAttribute = element.getAttribute, element.getAttribute = function(name) {
    "src" == name.toLowerCase() && context.hasAttribute_(this, "pagespeed_lazy_src") && (name = "pagespeed_lazy_src");
    return this._getAttribute(name);
  }, element.setAttribute("pagespeed_lazy_replaced_functions", "1"));
};
pagespeed.lazyLoadInit = function(loadAfterOnload, blankImageSrc) {
  var context = new pagespeed.LazyloadImages(blankImageSrc);
  pagespeed.lazyLoadImages = context;
  var lazy_onload = function() {
    context.onload_done_ = !0;
    context.force_load_ = loadAfterOnload;
    context.buffer_ = 200;
    context.loadVisible_();
  };
  pagespeedutils.addHandler(window, "load", lazy_onload);
  0 != blankImageSrc.indexOf("data") && ((new Image).src = blankImageSrc);
  var lazy_onscroll = function() {
    if (!(context.onload_done_ && loadAfterOnload || context.scroll_timer_)) {
      var now = (new Date).getTime(), timeout_ms = context.min_scroll_time_;
      now - context.last_scroll_time_ > context.min_scroll_time_ && (timeout_ms = 0);
      context.scroll_timer_ = window.setTimeout(function() {
        context.last_scroll_time_ = (new Date).getTime();
        context.loadVisible_();
        context.scroll_timer_ = null;
      }, timeout_ms);
    }
  };
  pagespeedutils.addHandler(window, "scroll", lazy_onscroll);
  pagespeedutils.addHandler(window, "resize", lazy_onscroll);
};
pagespeed.lazyLoadInit = pagespeed.lazyLoadInit;
})();
