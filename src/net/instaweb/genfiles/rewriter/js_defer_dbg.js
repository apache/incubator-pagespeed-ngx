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
pagespeed.deferJsNs = {};
var deferJsNs = pagespeed.deferJsNs;
deferJsNs.DeferJs = function() {
  this.queue_ = [];
  this.logs = [];
  this.dynamicInsertedScriptCount_ = this.next_ = 0;
  this.dynamicInsertedScript_ = [];
  this.documentWriteHtml_ = "";
  this.eventListernersMap_ = {};
  this.jsMimeTypes = "application/ecmascript application/javascript application/x-ecmascript application/x-javascript text/ecmascript text/javascript text/javascript1.0 text/javascript1.1 text/javascript1.2 text/javascript1.3 text/javascript1.4 text/javascript1.5 text/jscript text/livescript text/x-ecmascript text/x-javascript".split(" ");
  this.origGetElementById_ = document.getElementById;
  this.origGetElementsByTagName_ = document.getElementsByTagName;
  this.origDocWrite_ = document.write;
  this.origDocWriteln_ = document.writeln;
  this.origDocOpen_ = document.open;
  this.origDocClose_ = document.close;
  this.origDocAddEventListener_ = document.addEventListener;
  this.origWindowAddEventListener_ = window.addEventListener;
  this.origDocAttachEvent_ = document.attachEvent;
  this.origWindowAttachEvent_ = window.attachEvent;
  this.origCreateElement_ = document.createElement;
  this.state_ = deferJsNs.DeferJs.STATES.NOT_STARTED;
  this.eventState_ = deferJsNs.DeferJs.EVENT.NOT_STARTED;
  this.lastIncrementalRun_ = this.firstIncrementalRun_ = !0;
  this.incrementalScriptsDoneCallback_ = null;
  this.noDeferAsyncScriptsCount_ = 0;
  this.noDeferAsyncScripts_ = [];
  this.psaNotProcessed_ = this.psaScriptType_ = "";
  this.optLastIndex_ = -1;
};
deferJsNs.DeferJs.isExperimentalMode = !1;
deferJsNs.DeferJs.STATES = {NOT_STARTED:0, WAITING_FOR_NEXT_RUN:1, SCRIPTS_REGISTERED:2, SCRIPTS_EXECUTING:3, SYNC_SCRIPTS_DONE:4, WAITING_FOR_ONLOAD:5, SCRIPTS_DONE:6};
deferJsNs.DeferJs.EVENT = {NOT_STARTED:0, BEFORE_SCRIPTS:1, DOM_READY:2, LOAD:3, AFTER_SCRIPTS:4};
deferJsNs.DeferJs.PRIORITY_PSA_NOT_PROCESSED = "priority_psa_not_processed";
deferJsNs.DeferJs.PSA_NOT_PROCESSED = "psa_not_processed";
deferJsNs.DeferJs.PSA_CURRENT_NODE = "psa_current_node";
deferJsNs.DeferJs.PSA_TO_BE_DELETED = "psa_to_be_deleted";
deferJsNs.DeferJs.PSA_SCRIPT_TYPE = "text/psajs";
deferJsNs.DeferJs.PRIORITY_PSA_SCRIPT_TYPE = "text/prioritypsajs";
deferJsNs.DeferJs.PSA_ORIG_TYPE = "pagespeed_orig_type";
deferJsNs.DeferJs.PSA_ORIG_SRC = "pagespeed_orig_src";
deferJsNs.DeferJs.PSA_ORIG_INDEX = "orig_index";
deferJsNs.DeferJs.PAGESPEED_ONLOAD = "data-pagespeed-onload";
deferJsNs.DeferJs.prototype.log = function(line, opt_exception) {
  this.logs && (this.logs.push("" + line), opt_exception && (this.logs.push(opt_exception), "undefined" != typeof console && "undefined" != typeof console.log && console.log("PSA ERROR: " + line + opt_exception.message)));
};
deferJsNs.DeferJs.prototype.submitTask = function(task, opt_pos) {
  var pos = opt_pos ? opt_pos : this.queue_.length;
  this.queue_.splice(pos, 0, task);
};
deferJsNs.DeferJs.prototype.globalEval = function(str, opt_script_elem) {
  var script = this.cloneScriptNode(opt_script_elem);
  script.text = str;
  script.setAttribute("type", "text/javascript");
  var currentElem = this.getCurrentDomLocation();
  currentElem.parentNode.insertBefore(script, currentElem);
  return script;
};
deferJsNs.DeferJs.prototype.createIdVars = function() {
  for (var elems = document.getElementsByTagName("*"), idVarsString = "", i = 0;i < elems.length;i++) {
    if (elems[i].hasAttribute("id")) {
      var idStr = elems[i].getAttribute("id");
      if (idStr && -1 == idStr.search(/[-:.]/) && -1 == idStr.search(/^[0-9]/)) {
        try {
          window[idStr] && window[idStr].tagName && (idVarsString += "var " + idStr + '=document.getElementById("' + idStr + '");');
        } catch (err) {
          this.log("Exception while evaluating.", err);
        }
      }
    }
  }
  if (idVarsString) {
    var script = this.globalEval(idVarsString);
    script.setAttribute(deferJsNs.DeferJs.PSA_NOT_PROCESSED, "");
    script.setAttribute(deferJsNs.DeferJs.PRIORITY_PSA_NOT_PROCESSED, "");
  }
};
deferJsNs.DeferJs.prototype.attemptPrefetchOrQueue = function(url) {
  this.isWebKit() && ((new Image).src = url);
};
deferJsNs.DeferJs.prototype.addNode = function(script, opt_pos, opt_prefetch) {
  var src = script.getAttribute(deferJsNs.DeferJs.PSA_ORIG_SRC) || script.getAttribute("src");
  if (src) {
    opt_prefetch && this.attemptPrefetchOrQueue(src), this.addUrl(src, script, opt_pos);
  } else {
    var str = script.innerHTML || script.textContent || script.data || "";
    this.addStr(str, script, opt_pos);
  }
};
deferJsNs.DeferJs.prototype.addStr = function(str, script_elem, opt_pos) {
  if (this.isFireFox()) {
    this.addUrl("data:text/javascript," + encodeURIComponent(str), script_elem, opt_pos);
  } else {
    this.logs.push("Add to queue str: " + str);
    var me = this;
    this.submitTask(function() {
      me.removeNotProcessedAttributeTillNode(script_elem);
      var node = me.nextPsaJsNode();
      node.setAttribute(deferJsNs.DeferJs.PSA_CURRENT_NODE, "");
      try {
        me.globalEval(str, script_elem);
      } catch (err) {
        me.log("Exception while evaluating.", err);
      }
      me.log("Evaluated: " + str);
      me.runNext();
    }, opt_pos);
  }
};
deferJsNs.DeferJs.prototype.addStr = deferJsNs.DeferJs.prototype.addStr;
deferJsNs.DeferJs.prototype.cloneScriptNode = function(opt_script_elem) {
  var newScript = this.origCreateElement_.call(document, "script");
  if (opt_script_elem) {
    for (var a = opt_script_elem.attributes, n = a.length, i = n - 1;0 <= i;--i) {
      "type" != a[i].name && "src" != a[i].name && "async" != a[i].name && "defer" != a[i].name && a[i].name != deferJsNs.DeferJs.PSA_ORIG_TYPE && a[i].name != deferJsNs.DeferJs.PSA_ORIG_SRC && a[i].name != deferJsNs.DeferJs.PSA_ORIG_INDEX && a[i].name != deferJsNs.DeferJs.PSA_CURRENT_NODE && a[i].name != this.psaNotProcessed_ && (newScript.setAttribute(a[i].name, a[i].value), opt_script_elem.removeAttribute(a[i].name));
    }
  }
  return newScript;
};
deferJsNs.DeferJs.prototype.scriptOnLoad = function(url) {
  var script = this.origCreateElement_.call(document, "script");
  script.setAttribute("type", "text/javascript");
  script.async = !1;
  script.setAttribute(deferJsNs.DeferJs.PSA_TO_BE_DELETED, "");
  script.setAttribute(deferJsNs.DeferJs.PSA_NOT_PROCESSED, "");
  script.setAttribute(deferJsNs.DeferJs.PRIORITY_PSA_NOT_PROCESSED, "");
  var me = this, runNextHandler = function() {
    if (document.querySelector) {
      var node = document.querySelector("[" + deferJsNs.DeferJs.PSA_TO_BE_DELETED + "]");
      node && node.parentNode.removeChild(node);
    }
    me.log("Executed: " + url);
    me.runNext();
  };
  deferJsNs.addOnload(script, runNextHandler);
  pagespeedutils.addHandler(script, "error", runNextHandler);
  script.src = "data:text/javascript," + encodeURIComponent("window.pagespeed.psatemp=0;");
  var currentElem = this.getCurrentDomLocation();
  currentElem.parentNode.insertBefore(script, currentElem);
};
deferJsNs.DeferJs.prototype.addUrl = function(url, script_elem, opt_pos) {
  this.logs.push("Add to queue url: " + url);
  var me = this;
  this.submitTask(function() {
    me.removeNotProcessedAttributeTillNode(script_elem);
    var script = me.cloneScriptNode(script_elem);
    script.setAttribute("type", "text/javascript");
    var useSyncScript = !1;
    if ("async" in script) {
      useSyncScript = !0, script.async = !1;
    } else {
      if (script.readyState) {
        var stateChangeHandler = function() {
          if ("complete" == script.readyState || "loaded" == script.readyState) {
            script.onreadystatechange = null, me.log("Executed: " + url), me.runNext();
          }
        };
        pagespeedutils.addHandler(script, "readystatechange", stateChangeHandler);
      }
    }
    script.setAttribute("src", url);
    var str = script_elem.innerHTML || script_elem.textContent || script_elem.data;
    str && script.appendChild(document.createTextNode(str));
    var currentElem = me.nextPsaJsNode();
    currentElem.setAttribute(deferJsNs.DeferJs.PSA_CURRENT_NODE, "");
    currentElem.parentNode.insertBefore(script, currentElem);
    useSyncScript && me.scriptOnLoad(url);
  }, opt_pos);
};
deferJsNs.DeferJs.prototype.addUrl = deferJsNs.DeferJs.prototype.addUrl;
deferJsNs.DeferJs.prototype.removeNotProcessedAttributeTillNode = function(opt_node) {
  if (document.querySelectorAll && !(8 >= this.getIEVersion())) {
    for (var nodes = document.querySelectorAll("[" + this.psaNotProcessed_ + "]"), i = 0;i < nodes.length;i++) {
      var dom_node = nodes.item(i);
      if (dom_node == opt_node) {
        break;
      }
      dom_node.getAttribute("type") != this.psaScriptType_ && dom_node.removeAttribute(this.psaNotProcessed_);
    }
  }
};
deferJsNs.DeferJs.prototype.setNotProcessedAttributeForNodes = function() {
  for (var nodes = this.origGetElementsByTagName_.call(document, "*"), i = 0;i < nodes.length;i++) {
    var dom_node = nodes.item(i);
    dom_node.setAttribute(this.psaNotProcessed_, "");
  }
};
deferJsNs.DeferJs.prototype.nextPsaJsNode = function() {
  var current_node = null;
  document.querySelector && (current_node = document.querySelector('[type="' + this.psaScriptType_ + '"]'));
  return current_node;
};
deferJsNs.DeferJs.prototype.getCurrentDomLocation = function() {
  var current_node;
  document.querySelector && (current_node = document.querySelector("[" + deferJsNs.DeferJs.PSA_CURRENT_NODE + "]"));
  return current_node || this.origGetElementsByTagName_.call(document, "psanode")[0];
};
deferJsNs.DeferJs.prototype.removeCurrentDomLocation = function() {
  var oldNode = this.getCurrentDomLocation();
  "SCRIPT" == oldNode.nodeName && oldNode.parentNode.removeChild(oldNode);
};
deferJsNs.DeferJs.prototype.onComplete = function() {
  if (!(this.state_ >= deferJsNs.DeferJs.STATES.WAITING_FOR_ONLOAD)) {
    if (this.lastIncrementalRun_ && this.isLowPriorityDeferJs() && (this.getIEVersion() && document.documentElement.originalDoScroll && (document.documentElement.doScroll = document.documentElement.originalDoScroll), Object.defineProperty && delete document.readyState, this.getIEVersion() && Object.defineProperty && delete document.all), document.getElementById = this.origGetElementById_, !document.querySelectorAll || 8 >= this.getIEVersion() || (document.getElementsByTagName = this.origGetElementsByTagName_), 
    document.createElement = this.origCreateElement_, document.open = this.origDocOpen_, document.close = this.origDocClose_, document.write = this.origDocWrite_, document.writeln = this.origDocWriteln_, this.lastIncrementalRun_) {
      if (this.state_ = deferJsNs.DeferJs.STATES.WAITING_FOR_ONLOAD, this.restoreAddEventListeners(), this.isLowPriorityDeferJs()) {
        var me = this;
        "complete" != document.readyState ? deferJsNs.addOnload(window, function() {
          me.fireOnload();
        }) : (document.onreadystatechange && this.exec(document.onreadystatechange, document), window.onload && (psaAddEventListener(window, "onload", window.onload), window.onload = null), this.fireOnload());
      } else {
        this.highPriorityFinalize();
      }
    } else {
      this.state_ = deferJsNs.DeferJs.STATES.WAITING_FOR_NEXT_RUN, this.firstIncrementalRun_ = !1, this.incrementalScriptsDoneCallback_ && this.isLowPriorityDeferJs() ? (pagespeed.deferJs = pagespeed.highPriorityDeferJs, pagespeed.deferJs = pagespeed.highPriorityDeferJs, this.exec(this.incrementalScriptsDoneCallback_), this.incrementalScriptsDoneCallback_ = null) : this.highPriorityFinalize();
    }
  }
};
deferJsNs.DeferJs.prototype.fireOnload = function() {
  this.isLowPriorityDeferJs() && (this.addDeferredOnloadListeners(), pagespeed.highPriorityDeferJs.fireOnload());
  this.fireEvent(deferJsNs.DeferJs.EVENT.LOAD);
  if (this.isLowPriorityDeferJs()) {
    for (var psanodes = document.body.getElementsByTagName("psanode"), i = psanodes.length - 1;0 <= i;i--) {
      document.body.removeChild(psanodes[i]);
    }
    for (var prefetchContainers = document.body.getElementsByClassName("psa_prefetch_container"), i = prefetchContainers.length - 1;0 <= i;i--) {
      prefetchContainers[i].parentNode.removeChild(prefetchContainers[i]);
    }
  }
  this.state_ = deferJsNs.DeferJs.STATES.SCRIPTS_DONE;
  this.fireEvent(deferJsNs.DeferJs.EVENT.AFTER_SCRIPTS);
};
deferJsNs.DeferJs.prototype.highPriorityFinalize = function() {
  var me = this;
  window.setTimeout(function() {
    pagespeed.deferJs = pagespeed.lowPriorityDeferJs;
    pagespeed.deferJs = pagespeed.lowPriorityDeferJs;
    me.incrementalScriptsDoneCallback_ ? (pagespeed.deferJs.registerScriptTags(me.incrementalScriptsDoneCallback_, me.optLastIndex_), me.incrementalScriptsDoneCallback_ = null) : pagespeed.deferJs.registerScriptTags();
    pagespeed.deferJs.execute();
  }, 0);
};
deferJsNs.DeferJs.prototype.checkNodeInDom = function(node) {
  for (;node = node.parentNode;) {
    if (node == document) {
      return!0;
    }
  }
  return!1;
};
deferJsNs.DeferJs.prototype.getNumScriptsWithNoOnload = function(dynamicInsertedScriptList) {
  for (var count = 0, len = dynamicInsertedScriptList.length, i = 0;i < len;++i) {
    var node = dynamicInsertedScriptList[i], parent = node.parentNode, src = node.src, text = node.textContent;
    (8 < this.getIEVersion() && (!parent || "" == src && "" == text) || !(this.getIEVersion() || this.checkNodeInDom(node) && "" != src)) && count++;
  }
  return count;
};
deferJsNs.DeferJs.prototype.canCallOnComplete = function() {
  if (this.state_ != deferJsNs.DeferJs.STATES.SYNC_SCRIPTS_DONE) {
    return!1;
  }
  var count = 0;
  0 != this.dynamicInsertedScriptCount_ && (count = this.getNumScriptsWithNoOnload(this.dynamicInsertedScript_));
  return this.dynamicInsertedScriptCount_ == count ? !0 : !1;
};
deferJsNs.DeferJs.prototype.scriptsAreDone = function() {
  return this.state_ === deferJsNs.DeferJs.STATES.SCRIPTS_DONE;
};
deferJsNs.DeferJs.prototype.scriptsAreDone = deferJsNs.DeferJs.prototype.scriptsAreDone;
deferJsNs.DeferJs.prototype.runNext = function() {
  this.handlePendingDocumentWrites();
  this.removeCurrentDomLocation();
  if (this.next_ < this.queue_.length) {
    this.next_++, this.queue_[this.next_ - 1].call(window);
  } else {
    if (this.lastIncrementalRun_) {
      if (this.state_ = deferJsNs.DeferJs.STATES.SYNC_SCRIPTS_DONE, this.removeNotProcessedAttributeTillNode(), this.fireEvent(deferJsNs.DeferJs.EVENT.DOM_READY), this.canCallOnComplete()) {
        this.onComplete();
      }
    } else {
      this.onComplete();
    }
  }
};
deferJsNs.DeferJs.prototype.nodeListToArray = function(nodeList) {
  for (var arr = [], len = nodeList.length, i = 0;i < len;++i) {
    arr.push(nodeList.item(i));
  }
  return arr;
};
deferJsNs.DeferJs.prototype.setUp = function() {
  var me = this;
  if (this.firstIncrementalRun_ && !this.isLowPriorityDeferJs()) {
    var initialContextNode = document.createElement("psanode");
    initialContextNode.setAttribute("psa_dw_target", "true");
    document.body.appendChild(initialContextNode);
    this.getIEVersion() && this.createIdVars();
    if (Object.defineProperty) {
      try {
        var propertyDescriptor = {configurable:!0, get:function() {
          return me.state_ >= deferJsNs.DeferJs.STATES.SYNC_SCRIPTS_DONE ? "interactive" : "loading";
        }};
        Object.defineProperty(document, "readyState", propertyDescriptor);
      } catch (err$$0) {
        this.log("Exception while overriding document.readyState.", err$$0);
      }
    }
    if (this.getIEVersion() && (document.documentElement.originalDoScroll = document.documentElement.doScroll, document.documentElement.doScroll = function() {
      throw "psa exception";
    }, Object.defineProperty)) {
      try {
        propertyDescriptor = {configurable:!0, get:function() {
        }}, Object.defineProperty(document, "all", propertyDescriptor);
      } catch (err$$1) {
        this.log("Exception while overriding document.all.", err$$1);
      }
    }
  }
  this.overrideAddEventListeners();
  document.writeln = function(x) {
    me.writeHtml(x + "\n");
  };
  document.write = function(x) {
    me.writeHtml(x);
  };
  document.open = function() {
  };
  document.close = function() {
  };
  document.getElementById = function(str) {
    me.handlePendingDocumentWrites();
    var node = me.origGetElementById_.call(document, str);
    return null == node || node.hasAttribute(me.psaNotProcessed_) ? null : node;
  };
  !document.querySelectorAll || 8 >= me.getIEVersion() || (document.getElementsByTagName = function(tagName) {
    try {
      return document.querySelectorAll(tagName + ":not([" + me.psaNotProcessed_ + "])");
    } catch (err) {
      return me.origGetElementsByTagName_.call(document, tagName);
    }
  });
  document.createElement = function(str) {
    var elem = me.origCreateElement_.call(document, str);
    if ("script" == str.toLowerCase()) {
      me.dynamicInsertedScript_.push(elem);
      me.dynamicInsertedScriptCount_++;
      var onload = function() {
        me.dynamicInsertedScriptCount_--;
        var index = me.dynamicInsertedScript_.indexOf(this);
        if (-1 != index && (me.dynamicInsertedScript_.splice(index, 1), me.canCallOnComplete())) {
          me.onComplete();
        }
      };
      deferJsNs.addOnload(elem, onload);
      pagespeedutils.addHandler(elem, "error", onload);
    }
    return elem;
  };
};
deferJsNs.DeferJs.prototype.execute = function() {
  if (this.state_ == deferJsNs.DeferJs.STATES.SCRIPTS_REGISTERED) {
    var count = 0;
    0 != this.noDeferAsyncScriptsCount_ && (count = this.getNumScriptsWithNoOnload(this.noDeferAsyncScripts_));
    this.noDeferAsyncScriptsCount_ == count && (document.createElement = this.origCreateElement_, this.run());
  }
};
deferJsNs.DeferJs.prototype.execute = deferJsNs.DeferJs.prototype.execute;
deferJsNs.DeferJs.prototype.run = function() {
  this.state_ == deferJsNs.DeferJs.STATES.SCRIPTS_REGISTERED && (this.firstIncrementalRun_ && this.fireEvent(deferJsNs.DeferJs.EVENT.BEFORE_SCRIPTS), this.state_ = deferJsNs.DeferJs.STATES.SCRIPTS_EXECUTING, this.setUp(), this.runNext());
};
deferJsNs.DeferJs.prototype.run = deferJsNs.DeferJs.prototype.run;
deferJsNs.DeferJs.prototype.parseHtml = function(html) {
  var div = this.origCreateElement_.call(document, "div");
  div.innerHTML = "<div>_</div>" + html;
  div.removeChild(div.firstChild);
  return div;
};
deferJsNs.DeferJs.prototype.disown = function(node) {
  var parentNode = node.parentNode;
  parentNode && parentNode.removeChild(node);
};
deferJsNs.DeferJs.prototype.insertNodesBeforeElem = function(nodes, elem) {
  for (var nodeArray = this.nodeListToArray(nodes), len = nodeArray.length, parentNode = elem.parentNode, i = 0;i < len;++i) {
    var node = nodeArray[i];
    this.disown(node);
    parentNode.insertBefore(node, elem);
  }
};
deferJsNs.DeferJs.prototype.isJSNode = function(node) {
  if ("SCRIPT" != node.nodeName) {
    return!1;
  }
  if (node.hasAttribute("type")) {
    var type = node.getAttribute("type");
    return!type || -1 != this.jsMimeTypes.indexOf(type);
  }
  if (node.hasAttribute("language")) {
    var lang = node.getAttribute("language");
    return!lang || -1 != this.jsMimeTypes.indexOf("text/" + lang.toLowerCase());
  }
  return!0;
};
deferJsNs.DeferJs.prototype.markNodesAndExtractScriptNodes = function(node, scriptNodes) {
  if (node.childNodes) {
    for (var nodeArray = this.nodeListToArray(node.childNodes), len = nodeArray.length, i = 0;i < len;++i) {
      var child = nodeArray[i];
      "SCRIPT" == child.nodeName ? this.isJSNode(child) && (scriptNodes.push(child), child.setAttribute(deferJsNs.DeferJs.PSA_ORIG_TYPE, child.type), child.setAttribute("type", this.psaScriptType_), child.setAttribute(deferJsNs.DeferJs.PSA_ORIG_SRC, child.src), child.setAttribute("src", ""), child.setAttribute(this.psaNotProcessed_, "")) : this.markNodesAndExtractScriptNodes(child, scriptNodes);
    }
  }
};
deferJsNs.DeferJs.prototype.deferScripts = function(scripts, pos) {
  for (var len = scripts.length, i = 0;i < len;++i) {
    this.addNode(scripts[i], pos + i, !!i);
  }
};
deferJsNs.DeferJs.prototype.insertHtml = function(html, pos, opt_elem) {
  var node = this.parseHtml(html), scriptNodes = [];
  this.markNodesAndExtractScriptNodes(node, scriptNodes);
  opt_elem ? this.insertNodesBeforeElem(node.childNodes, opt_elem) : this.log("Unable to insert nodes, no context element found");
  this.deferScripts(scriptNodes, pos);
};
deferJsNs.DeferJs.prototype.handlePendingDocumentWrites = function() {
  if ("" != this.documentWriteHtml_) {
    this.log("handle_dw: " + this.documentWriteHtml_);
    var html = this.documentWriteHtml_;
    this.documentWriteHtml_ = "";
    var currentElem = this.getCurrentDomLocation();
    this.insertHtml(html, this.next_, currentElem);
  }
};
deferJsNs.DeferJs.prototype.writeHtml = function(html) {
  this.log("dw: " + html);
  this.documentWriteHtml_ += html;
};
deferJsNs.DeferJs.prototype.addDeferredOnloadListeners = function() {
  var onloadDeferredElements;
  document.querySelectorAll && (onloadDeferredElements = document.querySelectorAll("[" + deferJsNs.DeferJs.PAGESPEED_ONLOAD + "][data-pagespeed-loaded]"));
  for (var i = 0;i < onloadDeferredElements.length;i++) {
    var elem = onloadDeferredElements.item(i), handlerStr = elem.getAttribute(deferJsNs.DeferJs.PAGESPEED_ONLOAD), functionStr = "var psaFunc=function() {" + handlerStr + "};";
    window.eval.call(window, functionStr);
    "function" != typeof window.psaFunc ? this.log("Function is not defined", Error("")) : psaAddEventListener(elem, "onload", window.psaFunc);
  }
};
deferJsNs.DeferJs.prototype.addBeforeDeferRunFunctions = function(func) {
  psaAddEventListener(window, "onbeforescripts", func);
};
deferJsNs.DeferJs.prototype.addBeforeDeferRunFunctions = deferJsNs.DeferJs.prototype.addBeforeDeferRunFunctions;
deferJsNs.DeferJs.prototype.addAfterDeferRunFunctions = function(func) {
  psaAddEventListener(window, "onafterscripts", func);
};
deferJsNs.DeferJs.prototype.addAfterDeferRunFunctions = deferJsNs.DeferJs.prototype.addAfterDeferRunFunctions;
deferJsNs.DeferJs.prototype.fireEvent = function(evt) {
  this.eventState_ = evt;
  this.log("Firing Event: " + evt);
  for (var eventListeners = this.eventListernersMap_[evt] || [], i = 0;i < eventListeners.length;++i) {
    this.exec(eventListeners[i]);
  }
  eventListeners.length = 0;
};
deferJsNs.DeferJs.prototype.exec = function(func, opt_scopeObject) {
  try {
    func.call(opt_scopeObject || window);
  } catch (err) {
    this.log("Exception while evaluating.", err);
  }
};
deferJsNs.DeferJs.prototype.overrideAddEventListeners = function() {
  var me = this;
  window.addEventListener ? (document.addEventListener = function(eventName, func, capture) {
    psaAddEventListener(document, eventName, func, capture, me.origDocAddEventListener_);
  }, window.addEventListener = function(eventName, func, capture) {
    psaAddEventListener(window, eventName, func, capture, me.origWindowAddEventListener_);
  }) : window.attachEvent && (document.attachEvent = function(eventName, func) {
    psaAddEventListener(document, eventName, func, void 0, me.origDocAttachEvent_);
  }, window.attachEvent = function(eventName, func) {
    psaAddEventListener(window, eventName, func, void 0, me.origWindowAttachEvent_);
  });
};
deferJsNs.DeferJs.prototype.restoreAddEventListeners = function() {
  window.addEventListener ? (document.addEventListener = this.origDocAddEventListener_, window.addEventListener = this.origWindowAddEventListener_) : window.attachEvent && (document.attachEvent = this.origDocAttachEvent_, window.attachEvent = this.origWindowAttachEvent_);
};
var psaAddEventListener = function(elem, eventName, func, opt_capture, opt_originalAddEventListener) {
  var deferJs = pagespeed.deferJs;
  if (!(deferJs.state_ >= deferJsNs.DeferJs.STATES.SCRIPTS_DONE)) {
    var deferJsEvent, deferJsEventName;
    if (deferJs.eventState_ < deferJsNs.DeferJs.EVENT.DOM_READY && ("DOMContentLoaded" == eventName || "readystatechange" == eventName || "onDOMContentLoaded" == eventName || "onreadystatechange" == eventName)) {
      deferJsEvent = deferJsNs.DeferJs.EVENT.DOM_READY, deferJsEventName = "DOMContentLoaded";
    } else {
      if (deferJs.eventState_ < deferJsNs.DeferJs.EVENT.LOAD && ("load" == eventName || "onload" == eventName)) {
        deferJsEvent = deferJsNs.DeferJs.EVENT.LOAD, deferJsEventName = "load";
      } else {
        if ("onbeforescripts" == eventName) {
          deferJsEvent = deferJsNs.DeferJs.EVENT.BEFORE_SCRIPTS;
        } else {
          if ("onafterscripts" == eventName) {
            deferJsEvent = deferJsNs.DeferJs.EVENT.AFTER_SCRIPTS;
          } else {
            opt_originalAddEventListener && opt_originalAddEventListener.call(elem, eventName, func, opt_capture);
            return;
          }
        }
      }
    }
    var eventListenerClosure = function() {
      var customEvent = {bubbles:!1, cancelable:!1, eventPhase:2};
      customEvent.timeStamp = (new Date).getTime();
      customEvent.type = deferJsEventName;
      customEvent.target = elem != window ? elem : document;
      customEvent.currentTarget = elem;
      func.call(elem, customEvent);
    };
    deferJs.eventListernersMap_[deferJsEvent] || (deferJs.eventListernersMap_[deferJsEvent] = []);
    deferJs.eventListernersMap_[deferJsEvent].push(eventListenerClosure);
  }
};
deferJsNs.DeferJs.prototype.registerScriptTags = function(opt_callback, opt_last_index) {
  if (!(this.state_ >= deferJsNs.DeferJs.STATES.SCRIPTS_REGISTERED)) {
    if (opt_callback) {
      if (!deferJsNs.DeferJs.isExperimentalMode) {
        opt_callback();
        return;
      }
      this.lastIncrementalRun_ = !1;
      this.incrementalScriptsDoneCallback_ = opt_callback;
      opt_last_index && (this.optLastIndex_ = opt_last_index);
    } else {
      this.lastIncrementalRun_ = !0;
    }
    this.state_ = deferJsNs.DeferJs.STATES.SCRIPTS_REGISTERED;
    for (var scripts = document.getElementsByTagName("script"), len = scripts.length, i = 0;i < len;++i) {
      var isFirstScript = this.queue_.length == this.next_, script = scripts[i];
      if (script.getAttribute("type") == this.psaScriptType_) {
        if (opt_callback) {
          var scriptIndex = script.getAttribute(deferJsNs.DeferJs.PSA_ORIG_INDEX);
          scriptIndex <= this.optLastIndex_ && this.addNode(script, void 0, !isFirstScript);
        } else {
          script.getAttribute(deferJsNs.DeferJs.PSA_ORIG_INDEX) < this.optLastIndex_ && this.log("Executing a script twice. Orig_Index: " + script.getAttribute(deferJsNs.DeferJs.PSA_ORIG_INDEX), Error("")), this.addNode(script, void 0, !isFirstScript);
        }
      }
    }
  }
};
deferJsNs.DeferJs.prototype.registerScriptTags = deferJsNs.DeferJs.prototype.registerScriptTags;
deferJsNs.addOnload = function(elem, func) {
  pagespeedutils.addHandler(elem, "load", func);
};
pagespeed.addOnload = deferJsNs.addOnload;
deferJsNs.DeferJs.prototype.isFireFox = function() {
  return-1 != navigator.userAgent.indexOf("Firefox");
};
deferJsNs.DeferJs.prototype.isWebKit = function() {
  return-1 != navigator.userAgent.indexOf("AppleWebKit");
};
deferJsNs.DeferJs.prototype.getIEVersion = function() {
  var version = /(?:MSIE.(\d+\.\d+))/.exec(navigator.userAgent);
  return version && version[1] ? document.documentMode || parseFloat(version[1]) : NaN;
};
deferJsNs.DeferJs.prototype.setType = function(type) {
  this.psaScriptType_ = type;
};
deferJsNs.DeferJs.prototype.setPsaNotProcessed = function(psaNotProcessed) {
  this.psaNotProcessed_ = psaNotProcessed;
};
deferJsNs.DeferJs.prototype.isLowPriorityDeferJs = function() {
  return this.psaScriptType_ == deferJsNs.DeferJs.PSA_SCRIPT_TYPE ? !0 : !1;
};
deferJsNs.DeferJs.prototype.noDeferCreateElementOverride = function() {
  var me = this;
  document.createElement = function(str) {
    var elem = me.origCreateElement_.call(document, str);
    if ("script" == str.toLowerCase()) {
      me.noDeferAsyncScripts_.push(elem);
      me.noDeferAsyncScriptsCount_++;
      var onload = function() {
        var index = me.noDeferAsyncScripts_.indexOf(this);
        -1 != index && (me.noDeferAsyncScripts_.splice(index, 1), me.noDeferAsyncScriptsCount_--, me.execute());
      };
      deferJsNs.addOnload(elem, onload);
      pagespeedutils.addHandler(elem, "error", onload);
    }
    return elem;
  };
};
deferJsNs.DeferJs.prototype.isExperimentalMode = function() {
  return deferJsNs.DeferJs.isExperimentalMode;
};
deferJsNs.DeferJs.prototype.isExperimentalMode = deferJsNs.DeferJs.prototype.isExperimentalMode;
deferJsNs.deferInit = function() {
  pagespeed.deferJs || (deferJsNs.DeferJs.isExperimentalMode = pagespeed.defer_js_experimental, pagespeed.highPriorityDeferJs = new deferJsNs.DeferJs, pagespeed.highPriorityDeferJs.setType(deferJsNs.DeferJs.PRIORITY_PSA_SCRIPT_TYPE), pagespeed.highPriorityDeferJs.setPsaNotProcessed(deferJsNs.DeferJs.PRIORITY_PSA_NOT_PROCESSED), pagespeed.highPriorityDeferJs.setNotProcessedAttributeForNodes(), pagespeed.lowPriorityDeferJs = new deferJsNs.DeferJs, pagespeed.lowPriorityDeferJs.setType(deferJsNs.DeferJs.PSA_SCRIPT_TYPE), 
  pagespeed.lowPriorityDeferJs.setPsaNotProcessed(deferJsNs.DeferJs.PSA_NOT_PROCESSED), pagespeed.lowPriorityDeferJs.setNotProcessedAttributeForNodes(), pagespeed.deferJs = pagespeed.highPriorityDeferJs, pagespeed.deferJs.noDeferCreateElementOverride(), pagespeed.deferJs = pagespeed.deferJs);
};
deferJsNs.deferInit();
pagespeed.deferJsStarted = !1;
deferJsNs.startDeferJs = function() {
  pagespeed.deferJsStarted || pagespeed.panelLoader || (pagespeed.deferJsStarted = !0, pagespeed.deferJs.registerScriptTags(), pagespeed.deferJs.execute());
};
deferJsNs.startDeferJs = deferJsNs.startDeferJs;
pagespeedutils.addHandler(document, "DOMContentLoaded", deferJsNs.startDeferJs);
deferJsNs.addOnload(window, deferJsNs.startDeferJs);
})();
