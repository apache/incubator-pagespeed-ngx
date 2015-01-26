(function(){var COMPILED = !0, goog = goog || {};
goog.global = this;
goog.isDef = function(a) {
  return void 0 !== a;
};
goog.exportPath_ = function(a, b, c) {
  a = a.split(".");
  c = c || goog.global;
  a[0] in c || !c.execScript || c.execScript("var " + a[0]);
  for (var d;a.length && (d = a.shift());) {
    !a.length && goog.isDef(b) ? c[d] = b : c = c[d] ? c[d] : c[d] = {};
  }
};
goog.define = function(a, b) {
  var c = b;
  COMPILED || (goog.global.CLOSURE_UNCOMPILED_DEFINES && Object.prototype.hasOwnProperty.call(goog.global.CLOSURE_UNCOMPILED_DEFINES, a) ? c = goog.global.CLOSURE_UNCOMPILED_DEFINES[a] : goog.global.CLOSURE_DEFINES && Object.prototype.hasOwnProperty.call(goog.global.CLOSURE_DEFINES, a) && (c = goog.global.CLOSURE_DEFINES[a]));
  goog.exportPath_(a, c);
};
goog.DEBUG = !0;
goog.LOCALE = "en";
goog.TRUSTED_SITE = !0;
goog.STRICT_MODE_COMPATIBLE = !1;
goog.provide = function(a) {
  if (!COMPILED && goog.isProvided_(a)) {
    throw Error('Namespace "' + a + '" already declared.');
  }
  goog.constructNamespace_(a);
};
goog.constructNamespace_ = function(a, b) {
  if (!COMPILED) {
    delete goog.implicitNamespaces_[a];
    for (var c = a;(c = c.substring(0, c.lastIndexOf("."))) && !goog.getObjectByName(c);) {
      goog.implicitNamespaces_[c] = !0;
    }
  }
  goog.exportPath_(a, b);
};
goog.module = function(a) {
  if (!goog.isString(a) || !a) {
    throw Error("Invalid module identifier");
  }
  if (!goog.isInModuleLoader_()) {
    throw Error("Module " + a + " has been loaded incorrectly.");
  }
  if (goog.moduleLoaderState_.moduleName) {
    throw Error("goog.module may only be called once per module.");
  }
  goog.moduleLoaderState_.moduleName = a;
  if (!COMPILED) {
    if (goog.isProvided_(a)) {
      throw Error('Namespace "' + a + '" already declared.');
    }
    delete goog.implicitNamespaces_[a];
  }
};
goog.module.get = function(a) {
  return goog.module.getInternal_(a);
};
goog.module.getInternal_ = function(a) {
  if (!COMPILED) {
    return goog.isProvided_(a) ? a in goog.loadedModules_ ? goog.loadedModules_[a] : goog.getObjectByName(a) : null;
  }
};
goog.moduleLoaderState_ = null;
goog.isInModuleLoader_ = function() {
  return null != goog.moduleLoaderState_;
};
goog.module.declareTestMethods = function() {
  if (!goog.isInModuleLoader_()) {
    throw Error("goog.module.declareTestMethods must be called from within a goog.module");
  }
  goog.moduleLoaderState_.declareTestMethods = !0;
};
goog.module.declareLegacyNamespace = function() {
  if (!COMPILED && !goog.isInModuleLoader_()) {
    throw Error("goog.module.declareLegacyNamespace must be called from within a goog.module");
  }
  if (!COMPILED && !goog.moduleLoaderState_.moduleName) {
    throw Error("goog.module must be called prior to goog.module.declareLegacyNamespace.");
  }
  goog.moduleLoaderState_.declareLegacyNamespace = !0;
};
goog.setTestOnly = function(a) {
  if (COMPILED && !goog.DEBUG) {
    throw a = a || "", Error("Importing test-only code into non-debug environment" + (a ? ": " + a : "."));
  }
};
goog.forwardDeclare = function(a) {
};
COMPILED || (goog.isProvided_ = function(a) {
  return a in goog.loadedModules_ || !goog.implicitNamespaces_[a] && goog.isDefAndNotNull(goog.getObjectByName(a));
}, goog.implicitNamespaces_ = {"goog.module":!0});
goog.getObjectByName = function(a, b) {
  for (var c = a.split("."), d = b || goog.global, e;e = c.shift();) {
    if (goog.isDefAndNotNull(d[e])) {
      d = d[e];
    } else {
      return null;
    }
  }
  return d;
};
goog.globalize = function(a, b) {
  var c = b || goog.global, d;
  for (d in a) {
    c[d] = a[d];
  }
};
goog.addDependency = function(a, b, c, d) {
  if (goog.DEPENDENCIES_ENABLED) {
    var e;
    a = a.replace(/\\/g, "/");
    for (var f = goog.dependencies_, g = 0;e = b[g];g++) {
      f.nameToPath[e] = a, f.pathIsModule[a] = !!d;
    }
    for (d = 0;b = c[d];d++) {
      a in f.requires || (f.requires[a] = {}), f.requires[a][b] = !0;
    }
  }
};
goog.ENABLE_DEBUG_LOADER = !0;
goog.logToConsole_ = function(a) {
  goog.global.console && goog.global.console.error(a);
};
goog.require = function(a) {
  if (!COMPILED) {
    if (goog.isProvided_(a)) {
      return goog.isInModuleLoader_() ? goog.module.getInternal_(a) : null;
    }
    if (goog.ENABLE_DEBUG_LOADER) {
      var b = goog.getPathFromDeps_(a);
      if (b) {
        return goog.included_[b] = !0, goog.writeScripts_(), null;
      }
    }
    a = "goog.require could not find: " + a;
    goog.logToConsole_(a);
    throw Error(a);
  }
};
goog.basePath = "";
goog.nullFunction = function() {
};
goog.identityFunction = function(a, b) {
  return a;
};
goog.abstractMethod = function() {
  throw Error("unimplemented abstract method");
};
goog.addSingletonGetter = function(a) {
  a.getInstance = function() {
    if (a.instance_) {
      return a.instance_;
    }
    goog.DEBUG && (goog.instantiatedSingletons_[goog.instantiatedSingletons_.length] = a);
    return a.instance_ = new a;
  };
};
goog.instantiatedSingletons_ = [];
goog.LOAD_MODULE_USING_EVAL = !0;
goog.SEAL_MODULE_EXPORTS = goog.DEBUG;
goog.loadedModules_ = {};
goog.DEPENDENCIES_ENABLED = !COMPILED && goog.ENABLE_DEBUG_LOADER;
goog.DEPENDENCIES_ENABLED && (goog.included_ = {}, goog.dependencies_ = {pathIsModule:{}, nameToPath:{}, requires:{}, visited:{}, written:{}}, goog.inHtmlDocument_ = function() {
  var a = goog.global.document;
  return "undefined" != typeof a && "write" in a;
}, goog.findBasePath_ = function() {
  if (goog.global.CLOSURE_BASE_PATH) {
    goog.basePath = goog.global.CLOSURE_BASE_PATH;
  } else {
    if (goog.inHtmlDocument_()) {
      for (var a = goog.global.document.getElementsByTagName("script"), b = a.length - 1;0 <= b;--b) {
        var c = a[b].src, d = c.lastIndexOf("?"), d = -1 == d ? c.length : d;
        if ("base.js" == c.substr(d - 7, 7)) {
          goog.basePath = c.substr(0, d - 7);
          break;
        }
      }
    }
  }
}, goog.importScript_ = function(a, b) {
  (goog.global.CLOSURE_IMPORT_SCRIPT || goog.writeScriptTag_)(a, b) && (goog.dependencies_.written[a] = !0);
}, goog.IS_OLD_IE_ = goog.global.document && goog.global.document.all && !goog.global.atob, goog.importModule_ = function(a) {
  goog.importScript_("", 'goog.retrieveAndExecModule_("' + a + '");') && (goog.dependencies_.written[a] = !0);
}, goog.queuedModules_ = [], goog.retrieveAndExecModule_ = function(a) {
  for (var b;-1 != (b = a.indexOf("/./"));) {
    a = a.substr(0, b) + a.substr(b + 2);
  }
  for (;-1 != (b = a.indexOf("/../"));) {
    var c = a.lastIndexOf("/", b - 1);
    a = a.substr(0, c) + a.substr(b + 3);
  }
  b = goog.global.CLOSURE_IMPORT_SCRIPT || goog.writeScriptTag_;
  var d = null, c = new goog.global.XMLHttpRequest;
  c.onload = function() {
    d = this.responseText;
  };
  c.open("get", a, !1);
  c.send();
  d = c.responseText;
  if (null != d) {
    c = goog.wrapModule_(a, d), goog.IS_OLD_IE_ ? goog.queuedModules_.push(c) : b(a, c), goog.dependencies_.written[a] = !0;
  } else {
    throw Error("load of " + a + "failed");
  }
}, goog.wrapModule_ = function(a, b) {
  return goog.LOAD_MODULE_USING_EVAL && goog.isDef(goog.global.JSON) ? "goog.loadModule(" + goog.global.JSON.stringify(b + "\n//# sourceURL=" + a + "\n") + ");" : 'goog.loadModule(function(exports) {"use strict";' + b + "\n;return exports});\n//# sourceURL=" + a + "\n";
}, goog.loadQueuedModules_ = function() {
  var a = goog.queuedModules_.length;
  if (0 < a) {
    var b = goog.queuedModules_;
    goog.queuedModules_ = [];
    for (var c = 0;c < a;c++) {
      goog.globalEval(b[c]);
    }
  }
}, goog.loadModule = function(a) {
  try {
    goog.moduleLoaderState_ = {moduleName:void 0, declareTestMethods:!1};
    var b;
    if (goog.isFunction(a)) {
      b = a.call(goog.global, {});
    } else {
      if (goog.isString(a)) {
        b = goog.loadModuleFromSource_.call(goog.global, a);
      } else {
        throw Error("Invalid module definition");
      }
    }
    var c = goog.moduleLoaderState_.moduleName;
    if (!goog.isString(c) || !c) {
      throw Error('Invalid module name "' + c + '"');
    }
    goog.moduleLoaderState_.declareLegacyNamespace ? goog.constructNamespace_(c, b) : goog.SEAL_MODULE_EXPORTS && Object.seal && Object.seal(b);
    goog.loadedModules_[c] = b;
    if (goog.moduleLoaderState_.declareTestMethods) {
      for (var d in b) {
        if (0 === d.indexOf("test", 0) || "tearDown" == d || "setUp" == d || "setUpPage" == d || "tearDownPage" == d) {
          goog.global[d] = b[d];
        }
      }
    }
  } finally {
    goog.moduleLoaderState_ = null;
  }
}, goog.loadModuleFromSource_ = function(a) {
  eval(a);
  return{};
}, goog.writeScriptTag_ = function(a, b) {
  if (goog.inHtmlDocument_()) {
    var c = goog.global.document;
    if ("complete" == c.readyState) {
      if (/\bdeps.js$/.test(a)) {
        return!1;
      }
      throw Error('Cannot write "' + a + '" after document load');
    }
    var d = goog.IS_OLD_IE_;
    void 0 === b ? d ? (d = " onreadystatechange='goog.onScriptLoad_(this, " + ++goog.lastNonModuleScriptIndex_ + ")' ", c.write('<script type="text/javascript" src="' + a + '"' + d + ">\x3c/script>")) : c.write('<script type="text/javascript" src="' + a + '">\x3c/script>') : c.write('<script type="text/javascript">' + b + "\x3c/script>");
    return!0;
  }
  return!1;
}, goog.lastNonModuleScriptIndex_ = 0, goog.onScriptLoad_ = function(a, b) {
  "complete" == a.readyState && goog.lastNonModuleScriptIndex_ == b && goog.loadQueuedModules_();
  return!0;
}, goog.writeScripts_ = function() {
  function a(e) {
    if (!(e in d.written)) {
      if (!(e in d.visited) && (d.visited[e] = !0, e in d.requires)) {
        for (var f in d.requires[e]) {
          if (!goog.isProvided_(f)) {
            if (f in d.nameToPath) {
              a(d.nameToPath[f]);
            } else {
              throw Error("Undefined nameToPath for " + f);
            }
          }
        }
      }
      e in c || (c[e] = !0, b.push(e));
    }
  }
  var b = [], c = {}, d = goog.dependencies_, e;
  for (e in goog.included_) {
    d.written[e] || a(e);
  }
  for (var f = 0;f < b.length;f++) {
    e = b[f], goog.dependencies_.written[e] = !0;
  }
  var g = goog.moduleLoaderState_;
  goog.moduleLoaderState_ = null;
  for (f = 0;f < b.length;f++) {
    if (e = b[f]) {
      d.pathIsModule[e] ? goog.importModule_(goog.basePath + e) : goog.importScript_(goog.basePath + e);
    } else {
      throw goog.moduleLoaderState_ = g, Error("Undefined script input");
    }
  }
  goog.moduleLoaderState_ = g;
}, goog.getPathFromDeps_ = function(a) {
  return a in goog.dependencies_.nameToPath ? goog.dependencies_.nameToPath[a] : null;
}, goog.findBasePath_(), goog.global.CLOSURE_NO_DEPS || goog.importScript_(goog.basePath + "deps.js"));
goog.typeOf = function(a) {
  var b = typeof a;
  if ("object" == b) {
    if (a) {
      if (a instanceof Array) {
        return "array";
      }
      if (a instanceof Object) {
        return b;
      }
      var c = Object.prototype.toString.call(a);
      if ("[object Window]" == c) {
        return "object";
      }
      if ("[object Array]" == c || "number" == typeof a.length && "undefined" != typeof a.splice && "undefined" != typeof a.propertyIsEnumerable && !a.propertyIsEnumerable("splice")) {
        return "array";
      }
      if ("[object Function]" == c || "undefined" != typeof a.call && "undefined" != typeof a.propertyIsEnumerable && !a.propertyIsEnumerable("call")) {
        return "function";
      }
    } else {
      return "null";
    }
  } else {
    if ("function" == b && "undefined" == typeof a.call) {
      return "object";
    }
  }
  return b;
};
goog.isNull = function(a) {
  return null === a;
};
goog.isDefAndNotNull = function(a) {
  return null != a;
};
goog.isArray = function(a) {
  return "array" == goog.typeOf(a);
};
goog.isArrayLike = function(a) {
  var b = goog.typeOf(a);
  return "array" == b || "object" == b && "number" == typeof a.length;
};
goog.isDateLike = function(a) {
  return goog.isObject(a) && "function" == typeof a.getFullYear;
};
goog.isString = function(a) {
  return "string" == typeof a;
};
goog.isBoolean = function(a) {
  return "boolean" == typeof a;
};
goog.isNumber = function(a) {
  return "number" == typeof a;
};
goog.isFunction = function(a) {
  return "function" == goog.typeOf(a);
};
goog.isObject = function(a) {
  var b = typeof a;
  return "object" == b && null != a || "function" == b;
};
goog.getUid = function(a) {
  return a[goog.UID_PROPERTY_] || (a[goog.UID_PROPERTY_] = ++goog.uidCounter_);
};
goog.hasUid = function(a) {
  return!!a[goog.UID_PROPERTY_];
};
goog.removeUid = function(a) {
  "removeAttribute" in a && a.removeAttribute(goog.UID_PROPERTY_);
  try {
    delete a[goog.UID_PROPERTY_];
  } catch (b) {
  }
};
goog.UID_PROPERTY_ = "closure_uid_" + (1E9 * Math.random() >>> 0);
goog.uidCounter_ = 0;
goog.getHashCode = goog.getUid;
goog.removeHashCode = goog.removeUid;
goog.cloneObject = function(a) {
  var b = goog.typeOf(a);
  if ("object" == b || "array" == b) {
    if (a.clone) {
      return a.clone();
    }
    var b = "array" == b ? [] : {}, c;
    for (c in a) {
      b[c] = goog.cloneObject(a[c]);
    }
    return b;
  }
  return a;
};
goog.bindNative_ = function(a, b, c) {
  return a.call.apply(a.bind, arguments);
};
goog.bindJs_ = function(a, b, c) {
  if (!a) {
    throw Error();
  }
  if (2 < arguments.length) {
    var d = Array.prototype.slice.call(arguments, 2);
    return function() {
      var c = Array.prototype.slice.call(arguments);
      Array.prototype.unshift.apply(c, d);
      return a.apply(b, c);
    };
  }
  return function() {
    return a.apply(b, arguments);
  };
};
goog.bind = function(a, b, c) {
  Function.prototype.bind && -1 != Function.prototype.bind.toString().indexOf("native code") ? goog.bind = goog.bindNative_ : goog.bind = goog.bindJs_;
  return goog.bind.apply(null, arguments);
};
goog.partial = function(a, b) {
  var c = Array.prototype.slice.call(arguments, 1);
  return function() {
    var b = c.slice();
    b.push.apply(b, arguments);
    return a.apply(this, b);
  };
};
goog.mixin = function(a, b) {
  for (var c in b) {
    a[c] = b[c];
  }
};
goog.now = goog.TRUSTED_SITE && Date.now || function() {
  return+new Date;
};
goog.globalEval = function(a) {
  if (goog.global.execScript) {
    goog.global.execScript(a, "JavaScript");
  } else {
    if (goog.global.eval) {
      if (null == goog.evalWorksForGlobals_ && (goog.global.eval("var _et_ = 1;"), "undefined" != typeof goog.global._et_ ? (delete goog.global._et_, goog.evalWorksForGlobals_ = !0) : goog.evalWorksForGlobals_ = !1), goog.evalWorksForGlobals_) {
        goog.global.eval(a);
      } else {
        var b = goog.global.document, c = b.createElement("script");
        c.type = "text/javascript";
        c.defer = !1;
        c.appendChild(b.createTextNode(a));
        b.body.appendChild(c);
        b.body.removeChild(c);
      }
    } else {
      throw Error("goog.globalEval not available");
    }
  }
};
goog.evalWorksForGlobals_ = null;
goog.getCssName = function(a, b) {
  var c = function(a) {
    return goog.cssNameMapping_[a] || a;
  }, d = function(a) {
    a = a.split("-");
    for (var b = [], d = 0;d < a.length;d++) {
      b.push(c(a[d]));
    }
    return b.join("-");
  }, d = goog.cssNameMapping_ ? "BY_WHOLE" == goog.cssNameMappingStyle_ ? c : d : function(a) {
    return a;
  };
  return b ? a + "-" + d(b) : d(a);
};
goog.setCssNameMapping = function(a, b) {
  goog.cssNameMapping_ = a;
  goog.cssNameMappingStyle_ = b;
};
!COMPILED && goog.global.CLOSURE_CSS_NAME_MAPPING && (goog.cssNameMapping_ = goog.global.CLOSURE_CSS_NAME_MAPPING);
goog.getMsg = function(a, b) {
  b && (a = a.replace(/\{\$([^}]+)}/g, function(a, d) {
    return d in b ? b[d] : a;
  }));
  return a;
};
goog.getMsgWithFallback = function(a, b) {
  return a;
};
goog.exportSymbol = function(a, b, c) {
  goog.exportPath_(a, b, c);
};
goog.exportProperty = function(a, b, c) {
  a[b] = c;
};
goog.inherits = function(a, b) {
  function c() {
  }
  c.prototype = b.prototype;
  a.superClass_ = b.prototype;
  a.prototype = new c;
  a.prototype.constructor = a;
  a.base = function(a, c, f) {
    var g = Array.prototype.slice.call(arguments, 2);
    return b.prototype[c].apply(a, g);
  };
};
goog.base = function(a, b, c) {
  var d = arguments.callee.caller;
  if (goog.STRICT_MODE_COMPATIBLE || goog.DEBUG && !d) {
    throw Error("arguments.caller not defined.  goog.base() cannot be used with strict mode code. See http://www.ecma-international.org/ecma-262/5.1/#sec-C");
  }
  if (d.superClass_) {
    return d.superClass_.constructor.apply(a, Array.prototype.slice.call(arguments, 1));
  }
  for (var e = Array.prototype.slice.call(arguments, 2), f = !1, g = a.constructor;g;g = g.superClass_ && g.superClass_.constructor) {
    if (g.prototype[b] === d) {
      f = !0;
    } else {
      if (f) {
        return g.prototype[b].apply(a, e);
      }
    }
  }
  if (a[b] === d) {
    return a.constructor.prototype[b].apply(a, e);
  }
  throw Error("goog.base called from a method of one name to a method of a different name");
};
goog.scope = function(a) {
  a.call(goog.global);
};
COMPILED || (goog.global.COMPILED = COMPILED);
goog.defineClass = function(a, b) {
  var c = b.constructor, d = b.statics;
  c && c != Object.prototype.constructor || (c = function() {
    throw Error("cannot instantiate an interface (no constructor defined).");
  });
  c = goog.defineClass.createSealingConstructor_(c, a);
  a && goog.inherits(c, a);
  delete b.constructor;
  delete b.statics;
  goog.defineClass.applyProperties_(c.prototype, b);
  null != d && (d instanceof Function ? d(c) : goog.defineClass.applyProperties_(c, d));
  return c;
};
goog.defineClass.SEAL_CLASS_INSTANCES = goog.DEBUG;
goog.defineClass.createSealingConstructor_ = function(a, b) {
  if (goog.defineClass.SEAL_CLASS_INSTANCES && Object.seal instanceof Function) {
    if (b && b.prototype && b.prototype[goog.UNSEALABLE_CONSTRUCTOR_PROPERTY_]) {
      return a;
    }
    var c = function() {
      var b = a.apply(this, arguments) || this;
      b[goog.UID_PROPERTY_] = b[goog.UID_PROPERTY_];
      this.constructor === c && Object.seal(b);
      return b;
    };
    return c;
  }
  return a;
};
goog.defineClass.OBJECT_PROTOTYPE_FIELDS_ = "constructor hasOwnProperty isPrototypeOf propertyIsEnumerable toLocaleString toString valueOf".split(" ");
goog.defineClass.applyProperties_ = function(a, b) {
  for (var c in b) {
    Object.prototype.hasOwnProperty.call(b, c) && (a[c] = b[c]);
  }
  for (var d = 0;d < goog.defineClass.OBJECT_PROTOTYPE_FIELDS_.length;d++) {
    c = goog.defineClass.OBJECT_PROTOTYPE_FIELDS_[d], Object.prototype.hasOwnProperty.call(b, c) && (a[c] = b[c]);
  }
};
goog.tagUnsealableClass = function(a) {
  !COMPILED && goog.defineClass.SEAL_CLASS_INSTANCES && (a.prototype[goog.UNSEALABLE_CONSTRUCTOR_PROPERTY_] = !0);
};
goog.UNSEALABLE_CONSTRUCTOR_PROPERTY_ = "goog_defineClass_legacy_unsealable";
goog.math = {};
goog.math.Size = function(a, b) {
  this.width = a;
  this.height = b;
};
goog.math.Size.equals = function(a, b) {
  return a == b ? !0 : a && b ? a.width == b.width && a.height == b.height : !1;
};
goog.math.Size.prototype.clone = function() {
  return new goog.math.Size(this.width, this.height);
};
goog.DEBUG && (goog.math.Size.prototype.toString = function() {
  return "(" + this.width + " x " + this.height + ")";
});
goog.math.Size.prototype.getLongest = function() {
  return Math.max(this.width, this.height);
};
goog.math.Size.prototype.getShortest = function() {
  return Math.min(this.width, this.height);
};
goog.math.Size.prototype.area = function() {
  return this.width * this.height;
};
goog.math.Size.prototype.perimeter = function() {
  return 2 * (this.width + this.height);
};
goog.math.Size.prototype.aspectRatio = function() {
  return this.width / this.height;
};
goog.math.Size.prototype.isEmpty = function() {
  return!this.area();
};
goog.math.Size.prototype.ceil = function() {
  this.width = Math.ceil(this.width);
  this.height = Math.ceil(this.height);
  return this;
};
goog.math.Size.prototype.fitsInside = function(a) {
  return this.width <= a.width && this.height <= a.height;
};
goog.math.Size.prototype.floor = function() {
  this.width = Math.floor(this.width);
  this.height = Math.floor(this.height);
  return this;
};
goog.math.Size.prototype.round = function() {
  this.width = Math.round(this.width);
  this.height = Math.round(this.height);
  return this;
};
goog.math.Size.prototype.scale = function(a, b) {
  var c = goog.isNumber(b) ? b : a;
  this.width *= a;
  this.height *= c;
  return this;
};
goog.math.Size.prototype.scaleToFit = function(a) {
  a = this.aspectRatio() > a.aspectRatio() ? a.width / this.width : a.height / this.height;
  return this.scale(a);
};
goog.object = {};
goog.object.forEach = function(a, b, c) {
  for (var d in a) {
    b.call(c, a[d], d, a);
  }
};
goog.object.filter = function(a, b, c) {
  var d = {}, e;
  for (e in a) {
    b.call(c, a[e], e, a) && (d[e] = a[e]);
  }
  return d;
};
goog.object.map = function(a, b, c) {
  var d = {}, e;
  for (e in a) {
    d[e] = b.call(c, a[e], e, a);
  }
  return d;
};
goog.object.some = function(a, b, c) {
  for (var d in a) {
    if (b.call(c, a[d], d, a)) {
      return!0;
    }
  }
  return!1;
};
goog.object.every = function(a, b, c) {
  for (var d in a) {
    if (!b.call(c, a[d], d, a)) {
      return!1;
    }
  }
  return!0;
};
goog.object.getCount = function(a) {
  var b = 0, c;
  for (c in a) {
    b++;
  }
  return b;
};
goog.object.getAnyKey = function(a) {
  for (var b in a) {
    return b;
  }
};
goog.object.getAnyValue = function(a) {
  for (var b in a) {
    return a[b];
  }
};
goog.object.contains = function(a, b) {
  return goog.object.containsValue(a, b);
};
goog.object.getValues = function(a) {
  var b = [], c = 0, d;
  for (d in a) {
    b[c++] = a[d];
  }
  return b;
};
goog.object.getKeys = function(a) {
  var b = [], c = 0, d;
  for (d in a) {
    b[c++] = d;
  }
  return b;
};
goog.object.getValueByKeys = function(a, b) {
  for (var c = goog.isArrayLike(b), d = c ? b : arguments, c = c ? 0 : 1;c < d.length && (a = a[d[c]], goog.isDef(a));c++) {
  }
  return a;
};
goog.object.containsKey = function(a, b) {
  return b in a;
};
goog.object.containsValue = function(a, b) {
  for (var c in a) {
    if (a[c] == b) {
      return!0;
    }
  }
  return!1;
};
goog.object.findKey = function(a, b, c) {
  for (var d in a) {
    if (b.call(c, a[d], d, a)) {
      return d;
    }
  }
};
goog.object.findValue = function(a, b, c) {
  return(b = goog.object.findKey(a, b, c)) && a[b];
};
goog.object.isEmpty = function(a) {
  for (var b in a) {
    return!1;
  }
  return!0;
};
goog.object.clear = function(a) {
  for (var b in a) {
    delete a[b];
  }
};
goog.object.remove = function(a, b) {
  var c;
  (c = b in a) && delete a[b];
  return c;
};
goog.object.add = function(a, b, c) {
  if (b in a) {
    throw Error('The object already contains the key "' + b + '"');
  }
  goog.object.set(a, b, c);
};
goog.object.get = function(a, b, c) {
  return b in a ? a[b] : c;
};
goog.object.set = function(a, b, c) {
  a[b] = c;
};
goog.object.setIfUndefined = function(a, b, c) {
  return b in a ? a[b] : a[b] = c;
};
goog.object.equals = function(a, b) {
  for (var c in a) {
    if (!(c in b) || a[c] !== b[c]) {
      return!1;
    }
  }
  for (c in b) {
    if (!(c in a)) {
      return!1;
    }
  }
  return!0;
};
goog.object.clone = function(a) {
  var b = {}, c;
  for (c in a) {
    b[c] = a[c];
  }
  return b;
};
goog.object.unsafeClone = function(a) {
  var b = goog.typeOf(a);
  if ("object" == b || "array" == b) {
    if (a.clone) {
      return a.clone();
    }
    var b = "array" == b ? [] : {}, c;
    for (c in a) {
      b[c] = goog.object.unsafeClone(a[c]);
    }
    return b;
  }
  return a;
};
goog.object.transpose = function(a) {
  var b = {}, c;
  for (c in a) {
    b[a[c]] = c;
  }
  return b;
};
goog.object.PROTOTYPE_FIELDS_ = "constructor hasOwnProperty isPrototypeOf propertyIsEnumerable toLocaleString toString valueOf".split(" ");
goog.object.extend = function(a, b) {
  for (var c, d, e = 1;e < arguments.length;e++) {
    d = arguments[e];
    for (c in d) {
      a[c] = d[c];
    }
    for (var f = 0;f < goog.object.PROTOTYPE_FIELDS_.length;f++) {
      c = goog.object.PROTOTYPE_FIELDS_[f], Object.prototype.hasOwnProperty.call(d, c) && (a[c] = d[c]);
    }
  }
};
goog.object.create = function(a) {
  var b = arguments.length;
  if (1 == b && goog.isArray(arguments[0])) {
    return goog.object.create.apply(null, arguments[0]);
  }
  if (b % 2) {
    throw Error("Uneven number of arguments");
  }
  for (var c = {}, d = 0;d < b;d += 2) {
    c[arguments[d]] = arguments[d + 1];
  }
  return c;
};
goog.object.createSet = function(a) {
  var b = arguments.length;
  if (1 == b && goog.isArray(arguments[0])) {
    return goog.object.createSet.apply(null, arguments[0]);
  }
  for (var c = {}, d = 0;d < b;d++) {
    c[arguments[d]] = !0;
  }
  return c;
};
goog.object.createImmutableView = function(a) {
  var b = a;
  Object.isFrozen && !Object.isFrozen(a) && (b = Object.create(a), Object.freeze(b));
  return b;
};
goog.object.isImmutableView = function(a) {
  return!!Object.isFrozen && Object.isFrozen(a);
};
goog.debug = {};
goog.debug.Error = function(a) {
  if (Error.captureStackTrace) {
    Error.captureStackTrace(this, goog.debug.Error);
  } else {
    var b = Error().stack;
    b && (this.stack = b);
  }
  a && (this.message = String(a));
};
goog.inherits(goog.debug.Error, Error);
goog.debug.Error.prototype.name = "CustomError";
goog.color = {};
goog.color.names = {aliceblue:"#f0f8ff", antiquewhite:"#faebd7", aqua:"#00ffff", aquamarine:"#7fffd4", azure:"#f0ffff", beige:"#f5f5dc", bisque:"#ffe4c4", black:"#000000", blanchedalmond:"#ffebcd", blue:"#0000ff", blueviolet:"#8a2be2", brown:"#a52a2a", burlywood:"#deb887", cadetblue:"#5f9ea0", chartreuse:"#7fff00", chocolate:"#d2691e", coral:"#ff7f50", cornflowerblue:"#6495ed", cornsilk:"#fff8dc", crimson:"#dc143c", cyan:"#00ffff", darkblue:"#00008b", darkcyan:"#008b8b", darkgoldenrod:"#b8860b", 
darkgray:"#a9a9a9", darkgreen:"#006400", darkgrey:"#a9a9a9", darkkhaki:"#bdb76b", darkmagenta:"#8b008b", darkolivegreen:"#556b2f", darkorange:"#ff8c00", darkorchid:"#9932cc", darkred:"#8b0000", darksalmon:"#e9967a", darkseagreen:"#8fbc8f", darkslateblue:"#483d8b", darkslategray:"#2f4f4f", darkslategrey:"#2f4f4f", darkturquoise:"#00ced1", darkviolet:"#9400d3", deeppink:"#ff1493", deepskyblue:"#00bfff", dimgray:"#696969", dimgrey:"#696969", dodgerblue:"#1e90ff", firebrick:"#b22222", floralwhite:"#fffaf0", 
forestgreen:"#228b22", fuchsia:"#ff00ff", gainsboro:"#dcdcdc", ghostwhite:"#f8f8ff", gold:"#ffd700", goldenrod:"#daa520", gray:"#808080", green:"#008000", greenyellow:"#adff2f", grey:"#808080", honeydew:"#f0fff0", hotpink:"#ff69b4", indianred:"#cd5c5c", indigo:"#4b0082", ivory:"#fffff0", khaki:"#f0e68c", lavender:"#e6e6fa", lavenderblush:"#fff0f5", lawngreen:"#7cfc00", lemonchiffon:"#fffacd", lightblue:"#add8e6", lightcoral:"#f08080", lightcyan:"#e0ffff", lightgoldenrodyellow:"#fafad2", lightgray:"#d3d3d3", 
lightgreen:"#90ee90", lightgrey:"#d3d3d3", lightpink:"#ffb6c1", lightsalmon:"#ffa07a", lightseagreen:"#20b2aa", lightskyblue:"#87cefa", lightslategray:"#778899", lightslategrey:"#778899", lightsteelblue:"#b0c4de", lightyellow:"#ffffe0", lime:"#00ff00", limegreen:"#32cd32", linen:"#faf0e6", magenta:"#ff00ff", maroon:"#800000", mediumaquamarine:"#66cdaa", mediumblue:"#0000cd", mediumorchid:"#ba55d3", mediumpurple:"#9370db", mediumseagreen:"#3cb371", mediumslateblue:"#7b68ee", mediumspringgreen:"#00fa9a", 
mediumturquoise:"#48d1cc", mediumvioletred:"#c71585", midnightblue:"#191970", mintcream:"#f5fffa", mistyrose:"#ffe4e1", moccasin:"#ffe4b5", navajowhite:"#ffdead", navy:"#000080", oldlace:"#fdf5e6", olive:"#808000", olivedrab:"#6b8e23", orange:"#ffa500", orangered:"#ff4500", orchid:"#da70d6", palegoldenrod:"#eee8aa", palegreen:"#98fb98", paleturquoise:"#afeeee", palevioletred:"#db7093", papayawhip:"#ffefd5", peachpuff:"#ffdab9", peru:"#cd853f", pink:"#ffc0cb", plum:"#dda0dd", powderblue:"#b0e0e6", 
purple:"#800080", red:"#ff0000", rosybrown:"#bc8f8f", royalblue:"#4169e1", saddlebrown:"#8b4513", salmon:"#fa8072", sandybrown:"#f4a460", seagreen:"#2e8b57", seashell:"#fff5ee", sienna:"#a0522d", silver:"#c0c0c0", skyblue:"#87ceeb", slateblue:"#6a5acd", slategray:"#708090", slategrey:"#708090", snow:"#fffafa", springgreen:"#00ff7f", steelblue:"#4682b4", tan:"#d2b48c", teal:"#008080", thistle:"#d8bfd8", tomato:"#ff6347", turquoise:"#40e0d0", violet:"#ee82ee", wheat:"#f5deb3", white:"#ffffff", whitesmoke:"#f5f5f5", 
yellow:"#ffff00", yellowgreen:"#9acd32"};
goog.string = {};
goog.string.DETECT_DOUBLE_ESCAPING = !1;
goog.string.Unicode = {NBSP:"\u00a0"};
goog.string.startsWith = function(a, b) {
  return 0 == a.lastIndexOf(b, 0);
};
goog.string.endsWith = function(a, b) {
  var c = a.length - b.length;
  return 0 <= c && a.indexOf(b, c) == c;
};
goog.string.caseInsensitiveStartsWith = function(a, b) {
  return 0 == goog.string.caseInsensitiveCompare(b, a.substr(0, b.length));
};
goog.string.caseInsensitiveEndsWith = function(a, b) {
  return 0 == goog.string.caseInsensitiveCompare(b, a.substr(a.length - b.length, b.length));
};
goog.string.caseInsensitiveEquals = function(a, b) {
  return a.toLowerCase() == b.toLowerCase();
};
goog.string.subs = function(a, b) {
  for (var c = a.split("%s"), d = "", e = Array.prototype.slice.call(arguments, 1);e.length && 1 < c.length;) {
    d += c.shift() + e.shift();
  }
  return d + c.join("%s");
};
goog.string.collapseWhitespace = function(a) {
  return a.replace(/[\s\xa0]+/g, " ").replace(/^\s+|\s+$/g, "");
};
goog.string.isEmptyOrWhitespace = function(a) {
  return/^[\s\xa0]*$/.test(a);
};
goog.string.isEmptyString = function(a) {
  return 0 == a.length;
};
goog.string.isEmpty = goog.string.isEmptyOrWhitespace;
goog.string.isEmptyOrWhitespaceSafe = function(a) {
  return goog.string.isEmptyOrWhitespace(goog.string.makeSafe(a));
};
goog.string.isEmptySafe = goog.string.isEmptyOrWhitespaceSafe;
goog.string.isBreakingWhitespace = function(a) {
  return!/[^\t\n\r ]/.test(a);
};
goog.string.isAlpha = function(a) {
  return!/[^a-zA-Z]/.test(a);
};
goog.string.isNumeric = function(a) {
  return!/[^0-9]/.test(a);
};
goog.string.isAlphaNumeric = function(a) {
  return!/[^a-zA-Z0-9]/.test(a);
};
goog.string.isSpace = function(a) {
  return " " == a;
};
goog.string.isUnicodeChar = function(a) {
  return 1 == a.length && " " <= a && "~" >= a || "\u0080" <= a && "\ufffd" >= a;
};
goog.string.stripNewlines = function(a) {
  return a.replace(/(\r\n|\r|\n)+/g, " ");
};
goog.string.canonicalizeNewlines = function(a) {
  return a.replace(/(\r\n|\r|\n)/g, "\n");
};
goog.string.normalizeWhitespace = function(a) {
  return a.replace(/\xa0|\s/g, " ");
};
goog.string.normalizeSpaces = function(a) {
  return a.replace(/\xa0|[ \t]+/g, " ");
};
goog.string.collapseBreakingSpaces = function(a) {
  return a.replace(/[\t\r\n ]+/g, " ").replace(/^[\t\r\n ]+|[\t\r\n ]+$/g, "");
};
goog.string.trim = goog.TRUSTED_SITE && String.prototype.trim ? function(a) {
  return a.trim();
} : function(a) {
  return a.replace(/^[\s\xa0]+|[\s\xa0]+$/g, "");
};
goog.string.trimLeft = function(a) {
  return a.replace(/^[\s\xa0]+/, "");
};
goog.string.trimRight = function(a) {
  return a.replace(/[\s\xa0]+$/, "");
};
goog.string.caseInsensitiveCompare = function(a, b) {
  var c = String(a).toLowerCase(), d = String(b).toLowerCase();
  return c < d ? -1 : c == d ? 0 : 1;
};
goog.string.numerateCompareRegExp_ = /(\.\d+)|(\d+)|(\D+)/g;
goog.string.numerateCompare = function(a, b) {
  if (a == b) {
    return 0;
  }
  if (!a) {
    return-1;
  }
  if (!b) {
    return 1;
  }
  for (var c = a.toLowerCase().match(goog.string.numerateCompareRegExp_), d = b.toLowerCase().match(goog.string.numerateCompareRegExp_), e = Math.min(c.length, d.length), f = 0;f < e;f++) {
    var g = c[f], h = d[f];
    if (g != h) {
      return c = parseInt(g, 10), !isNaN(c) && (d = parseInt(h, 10), !isNaN(d) && c - d) ? c - d : g < h ? -1 : 1;
    }
  }
  return c.length != d.length ? c.length - d.length : a < b ? -1 : 1;
};
goog.string.urlEncode = function(a) {
  return encodeURIComponent(String(a));
};
goog.string.urlDecode = function(a) {
  return decodeURIComponent(a.replace(/\+/g, " "));
};
goog.string.newLineToBr = function(a, b) {
  return a.replace(/(\r\n|\r|\n)/g, b ? "<br />" : "<br>");
};
goog.string.htmlEscape = function(a, b) {
  if (b) {
    a = a.replace(goog.string.AMP_RE_, "&amp;").replace(goog.string.LT_RE_, "&lt;").replace(goog.string.GT_RE_, "&gt;").replace(goog.string.QUOT_RE_, "&quot;").replace(goog.string.SINGLE_QUOTE_RE_, "&#39;").replace(goog.string.NULL_RE_, "&#0;"), goog.string.DETECT_DOUBLE_ESCAPING && (a = a.replace(goog.string.E_RE_, "&#101;"));
  } else {
    if (!goog.string.ALL_RE_.test(a)) {
      return a;
    }
    -1 != a.indexOf("&") && (a = a.replace(goog.string.AMP_RE_, "&amp;"));
    -1 != a.indexOf("<") && (a = a.replace(goog.string.LT_RE_, "&lt;"));
    -1 != a.indexOf(">") && (a = a.replace(goog.string.GT_RE_, "&gt;"));
    -1 != a.indexOf('"') && (a = a.replace(goog.string.QUOT_RE_, "&quot;"));
    -1 != a.indexOf("'") && (a = a.replace(goog.string.SINGLE_QUOTE_RE_, "&#39;"));
    -1 != a.indexOf("\x00") && (a = a.replace(goog.string.NULL_RE_, "&#0;"));
    goog.string.DETECT_DOUBLE_ESCAPING && -1 != a.indexOf("e") && (a = a.replace(goog.string.E_RE_, "&#101;"));
  }
  return a;
};
goog.string.AMP_RE_ = /&/g;
goog.string.LT_RE_ = /</g;
goog.string.GT_RE_ = />/g;
goog.string.QUOT_RE_ = /"/g;
goog.string.SINGLE_QUOTE_RE_ = /'/g;
goog.string.NULL_RE_ = /\x00/g;
goog.string.E_RE_ = /e/g;
goog.string.ALL_RE_ = goog.string.DETECT_DOUBLE_ESCAPING ? /[\x00&<>"'e]/ : /[\x00&<>"']/;
goog.string.unescapeEntities = function(a) {
  return goog.string.contains(a, "&") ? "document" in goog.global ? goog.string.unescapeEntitiesUsingDom_(a) : goog.string.unescapePureXmlEntities_(a) : a;
};
goog.string.unescapeEntitiesWithDocument = function(a, b) {
  return goog.string.contains(a, "&") ? goog.string.unescapeEntitiesUsingDom_(a, b) : a;
};
goog.string.unescapeEntitiesUsingDom_ = function(a, b) {
  var c = {"&amp;":"&", "&lt;":"<", "&gt;":">", "&quot;":'"'}, d;
  d = b ? b.createElement("div") : goog.global.document.createElement("div");
  return a.replace(goog.string.HTML_ENTITY_PATTERN_, function(a, b) {
    var g = c[a];
    if (g) {
      return g;
    }
    if ("#" == b.charAt(0)) {
      var h = Number("0" + b.substr(1));
      isNaN(h) || (g = String.fromCharCode(h));
    }
    g || (d.innerHTML = a + " ", g = d.firstChild.nodeValue.slice(0, -1));
    return c[a] = g;
  });
};
goog.string.unescapePureXmlEntities_ = function(a) {
  return a.replace(/&([^;]+);/g, function(a, c) {
    switch(c) {
      case "amp":
        return "&";
      case "lt":
        return "<";
      case "gt":
        return ">";
      case "quot":
        return'"';
      default:
        if ("#" == c.charAt(0)) {
          var d = Number("0" + c.substr(1));
          if (!isNaN(d)) {
            return String.fromCharCode(d);
          }
        }
        return a;
    }
  });
};
goog.string.HTML_ENTITY_PATTERN_ = /&([^;\s<&]+);?/g;
goog.string.whitespaceEscape = function(a, b) {
  return goog.string.newLineToBr(a.replace(/  /g, " &#160;"), b);
};
goog.string.preserveSpaces = function(a) {
  return a.replace(/(^|[\n ]) /g, "$1" + goog.string.Unicode.NBSP);
};
goog.string.stripQuotes = function(a, b) {
  for (var c = b.length, d = 0;d < c;d++) {
    var e = 1 == c ? b : b.charAt(d);
    if (a.charAt(0) == e && a.charAt(a.length - 1) == e) {
      return a.substring(1, a.length - 1);
    }
  }
  return a;
};
goog.string.truncate = function(a, b, c) {
  c && (a = goog.string.unescapeEntities(a));
  a.length > b && (a = a.substring(0, b - 3) + "...");
  c && (a = goog.string.htmlEscape(a));
  return a;
};
goog.string.truncateMiddle = function(a, b, c, d) {
  c && (a = goog.string.unescapeEntities(a));
  if (d && a.length > b) {
    d > b && (d = b);
    var e = a.length - d;
    a = a.substring(0, b - d) + "..." + a.substring(e);
  } else {
    a.length > b && (d = Math.floor(b / 2), e = a.length - d, a = a.substring(0, d + b % 2) + "..." + a.substring(e));
  }
  c && (a = goog.string.htmlEscape(a));
  return a;
};
goog.string.specialEscapeChars_ = {"\x00":"\\0", "\b":"\\b", "\f":"\\f", "\n":"\\n", "\r":"\\r", "\t":"\\t", "\x0B":"\\x0B", '"':'\\"', "\\":"\\\\"};
goog.string.jsEscapeCache_ = {"'":"\\'"};
goog.string.quote = function(a) {
  a = String(a);
  if (a.quote) {
    return a.quote();
  }
  for (var b = ['"'], c = 0;c < a.length;c++) {
    var d = a.charAt(c), e = d.charCodeAt(0);
    b[c + 1] = goog.string.specialEscapeChars_[d] || (31 < e && 127 > e ? d : goog.string.escapeChar(d));
  }
  b.push('"');
  return b.join("");
};
goog.string.escapeString = function(a) {
  for (var b = [], c = 0;c < a.length;c++) {
    b[c] = goog.string.escapeChar(a.charAt(c));
  }
  return b.join("");
};
goog.string.escapeChar = function(a) {
  if (a in goog.string.jsEscapeCache_) {
    return goog.string.jsEscapeCache_[a];
  }
  if (a in goog.string.specialEscapeChars_) {
    return goog.string.jsEscapeCache_[a] = goog.string.specialEscapeChars_[a];
  }
  var b = a, c = a.charCodeAt(0);
  if (31 < c && 127 > c) {
    b = a;
  } else {
    if (256 > c) {
      if (b = "\\x", 16 > c || 256 < c) {
        b += "0";
      }
    } else {
      b = "\\u", 4096 > c && (b += "0");
    }
    b += c.toString(16).toUpperCase();
  }
  return goog.string.jsEscapeCache_[a] = b;
};
goog.string.contains = function(a, b) {
  return-1 != a.indexOf(b);
};
goog.string.caseInsensitiveContains = function(a, b) {
  return goog.string.contains(a.toLowerCase(), b.toLowerCase());
};
goog.string.countOf = function(a, b) {
  return a && b ? a.split(b).length - 1 : 0;
};
goog.string.removeAt = function(a, b, c) {
  var d = a;
  0 <= b && b < a.length && 0 < c && (d = a.substr(0, b) + a.substr(b + c, a.length - b - c));
  return d;
};
goog.string.remove = function(a, b) {
  var c = new RegExp(goog.string.regExpEscape(b), "");
  return a.replace(c, "");
};
goog.string.removeAll = function(a, b) {
  var c = new RegExp(goog.string.regExpEscape(b), "g");
  return a.replace(c, "");
};
goog.string.regExpEscape = function(a) {
  return String(a).replace(/([-()\[\]{}+?*.$\^|,:#<!\\])/g, "\\$1").replace(/\x08/g, "\\x08");
};
goog.string.repeat = function(a, b) {
  return Array(b + 1).join(a);
};
goog.string.padNumber = function(a, b, c) {
  a = goog.isDef(c) ? a.toFixed(c) : String(a);
  c = a.indexOf(".");
  -1 == c && (c = a.length);
  return goog.string.repeat("0", Math.max(0, b - c)) + a;
};
goog.string.makeSafe = function(a) {
  return null == a ? "" : String(a);
};
goog.string.buildString = function(a) {
  return Array.prototype.join.call(arguments, "");
};
goog.string.getRandomString = function() {
  return Math.floor(2147483648 * Math.random()).toString(36) + Math.abs(Math.floor(2147483648 * Math.random()) ^ goog.now()).toString(36);
};
goog.string.compareVersions = function(a, b) {
  for (var c = 0, d = goog.string.trim(String(a)).split("."), e = goog.string.trim(String(b)).split("."), f = Math.max(d.length, e.length), g = 0;0 == c && g < f;g++) {
    var h = d[g] || "", k = e[g] || "", l = RegExp("(\\d*)(\\D*)", "g"), m = RegExp("(\\d*)(\\D*)", "g");
    do {
      var n = l.exec(h) || ["", "", ""], p = m.exec(k) || ["", "", ""];
      if (0 == n[0].length && 0 == p[0].length) {
        break;
      }
      var c = 0 == n[1].length ? 0 : parseInt(n[1], 10), q = 0 == p[1].length ? 0 : parseInt(p[1], 10), c = goog.string.compareElements_(c, q) || goog.string.compareElements_(0 == n[2].length, 0 == p[2].length) || goog.string.compareElements_(n[2], p[2]);
    } while (0 == c);
  }
  return c;
};
goog.string.compareElements_ = function(a, b) {
  return a < b ? -1 : a > b ? 1 : 0;
};
goog.string.HASHCODE_MAX_ = 4294967296;
goog.string.hashCode = function(a) {
  for (var b = 0, c = 0;c < a.length;++c) {
    b = 31 * b + a.charCodeAt(c), b %= goog.string.HASHCODE_MAX_;
  }
  return b;
};
goog.string.uniqueStringCounter_ = 2147483648 * Math.random() | 0;
goog.string.createUniqueString = function() {
  return "goog_" + goog.string.uniqueStringCounter_++;
};
goog.string.toNumber = function(a) {
  var b = Number(a);
  return 0 == b && goog.string.isEmpty(a) ? NaN : b;
};
goog.string.isLowerCamelCase = function(a) {
  return/^[a-z]+([A-Z][a-z]*)*$/.test(a);
};
goog.string.isUpperCamelCase = function(a) {
  return/^([A-Z][a-z]*)+$/.test(a);
};
goog.string.toCamelCase = function(a) {
  return String(a).replace(/\-([a-z])/g, function(a, c) {
    return c.toUpperCase();
  });
};
goog.string.toSelectorCase = function(a) {
  return String(a).replace(/([A-Z])/g, "-$1").toLowerCase();
};
goog.string.toTitleCase = function(a, b) {
  var c = goog.isString(b) ? goog.string.regExpEscape(b) : "\\s";
  return a.replace(new RegExp("(^" + (c ? "|[" + c + "]+" : "") + ")([a-z])", "g"), function(a, b, c) {
    return b + c.toUpperCase();
  });
};
goog.string.parseInt = function(a) {
  isFinite(a) && (a = String(a));
  return goog.isString(a) ? /^\s*-?0x/i.test(a) ? parseInt(a, 16) : parseInt(a, 10) : NaN;
};
goog.string.splitLimit = function(a, b, c) {
  a = a.split(b);
  for (var d = [];0 < c && a.length;) {
    d.push(a.shift()), c--;
  }
  a.length && d.push(a.join(b));
  return d;
};
goog.labs = {};
goog.labs.userAgent = {};
goog.labs.userAgent.util = {};
goog.labs.userAgent.util.getNativeUserAgentString_ = function() {
  var a = goog.labs.userAgent.util.getNavigator_();
  return a && (a = a.userAgent) ? a : "";
};
goog.labs.userAgent.util.getNavigator_ = function() {
  return goog.global.navigator;
};
goog.labs.userAgent.util.userAgent_ = goog.labs.userAgent.util.getNativeUserAgentString_();
goog.labs.userAgent.util.setUserAgent = function(a) {
  goog.labs.userAgent.util.userAgent_ = a || goog.labs.userAgent.util.getNativeUserAgentString_();
};
goog.labs.userAgent.util.getUserAgent = function() {
  return goog.labs.userAgent.util.userAgent_;
};
goog.labs.userAgent.util.matchUserAgent = function(a) {
  var b = goog.labs.userAgent.util.getUserAgent();
  return goog.string.contains(b, a);
};
goog.labs.userAgent.util.matchUserAgentIgnoreCase = function(a) {
  var b = goog.labs.userAgent.util.getUserAgent();
  return goog.string.caseInsensitiveContains(b, a);
};
goog.labs.userAgent.util.extractVersionTuples = function(a) {
  for (var b = RegExp("(\\w[\\w ]+)/([^\\s]+)\\s*(?:\\((.*?)\\))?", "g"), c = [], d;d = b.exec(a);) {
    c.push([d[1], d[2], d[3] || void 0]);
  }
  return c;
};
goog.dom = {};
goog.dom.NodeType = {ELEMENT:1, ATTRIBUTE:2, TEXT:3, CDATA_SECTION:4, ENTITY_REFERENCE:5, ENTITY:6, PROCESSING_INSTRUCTION:7, COMMENT:8, DOCUMENT:9, DOCUMENT_TYPE:10, DOCUMENT_FRAGMENT:11, NOTATION:12};
goog.asserts = {};
goog.asserts.ENABLE_ASSERTS = goog.DEBUG;
goog.asserts.AssertionError = function(a, b) {
  b.unshift(a);
  goog.debug.Error.call(this, goog.string.subs.apply(null, b));
  b.shift();
  this.messagePattern = a;
};
goog.inherits(goog.asserts.AssertionError, goog.debug.Error);
goog.asserts.AssertionError.prototype.name = "AssertionError";
goog.asserts.DEFAULT_ERROR_HANDLER = function(a) {
  throw a;
};
goog.asserts.errorHandler_ = goog.asserts.DEFAULT_ERROR_HANDLER;
goog.asserts.doAssertFailure_ = function(a, b, c, d) {
  var e = "Assertion failed";
  if (c) {
    var e = e + (": " + c), f = d
  } else {
    a && (e += ": " + a, f = b);
  }
  a = new goog.asserts.AssertionError("" + e, f || []);
  goog.asserts.errorHandler_(a);
};
goog.asserts.setErrorHandler = function(a) {
  goog.asserts.ENABLE_ASSERTS && (goog.asserts.errorHandler_ = a);
};
goog.asserts.assert = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !a && goog.asserts.doAssertFailure_("", null, b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.fail = function(a, b) {
  goog.asserts.ENABLE_ASSERTS && goog.asserts.errorHandler_(new goog.asserts.AssertionError("Failure" + (a ? ": " + a : ""), Array.prototype.slice.call(arguments, 1)));
};
goog.asserts.assertNumber = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isNumber(a) && goog.asserts.doAssertFailure_("Expected number but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertString = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isString(a) && goog.asserts.doAssertFailure_("Expected string but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertFunction = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isFunction(a) && goog.asserts.doAssertFailure_("Expected function but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertObject = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isObject(a) && goog.asserts.doAssertFailure_("Expected object but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertArray = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isArray(a) && goog.asserts.doAssertFailure_("Expected array but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertBoolean = function(a, b, c) {
  goog.asserts.ENABLE_ASSERTS && !goog.isBoolean(a) && goog.asserts.doAssertFailure_("Expected boolean but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertElement = function(a, b, c) {
  !goog.asserts.ENABLE_ASSERTS || goog.isObject(a) && a.nodeType == goog.dom.NodeType.ELEMENT || goog.asserts.doAssertFailure_("Expected Element but got %s: %s.", [goog.typeOf(a), a], b, Array.prototype.slice.call(arguments, 2));
  return a;
};
goog.asserts.assertInstanceof = function(a, b, c, d) {
  !goog.asserts.ENABLE_ASSERTS || a instanceof b || goog.asserts.doAssertFailure_("Expected instanceof %s but got %s.", [goog.asserts.getType_(b), goog.asserts.getType_(a)], c, Array.prototype.slice.call(arguments, 3));
  return a;
};
goog.asserts.assertObjectPrototypeIsIntact = function() {
  for (var a in Object.prototype) {
    goog.asserts.fail(a + " should not be enumerable in Object.prototype.");
  }
};
goog.asserts.getType_ = function(a) {
  return a instanceof Function ? a.displayName || a.name || "unknown type name" : a instanceof Object ? a.constructor.displayName || a.constructor.name || Object.prototype.toString.call(a) : null === a ? "null" : typeof a;
};
goog.array = {};
goog.NATIVE_ARRAY_PROTOTYPES = goog.TRUSTED_SITE;
goog.array.ASSUME_NATIVE_FUNCTIONS = !1;
goog.array.peek = function(a) {
  return a[a.length - 1];
};
goog.array.last = goog.array.peek;
goog.array.ARRAY_PROTOTYPE_ = Array.prototype;
goog.array.indexOf = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.indexOf) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.indexOf.call(a, b, c);
} : function(a, b, c) {
  c = null == c ? 0 : 0 > c ? Math.max(0, a.length + c) : c;
  if (goog.isString(a)) {
    return goog.isString(b) && 1 == b.length ? a.indexOf(b, c) : -1;
  }
  for (;c < a.length;c++) {
    if (c in a && a[c] === b) {
      return c;
    }
  }
  return-1;
};
goog.array.lastIndexOf = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.lastIndexOf) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.lastIndexOf.call(a, b, null == c ? a.length - 1 : c);
} : function(a, b, c) {
  c = null == c ? a.length - 1 : c;
  0 > c && (c = Math.max(0, a.length + c));
  if (goog.isString(a)) {
    return goog.isString(b) && 1 == b.length ? a.lastIndexOf(b, c) : -1;
  }
  for (;0 <= c;c--) {
    if (c in a && a[c] === b) {
      return c;
    }
  }
  return-1;
};
goog.array.forEach = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.forEach) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  goog.array.ARRAY_PROTOTYPE_.forEach.call(a, b, c);
} : function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, f = 0;f < d;f++) {
    f in e && b.call(c, e[f], f, a);
  }
};
goog.array.forEachRight = function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, d = d - 1;0 <= d;--d) {
    d in e && b.call(c, e[d], d, a);
  }
};
goog.array.filter = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.filter) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.filter.call(a, b, c);
} : function(a, b, c) {
  for (var d = a.length, e = [], f = 0, g = goog.isString(a) ? a.split("") : a, h = 0;h < d;h++) {
    if (h in g) {
      var k = g[h];
      b.call(c, k, h, a) && (e[f++] = k);
    }
  }
  return e;
};
goog.array.map = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.map) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.map.call(a, b, c);
} : function(a, b, c) {
  for (var d = a.length, e = Array(d), f = goog.isString(a) ? a.split("") : a, g = 0;g < d;g++) {
    g in f && (e[g] = b.call(c, f[g], g, a));
  }
  return e;
};
goog.array.reduce = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.reduce) ? function(a, b, c, d) {
  goog.asserts.assert(null != a.length);
  d && (b = goog.bind(b, d));
  return goog.array.ARRAY_PROTOTYPE_.reduce.call(a, b, c);
} : function(a, b, c, d) {
  var e = c;
  goog.array.forEach(a, function(c, g) {
    e = b.call(d, e, c, g, a);
  });
  return e;
};
goog.array.reduceRight = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.reduceRight) ? function(a, b, c, d) {
  goog.asserts.assert(null != a.length);
  d && (b = goog.bind(b, d));
  return goog.array.ARRAY_PROTOTYPE_.reduceRight.call(a, b, c);
} : function(a, b, c, d) {
  var e = c;
  goog.array.forEachRight(a, function(c, g) {
    e = b.call(d, e, c, g, a);
  });
  return e;
};
goog.array.some = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.some) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.some.call(a, b, c);
} : function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, f = 0;f < d;f++) {
    if (f in e && b.call(c, e[f], f, a)) {
      return!0;
    }
  }
  return!1;
};
goog.array.every = goog.NATIVE_ARRAY_PROTOTYPES && (goog.array.ASSUME_NATIVE_FUNCTIONS || goog.array.ARRAY_PROTOTYPE_.every) ? function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.every.call(a, b, c);
} : function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, f = 0;f < d;f++) {
    if (f in e && !b.call(c, e[f], f, a)) {
      return!1;
    }
  }
  return!0;
};
goog.array.count = function(a, b, c) {
  var d = 0;
  goog.array.forEach(a, function(a, f, g) {
    b.call(c, a, f, g) && ++d;
  }, c);
  return d;
};
goog.array.find = function(a, b, c) {
  b = goog.array.findIndex(a, b, c);
  return 0 > b ? null : goog.isString(a) ? a.charAt(b) : a[b];
};
goog.array.findIndex = function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, f = 0;f < d;f++) {
    if (f in e && b.call(c, e[f], f, a)) {
      return f;
    }
  }
  return-1;
};
goog.array.findRight = function(a, b, c) {
  b = goog.array.findIndexRight(a, b, c);
  return 0 > b ? null : goog.isString(a) ? a.charAt(b) : a[b];
};
goog.array.findIndexRight = function(a, b, c) {
  for (var d = a.length, e = goog.isString(a) ? a.split("") : a, d = d - 1;0 <= d;d--) {
    if (d in e && b.call(c, e[d], d, a)) {
      return d;
    }
  }
  return-1;
};
goog.array.contains = function(a, b) {
  return 0 <= goog.array.indexOf(a, b);
};
goog.array.isEmpty = function(a) {
  return 0 == a.length;
};
goog.array.clear = function(a) {
  if (!goog.isArray(a)) {
    for (var b = a.length - 1;0 <= b;b--) {
      delete a[b];
    }
  }
  a.length = 0;
};
goog.array.insert = function(a, b) {
  goog.array.contains(a, b) || a.push(b);
};
goog.array.insertAt = function(a, b, c) {
  goog.array.splice(a, c, 0, b);
};
goog.array.insertArrayAt = function(a, b, c) {
  goog.partial(goog.array.splice, a, c, 0).apply(null, b);
};
goog.array.insertBefore = function(a, b, c) {
  var d;
  2 == arguments.length || 0 > (d = goog.array.indexOf(a, c)) ? a.push(b) : goog.array.insertAt(a, b, d);
};
goog.array.remove = function(a, b) {
  var c = goog.array.indexOf(a, b), d;
  (d = 0 <= c) && goog.array.removeAt(a, c);
  return d;
};
goog.array.removeAt = function(a, b) {
  goog.asserts.assert(null != a.length);
  return 1 == goog.array.ARRAY_PROTOTYPE_.splice.call(a, b, 1).length;
};
goog.array.removeIf = function(a, b, c) {
  b = goog.array.findIndex(a, b, c);
  return 0 <= b ? (goog.array.removeAt(a, b), !0) : !1;
};
goog.array.removeAllIf = function(a, b, c) {
  var d = 0;
  goog.array.forEachRight(a, function(e, f) {
    b.call(c, e, f, a) && goog.array.removeAt(a, f) && d++;
  });
  return d;
};
goog.array.concat = function(a) {
  return goog.array.ARRAY_PROTOTYPE_.concat.apply(goog.array.ARRAY_PROTOTYPE_, arguments);
};
goog.array.join = function(a) {
  return goog.array.ARRAY_PROTOTYPE_.concat.apply(goog.array.ARRAY_PROTOTYPE_, arguments);
};
goog.array.toArray = function(a) {
  var b = a.length;
  if (0 < b) {
    for (var c = Array(b), d = 0;d < b;d++) {
      c[d] = a[d];
    }
    return c;
  }
  return[];
};
goog.array.clone = goog.array.toArray;
goog.array.extend = function(a, b) {
  for (var c = 1;c < arguments.length;c++) {
    var d = arguments[c], e;
    if (goog.isArray(d) || (e = goog.isArrayLike(d)) && Object.prototype.hasOwnProperty.call(d, "callee")) {
      a.push.apply(a, d);
    } else {
      if (e) {
        for (var f = a.length, g = d.length, h = 0;h < g;h++) {
          a[f + h] = d[h];
        }
      } else {
        a.push(d);
      }
    }
  }
};
goog.array.splice = function(a, b, c, d) {
  goog.asserts.assert(null != a.length);
  return goog.array.ARRAY_PROTOTYPE_.splice.apply(a, goog.array.slice(arguments, 1));
};
goog.array.slice = function(a, b, c) {
  goog.asserts.assert(null != a.length);
  return 2 >= arguments.length ? goog.array.ARRAY_PROTOTYPE_.slice.call(a, b) : goog.array.ARRAY_PROTOTYPE_.slice.call(a, b, c);
};
goog.array.removeDuplicates = function(a, b, c) {
  b = b || a;
  var d = function(a) {
    return goog.isObject(g) ? "o" + goog.getUid(g) : (typeof g).charAt(0) + g;
  };
  c = c || d;
  for (var d = {}, e = 0, f = 0;f < a.length;) {
    var g = a[f++], h = c(g);
    Object.prototype.hasOwnProperty.call(d, h) || (d[h] = !0, b[e++] = g);
  }
  b.length = e;
};
goog.array.binarySearch = function(a, b, c) {
  return goog.array.binarySearch_(a, c || goog.array.defaultCompare, !1, b);
};
goog.array.binarySelect = function(a, b, c) {
  return goog.array.binarySearch_(a, b, !0, void 0, c);
};
goog.array.binarySearch_ = function(a, b, c, d, e) {
  for (var f = 0, g = a.length, h;f < g;) {
    var k = f + g >> 1, l;
    l = c ? b.call(e, a[k], k, a) : b(d, a[k]);
    0 < l ? f = k + 1 : (g = k, h = !l);
  }
  return h ? f : ~f;
};
goog.array.sort = function(a, b) {
  a.sort(b || goog.array.defaultCompare);
};
goog.array.stableSort = function(a, b) {
  for (var c = 0;c < a.length;c++) {
    a[c] = {index:c, value:a[c]};
  }
  var d = b || goog.array.defaultCompare;
  goog.array.sort(a, function(a, b) {
    return d(a.value, b.value) || a.index - b.index;
  });
  for (c = 0;c < a.length;c++) {
    a[c] = a[c].value;
  }
};
goog.array.sortByKey = function(a, b, c) {
  var d = c || goog.array.defaultCompare;
  goog.array.sort(a, function(a, c) {
    return d(b(a), b(c));
  });
};
goog.array.sortObjectsByKey = function(a, b, c) {
  goog.array.sortByKey(a, function(a) {
    return a[b];
  }, c);
};
goog.array.isSorted = function(a, b, c) {
  b = b || goog.array.defaultCompare;
  for (var d = 1;d < a.length;d++) {
    var e = b(a[d - 1], a[d]);
    if (0 < e || 0 == e && c) {
      return!1;
    }
  }
  return!0;
};
goog.array.equals = function(a, b, c) {
  if (!goog.isArrayLike(a) || !goog.isArrayLike(b) || a.length != b.length) {
    return!1;
  }
  var d = a.length;
  c = c || goog.array.defaultCompareEquality;
  for (var e = 0;e < d;e++) {
    if (!c(a[e], b[e])) {
      return!1;
    }
  }
  return!0;
};
goog.array.compare3 = function(a, b, c) {
  c = c || goog.array.defaultCompare;
  for (var d = Math.min(a.length, b.length), e = 0;e < d;e++) {
    var f = c(a[e], b[e]);
    if (0 != f) {
      return f;
    }
  }
  return goog.array.defaultCompare(a.length, b.length);
};
goog.array.defaultCompare = function(a, b) {
  return a > b ? 1 : a < b ? -1 : 0;
};
goog.array.defaultCompareEquality = function(a, b) {
  return a === b;
};
goog.array.binaryInsert = function(a, b, c) {
  c = goog.array.binarySearch(a, b, c);
  return 0 > c ? (goog.array.insertAt(a, b, -(c + 1)), !0) : !1;
};
goog.array.binaryRemove = function(a, b, c) {
  b = goog.array.binarySearch(a, b, c);
  return 0 <= b ? goog.array.removeAt(a, b) : !1;
};
goog.array.bucket = function(a, b, c) {
  for (var d = {}, e = 0;e < a.length;e++) {
    var f = a[e], g = b.call(c, f, e, a);
    goog.isDef(g) && (d[g] || (d[g] = [])).push(f);
  }
  return d;
};
goog.array.toObject = function(a, b, c) {
  var d = {};
  goog.array.forEach(a, function(e, f) {
    d[b.call(c, e, f, a)] = e;
  });
  return d;
};
goog.array.range = function(a, b, c) {
  var d = [], e = 0, f = a;
  c = c || 1;
  void 0 !== b && (e = a, f = b);
  if (0 > c * (f - e)) {
    return[];
  }
  if (0 < c) {
    for (a = e;a < f;a += c) {
      d.push(a);
    }
  } else {
    for (a = e;a > f;a += c) {
      d.push(a);
    }
  }
  return d;
};
goog.array.repeat = function(a, b) {
  for (var c = [], d = 0;d < b;d++) {
    c[d] = a;
  }
  return c;
};
goog.array.flatten = function(a) {
  for (var b = [], c = 0;c < arguments.length;c++) {
    var d = arguments[c];
    if (goog.isArray(d)) {
      for (var e = 0;e < d.length;e += 8192) {
        for (var f = goog.array.slice(d, e, e + 8192), f = goog.array.flatten.apply(null, f), g = 0;g < f.length;g++) {
          b.push(f[g]);
        }
      }
    } else {
      b.push(d);
    }
  }
  return b;
};
goog.array.rotate = function(a, b) {
  goog.asserts.assert(null != a.length);
  a.length && (b %= a.length, 0 < b ? goog.array.ARRAY_PROTOTYPE_.unshift.apply(a, a.splice(-b, b)) : 0 > b && goog.array.ARRAY_PROTOTYPE_.push.apply(a, a.splice(0, -b)));
  return a;
};
goog.array.moveItem = function(a, b, c) {
  goog.asserts.assert(0 <= b && b < a.length);
  goog.asserts.assert(0 <= c && c < a.length);
  b = goog.array.ARRAY_PROTOTYPE_.splice.call(a, b, 1);
  goog.array.ARRAY_PROTOTYPE_.splice.call(a, c, 0, b[0]);
};
goog.array.zip = function(a) {
  if (!arguments.length) {
    return[];
  }
  for (var b = [], c = 0;;c++) {
    for (var d = [], e = 0;e < arguments.length;e++) {
      var f = arguments[e];
      if (c >= f.length) {
        return b;
      }
      d.push(f[c]);
    }
    b.push(d);
  }
};
goog.array.shuffle = function(a, b) {
  for (var c = b || Math.random, d = a.length - 1;0 < d;d--) {
    var e = Math.floor(c() * (d + 1)), f = a[d];
    a[d] = a[e];
    a[e] = f;
  }
};
goog.labs.userAgent.browser = {};
goog.labs.userAgent.browser.matchOpera_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Opera") || goog.labs.userAgent.util.matchUserAgent("OPR");
};
goog.labs.userAgent.browser.matchIE_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Trident") || goog.labs.userAgent.util.matchUserAgent("MSIE");
};
goog.labs.userAgent.browser.matchFirefox_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Firefox");
};
goog.labs.userAgent.browser.matchSafari_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Safari") && !goog.labs.userAgent.util.matchUserAgent("Chrome") && !goog.labs.userAgent.util.matchUserAgent("CriOS") && !goog.labs.userAgent.util.matchUserAgent("Android");
};
goog.labs.userAgent.browser.matchCoast_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Coast");
};
goog.labs.userAgent.browser.matchIosWebview_ = function() {
  return(goog.labs.userAgent.util.matchUserAgent("iPad") || goog.labs.userAgent.util.matchUserAgent("iPhone")) && !goog.labs.userAgent.browser.matchSafari_() && !goog.labs.userAgent.browser.matchChrome_() && !goog.labs.userAgent.browser.matchCoast_() && goog.labs.userAgent.util.matchUserAgent("AppleWebKit");
};
goog.labs.userAgent.browser.matchChrome_ = function() {
  return goog.labs.userAgent.util.matchUserAgent("Chrome") || goog.labs.userAgent.util.matchUserAgent("CriOS");
};
goog.labs.userAgent.browser.matchAndroidBrowser_ = function() {
  return!goog.labs.userAgent.browser.isChrome() && goog.labs.userAgent.util.matchUserAgent("Android");
};
goog.labs.userAgent.browser.isOpera = goog.labs.userAgent.browser.matchOpera_;
goog.labs.userAgent.browser.isIE = goog.labs.userAgent.browser.matchIE_;
goog.labs.userAgent.browser.isFirefox = goog.labs.userAgent.browser.matchFirefox_;
goog.labs.userAgent.browser.isSafari = goog.labs.userAgent.browser.matchSafari_;
goog.labs.userAgent.browser.isCoast = goog.labs.userAgent.browser.matchCoast_;
goog.labs.userAgent.browser.isIosWebview = goog.labs.userAgent.browser.matchIosWebview_;
goog.labs.userAgent.browser.isChrome = goog.labs.userAgent.browser.matchChrome_;
goog.labs.userAgent.browser.isAndroidBrowser = goog.labs.userAgent.browser.matchAndroidBrowser_;
goog.labs.userAgent.browser.isSilk = function() {
  return goog.labs.userAgent.util.matchUserAgent("Silk");
};
goog.labs.userAgent.browser.getVersion = function() {
  function a(a) {
    a = goog.array.find(a, d);
    return c[a] || "";
  }
  var b = goog.labs.userAgent.util.getUserAgent();
  if (goog.labs.userAgent.browser.isIE()) {
    return goog.labs.userAgent.browser.getIEVersion_(b);
  }
  var b = goog.labs.userAgent.util.extractVersionTuples(b), c = {};
  goog.array.forEach(b, function(a) {
    c[a[0]] = a[1];
  });
  var d = goog.partial(goog.object.containsKey, c);
  return goog.labs.userAgent.browser.isOpera() ? a(["Version", "Opera", "OPR"]) : goog.labs.userAgent.browser.isChrome() ? a(["Chrome", "CriOS"]) : (b = b[2]) && b[1] || "";
};
goog.labs.userAgent.browser.isVersionOrHigher = function(a) {
  return 0 <= goog.string.compareVersions(goog.labs.userAgent.browser.getVersion(), a);
};
goog.labs.userAgent.browser.getIEVersion_ = function(a) {
  var b = /rv: *([\d\.]*)/.exec(a);
  if (b && b[1]) {
    return b[1];
  }
  var b = "", c = /MSIE +([\d\.]+)/.exec(a);
  if (c && c[1]) {
    if (a = /Trident\/(\d.\d)/.exec(a), "7.0" == c[1]) {
      if (a && a[1]) {
        switch(a[1]) {
          case "4.0":
            b = "8.0";
            break;
          case "5.0":
            b = "9.0";
            break;
          case "6.0":
            b = "10.0";
            break;
          case "7.0":
            b = "11.0";
        }
      } else {
        b = "7.0";
      }
    } else {
      b = c[1];
    }
  }
  return b;
};
goog.labs.userAgent.engine = {};
goog.labs.userAgent.engine.isPresto = function() {
  return goog.labs.userAgent.util.matchUserAgent("Presto");
};
goog.labs.userAgent.engine.isTrident = function() {
  return goog.labs.userAgent.util.matchUserAgent("Trident") || goog.labs.userAgent.util.matchUserAgent("MSIE");
};
goog.labs.userAgent.engine.isWebKit = function() {
  return goog.labs.userAgent.util.matchUserAgentIgnoreCase("WebKit");
};
goog.labs.userAgent.engine.isGecko = function() {
  return goog.labs.userAgent.util.matchUserAgent("Gecko") && !goog.labs.userAgent.engine.isWebKit() && !goog.labs.userAgent.engine.isTrident();
};
goog.labs.userAgent.engine.getVersion = function() {
  var a = goog.labs.userAgent.util.getUserAgent();
  if (a) {
    var a = goog.labs.userAgent.util.extractVersionTuples(a), b = a[1];
    if (b) {
      return "Gecko" == b[0] ? goog.labs.userAgent.engine.getVersionForKey_(a, "Firefox") : b[1];
    }
    var a = a[0], c;
    if (a && (c = a[2]) && (c = /Trident\/([^\s;]+)/.exec(c))) {
      return c[1];
    }
  }
  return "";
};
goog.labs.userAgent.engine.isVersionOrHigher = function(a) {
  return 0 <= goog.string.compareVersions(goog.labs.userAgent.engine.getVersion(), a);
};
goog.labs.userAgent.engine.getVersionForKey_ = function(a, b) {
  var c = goog.array.find(a, function(a) {
    return b == a[0];
  });
  return c && c[1] || "";
};
goog.math.randomInt = function(a) {
  return Math.floor(Math.random() * a);
};
goog.math.uniformRandom = function(a, b) {
  return a + Math.random() * (b - a);
};
goog.math.clamp = function(a, b, c) {
  return Math.min(Math.max(a, b), c);
};
goog.math.modulo = function(a, b) {
  var c = a % b;
  return 0 > c * b ? c + b : c;
};
goog.math.lerp = function(a, b, c) {
  return a + c * (b - a);
};
goog.math.nearlyEquals = function(a, b, c) {
  return Math.abs(a - b) <= (c || 1E-6);
};
goog.math.standardAngle = function(a) {
  return goog.math.modulo(a, 360);
};
goog.math.standardAngleInRadians = function(a) {
  return goog.math.modulo(a, 2 * Math.PI);
};
goog.math.toRadians = function(a) {
  return a * Math.PI / 180;
};
goog.math.toDegrees = function(a) {
  return 180 * a / Math.PI;
};
goog.math.angleDx = function(a, b) {
  return b * Math.cos(goog.math.toRadians(a));
};
goog.math.angleDy = function(a, b) {
  return b * Math.sin(goog.math.toRadians(a));
};
goog.math.angle = function(a, b, c, d) {
  return goog.math.standardAngle(goog.math.toDegrees(Math.atan2(d - b, c - a)));
};
goog.math.angleDifference = function(a, b) {
  var c = goog.math.standardAngle(b) - goog.math.standardAngle(a);
  180 < c ? c -= 360 : -180 >= c && (c = 360 + c);
  return c;
};
goog.math.sign = function(a) {
  return 0 == a ? 0 : 0 > a ? -1 : 1;
};
goog.math.longestCommonSubsequence = function(a, b, c, d) {
  c = c || function(a, b) {
    return a == b;
  };
  d = d || function(b, c) {
    return a[b];
  };
  for (var e = a.length, f = b.length, g = [], h = 0;h < e + 1;h++) {
    g[h] = [], g[h][0] = 0;
  }
  for (var k = 0;k < f + 1;k++) {
    g[0][k] = 0;
  }
  for (h = 1;h <= e;h++) {
    for (k = 1;k <= f;k++) {
      c(a[h - 1], b[k - 1]) ? g[h][k] = g[h - 1][k - 1] + 1 : g[h][k] = Math.max(g[h - 1][k], g[h][k - 1]);
    }
  }
  for (var l = [], h = e, k = f;0 < h && 0 < k;) {
    c(a[h - 1], b[k - 1]) ? (l.unshift(d(h - 1, k - 1)), h--, k--) : g[h - 1][k] > g[h][k - 1] ? h-- : k--;
  }
  return l;
};
goog.math.sum = function(a) {
  return goog.array.reduce(arguments, function(a, c) {
    return a + c;
  }, 0);
};
goog.math.average = function(a) {
  return goog.math.sum.apply(null, arguments) / arguments.length;
};
goog.math.sampleVariance = function(a) {
  var b = arguments.length;
  if (2 > b) {
    return 0;
  }
  var c = goog.math.average.apply(null, arguments);
  return goog.math.sum.apply(null, goog.array.map(arguments, function(a) {
    return Math.pow(a - c, 2);
  })) / (b - 1);
};
goog.math.standardDeviation = function(a) {
  return Math.sqrt(goog.math.sampleVariance.apply(null, arguments));
};
goog.math.isInt = function(a) {
  return isFinite(a) && 0 == a % 1;
};
goog.math.isFiniteNumber = function(a) {
  return isFinite(a) && !isNaN(a);
};
goog.math.log10Floor = function(a) {
  if (0 < a) {
    var b = Math.round(Math.log(a) * Math.LOG10E);
    return b - (parseFloat("1e" + b) > a);
  }
  return 0 == a ? -Infinity : NaN;
};
goog.math.safeFloor = function(a, b) {
  goog.asserts.assert(!goog.isDef(b) || 0 < b);
  return Math.floor(a + (b || 2E-15));
};
goog.math.safeCeil = function(a, b) {
  goog.asserts.assert(!goog.isDef(b) || 0 < b);
  return Math.ceil(a - (b || 2E-15));
};
goog.math.Coordinate = function(a, b) {
  this.x = goog.isDef(a) ? a : 0;
  this.y = goog.isDef(b) ? b : 0;
};
goog.math.Coordinate.prototype.clone = function() {
  return new goog.math.Coordinate(this.x, this.y);
};
goog.DEBUG && (goog.math.Coordinate.prototype.toString = function() {
  return "(" + this.x + ", " + this.y + ")";
});
goog.math.Coordinate.equals = function(a, b) {
  return a == b ? !0 : a && b ? a.x == b.x && a.y == b.y : !1;
};
goog.math.Coordinate.distance = function(a, b) {
  var c = a.x - b.x, d = a.y - b.y;
  return Math.sqrt(c * c + d * d);
};
goog.math.Coordinate.magnitude = function(a) {
  return Math.sqrt(a.x * a.x + a.y * a.y);
};
goog.math.Coordinate.azimuth = function(a) {
  return goog.math.angle(0, 0, a.x, a.y);
};
goog.math.Coordinate.squaredDistance = function(a, b) {
  var c = a.x - b.x, d = a.y - b.y;
  return c * c + d * d;
};
goog.math.Coordinate.difference = function(a, b) {
  return new goog.math.Coordinate(a.x - b.x, a.y - b.y);
};
goog.math.Coordinate.sum = function(a, b) {
  return new goog.math.Coordinate(a.x + b.x, a.y + b.y);
};
goog.math.Coordinate.prototype.ceil = function() {
  this.x = Math.ceil(this.x);
  this.y = Math.ceil(this.y);
  return this;
};
goog.math.Coordinate.prototype.floor = function() {
  this.x = Math.floor(this.x);
  this.y = Math.floor(this.y);
  return this;
};
goog.math.Coordinate.prototype.round = function() {
  this.x = Math.round(this.x);
  this.y = Math.round(this.y);
  return this;
};
goog.math.Coordinate.prototype.translate = function(a, b) {
  a instanceof goog.math.Coordinate ? (this.x += a.x, this.y += a.y) : (this.x += a, goog.isNumber(b) && (this.y += b));
  return this;
};
goog.math.Coordinate.prototype.scale = function(a, b) {
  var c = goog.isNumber(b) ? b : a;
  this.x *= a;
  this.y *= c;
  return this;
};
goog.math.Coordinate.prototype.rotateRadians = function(a, b) {
  var c = b || new goog.math.Coordinate(0, 0), d = this.x, e = this.y, f = Math.cos(a), g = Math.sin(a);
  this.x = (d - c.x) * f - (e - c.y) * g + c.x;
  this.y = (d - c.x) * g + (e - c.y) * f + c.y;
};
goog.math.Coordinate.prototype.rotateDegrees = function(a, b) {
  this.rotateRadians(goog.math.toRadians(a), b);
};
goog.math.Box = function(a, b, c, d) {
  this.top = a;
  this.right = b;
  this.bottom = c;
  this.left = d;
};
goog.math.Box.boundingBox = function(a) {
  for (var b = new goog.math.Box(arguments[0].y, arguments[0].x, arguments[0].y, arguments[0].x), c = 1;c < arguments.length;c++) {
    var d = arguments[c];
    b.top = Math.min(b.top, d.y);
    b.right = Math.max(b.right, d.x);
    b.bottom = Math.max(b.bottom, d.y);
    b.left = Math.min(b.left, d.x);
  }
  return b;
};
goog.math.Box.prototype.getWidth = function() {
  return this.right - this.left;
};
goog.math.Box.prototype.getHeight = function() {
  return this.bottom - this.top;
};
goog.math.Box.prototype.clone = function() {
  return new goog.math.Box(this.top, this.right, this.bottom, this.left);
};
goog.DEBUG && (goog.math.Box.prototype.toString = function() {
  return "(" + this.top + "t, " + this.right + "r, " + this.bottom + "b, " + this.left + "l)";
});
goog.math.Box.prototype.contains = function(a) {
  return goog.math.Box.contains(this, a);
};
goog.math.Box.prototype.expand = function(a, b, c, d) {
  goog.isObject(a) ? (this.top -= a.top, this.right += a.right, this.bottom += a.bottom, this.left -= a.left) : (this.top -= a, this.right += b, this.bottom += c, this.left -= d);
  return this;
};
goog.math.Box.prototype.expandToInclude = function(a) {
  this.left = Math.min(this.left, a.left);
  this.top = Math.min(this.top, a.top);
  this.right = Math.max(this.right, a.right);
  this.bottom = Math.max(this.bottom, a.bottom);
};
goog.math.Box.equals = function(a, b) {
  return a == b ? !0 : a && b ? a.top == b.top && a.right == b.right && a.bottom == b.bottom && a.left == b.left : !1;
};
goog.math.Box.contains = function(a, b) {
  return a && b ? b instanceof goog.math.Box ? b.left >= a.left && b.right <= a.right && b.top >= a.top && b.bottom <= a.bottom : b.x >= a.left && b.x <= a.right && b.y >= a.top && b.y <= a.bottom : !1;
};
goog.math.Box.relativePositionX = function(a, b) {
  return b.x < a.left ? b.x - a.left : b.x > a.right ? b.x - a.right : 0;
};
goog.math.Box.relativePositionY = function(a, b) {
  return b.y < a.top ? b.y - a.top : b.y > a.bottom ? b.y - a.bottom : 0;
};
goog.math.Box.distance = function(a, b) {
  var c = goog.math.Box.relativePositionX(a, b), d = goog.math.Box.relativePositionY(a, b);
  return Math.sqrt(c * c + d * d);
};
goog.math.Box.intersects = function(a, b) {
  return a.left <= b.right && b.left <= a.right && a.top <= b.bottom && b.top <= a.bottom;
};
goog.math.Box.intersectsWithPadding = function(a, b, c) {
  return a.left <= b.right + c && b.left <= a.right + c && a.top <= b.bottom + c && b.top <= a.bottom + c;
};
goog.math.Box.prototype.ceil = function() {
  this.top = Math.ceil(this.top);
  this.right = Math.ceil(this.right);
  this.bottom = Math.ceil(this.bottom);
  this.left = Math.ceil(this.left);
  return this;
};
goog.math.Box.prototype.floor = function() {
  this.top = Math.floor(this.top);
  this.right = Math.floor(this.right);
  this.bottom = Math.floor(this.bottom);
  this.left = Math.floor(this.left);
  return this;
};
goog.math.Box.prototype.round = function() {
  this.top = Math.round(this.top);
  this.right = Math.round(this.right);
  this.bottom = Math.round(this.bottom);
  this.left = Math.round(this.left);
  return this;
};
goog.math.Box.prototype.translate = function(a, b) {
  a instanceof goog.math.Coordinate ? (this.left += a.x, this.right += a.x, this.top += a.y, this.bottom += a.y) : (this.left += a, this.right += a, goog.isNumber(b) && (this.top += b, this.bottom += b));
  return this;
};
goog.math.Box.prototype.scale = function(a, b) {
  var c = goog.isNumber(b) ? b : a;
  this.left *= a;
  this.right *= a;
  this.top *= c;
  this.bottom *= c;
  return this;
};
goog.color.parse = function(a) {
  var b = {};
  a = String(a);
  var c = goog.color.prependHashIfNecessaryHelper(a);
  if (goog.color.isValidHexColor_(c)) {
    return b.hex = goog.color.normalizeHex(c), b.type = "hex", b;
  }
  c = goog.color.isValidRgbColor_(a);
  if (c.length) {
    return b.hex = goog.color.rgbArrayToHex(c), b.type = "rgb", b;
  }
  if (goog.color.names && (c = goog.color.names[a.toLowerCase()])) {
    return b.hex = c, b.type = "named", b;
  }
  throw Error(a + " is not a valid color string");
};
goog.color.isValidColor = function(a) {
  var b = goog.color.prependHashIfNecessaryHelper(a);
  return!!(goog.color.isValidHexColor_(b) || goog.color.isValidRgbColor_(a).length || goog.color.names && goog.color.names[a.toLowerCase()]);
};
goog.color.parseRgb = function(a) {
  var b = goog.color.isValidRgbColor_(a);
  if (!b.length) {
    throw Error(a + " is not a valid RGB color");
  }
  return b;
};
goog.color.hexToRgbStyle = function(a) {
  return goog.color.rgbStyle_(goog.color.hexToRgb(a));
};
goog.color.hexTripletRe_ = /#(.)(.)(.)/;
goog.color.normalizeHex = function(a) {
  if (!goog.color.isValidHexColor_(a)) {
    throw Error("'" + a + "' is not a valid hex color");
  }
  4 == a.length && (a = a.replace(goog.color.hexTripletRe_, "#$1$1$2$2$3$3"));
  return a.toLowerCase();
};
goog.color.hexToRgb = function(a) {
  a = goog.color.normalizeHex(a);
  var b = parseInt(a.substr(1, 2), 16), c = parseInt(a.substr(3, 2), 16);
  a = parseInt(a.substr(5, 2), 16);
  return[b, c, a];
};
goog.color.rgbToHex = function(a, b, c) {
  a = Number(a);
  b = Number(b);
  c = Number(c);
  if (isNaN(a) || 0 > a || 255 < a || isNaN(b) || 0 > b || 255 < b || isNaN(c) || 0 > c || 255 < c) {
    throw Error('"(' + a + "," + b + "," + c + '") is not a valid RGB color');
  }
  a = goog.color.prependZeroIfNecessaryHelper(a.toString(16));
  b = goog.color.prependZeroIfNecessaryHelper(b.toString(16));
  c = goog.color.prependZeroIfNecessaryHelper(c.toString(16));
  return "#" + a + b + c;
};
goog.color.rgbArrayToHex = function(a) {
  return goog.color.rgbToHex(a[0], a[1], a[2]);
};
goog.color.rgbToHsl = function(a, b, c) {
  a /= 255;
  b /= 255;
  c /= 255;
  var d = Math.max(a, b, c), e = Math.min(a, b, c), f = 0, g = 0, h = .5 * (d + e);
  d != e && (d == a ? f = 60 * (b - c) / (d - e) : d == b ? f = 60 * (c - a) / (d - e) + 120 : d == c && (f = 60 * (a - b) / (d - e) + 240), g = 0 < h && .5 >= h ? (d - e) / (2 * h) : (d - e) / (2 - 2 * h));
  return[Math.round(f + 360) % 360, g, h];
};
goog.color.rgbArrayToHsl = function(a) {
  return goog.color.rgbToHsl(a[0], a[1], a[2]);
};
goog.color.hueToRgb_ = function(a, b, c) {
  0 > c ? c += 1 : 1 < c && --c;
  return 1 > 6 * c ? a + 6 * (b - a) * c : 1 > 2 * c ? b : 2 > 3 * c ? a + (b - a) * (2 / 3 - c) * 6 : a;
};
goog.color.hslToRgb = function(a, b, c) {
  var d = 0, e = 0, f = 0;
  a /= 360;
  if (0 == b) {
    d = e = f = 255 * c;
  } else {
    var g = f = 0, g = .5 > c ? c * (1 + b) : c + b - b * c, f = 2 * c - g, d = 255 * goog.color.hueToRgb_(f, g, a + 1 / 3), e = 255 * goog.color.hueToRgb_(f, g, a), f = 255 * goog.color.hueToRgb_(f, g, a - 1 / 3)
  }
  return[Math.round(d), Math.round(e), Math.round(f)];
};
goog.color.hslArrayToRgb = function(a) {
  return goog.color.hslToRgb(a[0], a[1], a[2]);
};
goog.color.validHexColorRe_ = /^#(?:[0-9a-f]{3}){1,2}$/i;
goog.color.isValidHexColor_ = function(a) {
  return goog.color.validHexColorRe_.test(a);
};
goog.color.normalizedHexColorRe_ = /^#[0-9a-f]{6}$/;
goog.color.isNormalizedHexColor_ = function(a) {
  return goog.color.normalizedHexColorRe_.test(a);
};
goog.color.rgbColorRe_ = /^(?:rgb)?\((0|[1-9]\d{0,2}),\s?(0|[1-9]\d{0,2}),\s?(0|[1-9]\d{0,2})\)$/i;
goog.color.isValidRgbColor_ = function(a) {
  var b = a.match(goog.color.rgbColorRe_);
  if (b) {
    a = Number(b[1]);
    var c = Number(b[2]), b = Number(b[3]);
    if (0 <= a && 255 >= a && 0 <= c && 255 >= c && 0 <= b && 255 >= b) {
      return[a, c, b];
    }
  }
  return[];
};
goog.color.prependZeroIfNecessaryHelper = function(a) {
  return 1 == a.length ? "0" + a : a;
};
goog.color.prependHashIfNecessaryHelper = function(a) {
  return "#" == a.charAt(0) ? a : "#" + a;
};
goog.color.rgbStyle_ = function(a) {
  return "rgb(" + a.join(",") + ")";
};
goog.color.hsvToRgb = function(a, b, c) {
  var d = 0, e = 0, f = 0;
  if (0 == b) {
    f = e = d = c;
  } else {
    var g = Math.floor(a / 60), h = a / 60 - g;
    a = c * (1 - b);
    var k = c * (1 - b * h);
    b = c * (1 - b * (1 - h));
    switch(g) {
      case 1:
        d = k;
        e = c;
        f = a;
        break;
      case 2:
        d = a;
        e = c;
        f = b;
        break;
      case 3:
        d = a;
        e = k;
        f = c;
        break;
      case 4:
        d = b;
        e = a;
        f = c;
        break;
      case 5:
        d = c;
        e = a;
        f = k;
        break;
      case 6:
      ;
      case 0:
        d = c, e = b, f = a;
    }
  }
  return[Math.floor(d), Math.floor(e), Math.floor(f)];
};
goog.color.rgbToHsv = function(a, b, c) {
  var d = Math.max(Math.max(a, b), c), e = Math.min(Math.min(a, b), c);
  if (e == d) {
    e = a = 0;
  } else {
    var f = d - e, e = f / d;
    a = 60 * (a == d ? (b - c) / f : b == d ? 2 + (c - a) / f : 4 + (a - b) / f);
    0 > a && (a += 360);
    360 < a && (a -= 360);
  }
  return[a, e, d];
};
goog.color.rgbArrayToHsv = function(a) {
  return goog.color.rgbToHsv(a[0], a[1], a[2]);
};
goog.color.hsvArrayToRgb = function(a) {
  return goog.color.hsvToRgb(a[0], a[1], a[2]);
};
goog.color.hexToHsl = function(a) {
  a = goog.color.hexToRgb(a);
  return goog.color.rgbToHsl(a[0], a[1], a[2]);
};
goog.color.hslToHex = function(a, b, c) {
  return goog.color.rgbArrayToHex(goog.color.hslToRgb(a, b, c));
};
goog.color.hslArrayToHex = function(a) {
  return goog.color.rgbArrayToHex(goog.color.hslToRgb(a[0], a[1], a[2]));
};
goog.color.hexToHsv = function(a) {
  return goog.color.rgbArrayToHsv(goog.color.hexToRgb(a));
};
goog.color.hsvToHex = function(a, b, c) {
  return goog.color.rgbArrayToHex(goog.color.hsvToRgb(a, b, c));
};
goog.color.hsvArrayToHex = function(a) {
  return goog.color.hsvToHex(a[0], a[1], a[2]);
};
goog.color.hslDistance = function(a, b) {
  var c, d;
  c = .5 >= a[2] ? a[1] * a[2] : a[1] * (1 - a[2]);
  d = .5 >= b[2] ? b[1] * b[2] : b[1] * (1 - b[2]);
  return(a[2] - b[2]) * (a[2] - b[2]) + c * c + d * d - 2 * c * d * Math.cos(2 * (a[0] / 360 - b[0] / 360) * Math.PI);
};
goog.color.blend = function(a, b, c) {
  c = goog.math.clamp(c, 0, 1);
  return[Math.round(c * a[0] + (1 - c) * b[0]), Math.round(c * a[1] + (1 - c) * b[1]), Math.round(c * a[2] + (1 - c) * b[2])];
};
goog.color.darken = function(a, b) {
  return goog.color.blend([0, 0, 0], a, b);
};
goog.color.lighten = function(a, b) {
  return goog.color.blend([255, 255, 255], a, b);
};
goog.color.highContrast = function(a, b) {
  for (var c = [], d = 0;d < b.length;d++) {
    c.push({color:b[d], diff:goog.color.yiqBrightnessDiff_(b[d], a) + goog.color.colorDiff_(b[d], a)});
  }
  c.sort(function(a, b) {
    return b.diff - a.diff;
  });
  return c[0].color;
};
goog.color.yiqBrightness_ = function(a) {
  return Math.round((299 * a[0] + 587 * a[1] + 114 * a[2]) / 1E3);
};
goog.color.yiqBrightnessDiff_ = function(a, b) {
  return Math.abs(goog.color.yiqBrightness_(a) - goog.color.yiqBrightness_(b));
};
goog.color.colorDiff_ = function(a, b) {
  return Math.abs(a[0] - b[0]) + Math.abs(a[1] - b[1]) + Math.abs(a[2] - b[2]);
};
goog.userAgent = {};
goog.userAgent.ASSUME_IE = !1;
goog.userAgent.ASSUME_GECKO = !1;
goog.userAgent.ASSUME_WEBKIT = !1;
goog.userAgent.ASSUME_MOBILE_WEBKIT = !1;
goog.userAgent.ASSUME_OPERA = !1;
goog.userAgent.ASSUME_ANY_VERSION = !1;
goog.userAgent.BROWSER_KNOWN_ = goog.userAgent.ASSUME_IE || goog.userAgent.ASSUME_GECKO || goog.userAgent.ASSUME_MOBILE_WEBKIT || goog.userAgent.ASSUME_WEBKIT || goog.userAgent.ASSUME_OPERA;
goog.userAgent.getUserAgentString = function() {
  return goog.labs.userAgent.util.getUserAgent();
};
goog.userAgent.getNavigator = function() {
  return goog.global.navigator || null;
};
goog.userAgent.OPERA = goog.userAgent.BROWSER_KNOWN_ ? goog.userAgent.ASSUME_OPERA : goog.labs.userAgent.browser.isOpera();
goog.userAgent.IE = goog.userAgent.BROWSER_KNOWN_ ? goog.userAgent.ASSUME_IE : goog.labs.userAgent.browser.isIE();
goog.userAgent.GECKO = goog.userAgent.BROWSER_KNOWN_ ? goog.userAgent.ASSUME_GECKO : goog.labs.userAgent.engine.isGecko();
goog.userAgent.WEBKIT = goog.userAgent.BROWSER_KNOWN_ ? goog.userAgent.ASSUME_WEBKIT || goog.userAgent.ASSUME_MOBILE_WEBKIT : goog.labs.userAgent.engine.isWebKit();
goog.userAgent.isMobile_ = function() {
  return goog.userAgent.WEBKIT && goog.labs.userAgent.util.matchUserAgent("Mobile");
};
goog.userAgent.MOBILE = goog.userAgent.ASSUME_MOBILE_WEBKIT || goog.userAgent.isMobile_();
goog.userAgent.SAFARI = goog.userAgent.WEBKIT;
goog.userAgent.determinePlatform_ = function() {
  var a = goog.userAgent.getNavigator();
  return a && a.platform || "";
};
goog.userAgent.PLATFORM = goog.userAgent.determinePlatform_();
goog.userAgent.ASSUME_MAC = !1;
goog.userAgent.ASSUME_WINDOWS = !1;
goog.userAgent.ASSUME_LINUX = !1;
goog.userAgent.ASSUME_X11 = !1;
goog.userAgent.ASSUME_ANDROID = !1;
goog.userAgent.ASSUME_IPHONE = !1;
goog.userAgent.ASSUME_IPAD = !1;
goog.userAgent.PLATFORM_KNOWN_ = goog.userAgent.ASSUME_MAC || goog.userAgent.ASSUME_WINDOWS || goog.userAgent.ASSUME_LINUX || goog.userAgent.ASSUME_X11 || goog.userAgent.ASSUME_ANDROID || goog.userAgent.ASSUME_IPHONE || goog.userAgent.ASSUME_IPAD;
goog.userAgent.initPlatform_ = function() {
  goog.userAgent.detectedMac_ = goog.string.contains(goog.userAgent.PLATFORM, "Mac");
  goog.userAgent.detectedWindows_ = goog.string.contains(goog.userAgent.PLATFORM, "Win");
  goog.userAgent.detectedLinux_ = goog.string.contains(goog.userAgent.PLATFORM, "Linux");
  var a = goog.userAgent.getUserAgentString();
  goog.userAgent.detectedAndroid_ = !!a && goog.string.contains(a, "Android");
  goog.userAgent.detectedIPhone_ = !!a && goog.string.contains(a, "iPhone");
  goog.userAgent.detectedIPad_ = !!a && goog.string.contains(a, "iPad");
};
goog.userAgent.PLATFORM_KNOWN_ || goog.userAgent.initPlatform_();
goog.userAgent.MAC = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_MAC : goog.userAgent.detectedMac_;
goog.userAgent.WINDOWS = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_WINDOWS : goog.userAgent.detectedWindows_;
goog.userAgent.LINUX = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_LINUX : goog.userAgent.detectedLinux_;
goog.userAgent.isX11_ = function() {
  var a = goog.userAgent.getNavigator();
  return!!a && goog.string.contains(a.appVersion || "", "X11");
};
goog.userAgent.X11 = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_X11 : goog.userAgent.isX11_();
goog.userAgent.ANDROID = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_ANDROID : goog.userAgent.detectedAndroid_;
goog.userAgent.IPHONE = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_IPHONE : goog.userAgent.detectedIPhone_;
goog.userAgent.IPAD = goog.userAgent.PLATFORM_KNOWN_ ? goog.userAgent.ASSUME_IPAD : goog.userAgent.detectedIPad_;
goog.userAgent.determineVersion_ = function() {
  var a = "", b;
  if (goog.userAgent.OPERA && goog.global.opera) {
    return a = goog.global.opera.version, goog.isFunction(a) ? a() : a;
  }
  goog.userAgent.GECKO ? b = /rv\:([^\);]+)(\)|;)/ : goog.userAgent.IE ? b = /\b(?:MSIE|rv)[: ]([^\);]+)(\)|;)/ : goog.userAgent.WEBKIT && (b = /WebKit\/(\S+)/);
  b && (a = (a = b.exec(goog.userAgent.getUserAgentString())) ? a[1] : "");
  return goog.userAgent.IE && (b = goog.userAgent.getDocumentMode_(), b > parseFloat(a)) ? String(b) : a;
};
goog.userAgent.getDocumentMode_ = function() {
  var a = goog.global.document;
  return a ? a.documentMode : void 0;
};
goog.userAgent.VERSION = goog.userAgent.determineVersion_();
goog.userAgent.compare = function(a, b) {
  return goog.string.compareVersions(a, b);
};
goog.userAgent.isVersionOrHigherCache_ = {};
goog.userAgent.isVersionOrHigher = function(a) {
  return goog.userAgent.ASSUME_ANY_VERSION || goog.userAgent.isVersionOrHigherCache_[a] || (goog.userAgent.isVersionOrHigherCache_[a] = 0 <= goog.string.compareVersions(goog.userAgent.VERSION, a));
};
goog.userAgent.isVersion = goog.userAgent.isVersionOrHigher;
goog.userAgent.isDocumentModeOrHigher = function(a) {
  return goog.userAgent.IE && goog.userAgent.DOCUMENT_MODE >= a;
};
goog.userAgent.isDocumentMode = goog.userAgent.isDocumentModeOrHigher;
goog.userAgent.DOCUMENT_MODE = function() {
  var a = goog.global.document;
  return a && goog.userAgent.IE ? goog.userAgent.getDocumentMode_() || ("CSS1Compat" == a.compatMode ? parseInt(goog.userAgent.VERSION, 10) : 5) : void 0;
}();
goog.dom.TagName = {A:"A", ABBR:"ABBR", ACRONYM:"ACRONYM", ADDRESS:"ADDRESS", APPLET:"APPLET", AREA:"AREA", ARTICLE:"ARTICLE", ASIDE:"ASIDE", AUDIO:"AUDIO", B:"B", BASE:"BASE", BASEFONT:"BASEFONT", BDI:"BDI", BDO:"BDO", BIG:"BIG", BLOCKQUOTE:"BLOCKQUOTE", BODY:"BODY", BR:"BR", BUTTON:"BUTTON", CANVAS:"CANVAS", CAPTION:"CAPTION", CENTER:"CENTER", CITE:"CITE", CODE:"CODE", COL:"COL", COLGROUP:"COLGROUP", COMMAND:"COMMAND", DATA:"DATA", DATALIST:"DATALIST", DD:"DD", DEL:"DEL", DETAILS:"DETAILS", DFN:"DFN", 
DIALOG:"DIALOG", DIR:"DIR", DIV:"DIV", DL:"DL", DT:"DT", EM:"EM", EMBED:"EMBED", FIELDSET:"FIELDSET", FIGCAPTION:"FIGCAPTION", FIGURE:"FIGURE", FONT:"FONT", FOOTER:"FOOTER", FORM:"FORM", FRAME:"FRAME", FRAMESET:"FRAMESET", H1:"H1", H2:"H2", H3:"H3", H4:"H4", H5:"H5", H6:"H6", HEAD:"HEAD", HEADER:"HEADER", HGROUP:"HGROUP", HR:"HR", HTML:"HTML", I:"I", IFRAME:"IFRAME", IMG:"IMG", INPUT:"INPUT", INS:"INS", ISINDEX:"ISINDEX", KBD:"KBD", KEYGEN:"KEYGEN", LABEL:"LABEL", LEGEND:"LEGEND", LI:"LI", LINK:"LINK", 
MAP:"MAP", MARK:"MARK", MATH:"MATH", MENU:"MENU", META:"META", METER:"METER", NAV:"NAV", NOFRAMES:"NOFRAMES", NOSCRIPT:"NOSCRIPT", OBJECT:"OBJECT", OL:"OL", OPTGROUP:"OPTGROUP", OPTION:"OPTION", OUTPUT:"OUTPUT", P:"P", PARAM:"PARAM", PRE:"PRE", PROGRESS:"PROGRESS", Q:"Q", RP:"RP", RT:"RT", RUBY:"RUBY", S:"S", SAMP:"SAMP", SCRIPT:"SCRIPT", SECTION:"SECTION", SELECT:"SELECT", SMALL:"SMALL", SOURCE:"SOURCE", SPAN:"SPAN", STRIKE:"STRIKE", STRONG:"STRONG", STYLE:"STYLE", SUB:"SUB", SUMMARY:"SUMMARY", 
SUP:"SUP", SVG:"SVG", TABLE:"TABLE", TBODY:"TBODY", TD:"TD", TEXTAREA:"TEXTAREA", TFOOT:"TFOOT", TH:"TH", THEAD:"THEAD", TIME:"TIME", TITLE:"TITLE", TR:"TR", TRACK:"TRACK", TT:"TT", U:"U", UL:"UL", VAR:"VAR", VIDEO:"VIDEO", WBR:"WBR"};
goog.dom.classlist = {};
goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST = !1;
goog.dom.classlist.get = function(a) {
  if (goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList) {
    return a.classList;
  }
  a = a.className;
  return goog.isString(a) && a.match(/\S+/g) || [];
};
goog.dom.classlist.set = function(a, b) {
  a.className = b;
};
goog.dom.classlist.contains = function(a, b) {
  return goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList ? a.classList.contains(b) : goog.array.contains(goog.dom.classlist.get(a), b);
};
goog.dom.classlist.add = function(a, b) {
  goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList ? a.classList.add(b) : goog.dom.classlist.contains(a, b) || (a.className += 0 < a.className.length ? " " + b : b);
};
goog.dom.classlist.addAll = function(a, b) {
  if (goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList) {
    goog.array.forEach(b, function(b) {
      goog.dom.classlist.add(a, b);
    });
  } else {
    var c = {};
    goog.array.forEach(goog.dom.classlist.get(a), function(a) {
      c[a] = !0;
    });
    goog.array.forEach(b, function(a) {
      c[a] = !0;
    });
    a.className = "";
    for (var d in c) {
      a.className += 0 < a.className.length ? " " + d : d;
    }
  }
};
goog.dom.classlist.remove = function(a, b) {
  goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList ? a.classList.remove(b) : goog.dom.classlist.contains(a, b) && (a.className = goog.array.filter(goog.dom.classlist.get(a), function(a) {
    return a != b;
  }).join(" "));
};
goog.dom.classlist.removeAll = function(a, b) {
  goog.dom.classlist.ALWAYS_USE_DOM_TOKEN_LIST || a.classList ? goog.array.forEach(b, function(b) {
    goog.dom.classlist.remove(a, b);
  }) : a.className = goog.array.filter(goog.dom.classlist.get(a), function(a) {
    return!goog.array.contains(b, a);
  }).join(" ");
};
goog.dom.classlist.enable = function(a, b, c) {
  c ? goog.dom.classlist.add(a, b) : goog.dom.classlist.remove(a, b);
};
goog.dom.classlist.enableAll = function(a, b, c) {
  (c ? goog.dom.classlist.addAll : goog.dom.classlist.removeAll)(a, b);
};
goog.dom.classlist.swap = function(a, b, c) {
  return goog.dom.classlist.contains(a, b) ? (goog.dom.classlist.remove(a, b), goog.dom.classlist.add(a, c), !0) : !1;
};
goog.dom.classlist.toggle = function(a, b) {
  var c = !goog.dom.classlist.contains(a, b);
  goog.dom.classlist.enable(a, b, c);
  return c;
};
goog.dom.classlist.addRemove = function(a, b, c) {
  goog.dom.classlist.remove(a, b);
  goog.dom.classlist.add(a, c);
};
goog.dom.BrowserFeature = {CAN_ADD_NAME_OR_TYPE_ATTRIBUTES:!goog.userAgent.IE || goog.userAgent.isDocumentModeOrHigher(9), CAN_USE_CHILDREN_ATTRIBUTE:!goog.userAgent.GECKO && !goog.userAgent.IE || goog.userAgent.IE && goog.userAgent.isDocumentModeOrHigher(9) || goog.userAgent.GECKO && goog.userAgent.isVersionOrHigher("1.9.1"), CAN_USE_INNER_TEXT:goog.userAgent.IE && !goog.userAgent.isVersionOrHigher("9"), CAN_USE_PARENT_ELEMENT_PROPERTY:goog.userAgent.IE || goog.userAgent.OPERA || goog.userAgent.WEBKIT, 
INNER_HTML_NEEDS_SCOPED_ELEMENT:goog.userAgent.IE, LEGACY_IE_RANGES:goog.userAgent.IE && !goog.userAgent.isDocumentModeOrHigher(9)};
goog.dom.ASSUME_QUIRKS_MODE = !1;
goog.dom.ASSUME_STANDARDS_MODE = !1;
goog.dom.COMPAT_MODE_KNOWN_ = goog.dom.ASSUME_QUIRKS_MODE || goog.dom.ASSUME_STANDARDS_MODE;
goog.dom.getDomHelper = function(a) {
  return a ? new goog.dom.DomHelper(goog.dom.getOwnerDocument(a)) : goog.dom.defaultDomHelper_ || (goog.dom.defaultDomHelper_ = new goog.dom.DomHelper);
};
goog.dom.getDocument = function() {
  return document;
};
goog.dom.getElement = function(a) {
  return goog.dom.getElementHelper_(document, a);
};
goog.dom.getElementHelper_ = function(a, b) {
  return goog.isString(b) ? a.getElementById(b) : b;
};
goog.dom.getRequiredElement = function(a) {
  return goog.dom.getRequiredElementHelper_(document, a);
};
goog.dom.getRequiredElementHelper_ = function(a, b) {
  goog.asserts.assertString(b);
  var c = goog.dom.getElementHelper_(a, b);
  return c = goog.asserts.assertElement(c, "No element found with id: " + b);
};
goog.dom.$ = goog.dom.getElement;
goog.dom.getElementsByTagNameAndClass = function(a, b, c) {
  return goog.dom.getElementsByTagNameAndClass_(document, a, b, c);
};
goog.dom.getElementsByClass = function(a, b) {
  var c = b || document;
  return goog.dom.canUseQuerySelector_(c) ? c.querySelectorAll("." + a) : goog.dom.getElementsByTagNameAndClass_(document, "*", a, b);
};
goog.dom.getElementByClass = function(a, b) {
  var c = b || document, d = null;
  return(d = goog.dom.canUseQuerySelector_(c) ? c.querySelector("." + a) : goog.dom.getElementsByTagNameAndClass_(document, "*", a, b)[0]) || null;
};
goog.dom.getRequiredElementByClass = function(a, b) {
  var c = goog.dom.getElementByClass(a, b);
  return goog.asserts.assert(c, "No element found with className: " + a);
};
goog.dom.canUseQuerySelector_ = function(a) {
  return!(!a.querySelectorAll || !a.querySelector);
};
goog.dom.getElementsByTagNameAndClass_ = function(a, b, c, d) {
  a = d || a;
  b = b && "*" != b ? b.toUpperCase() : "";
  if (goog.dom.canUseQuerySelector_(a) && (b || c)) {
    return a.querySelectorAll(b + (c ? "." + c : ""));
  }
  if (c && a.getElementsByClassName) {
    a = a.getElementsByClassName(c);
    if (b) {
      d = {};
      for (var e = 0, f = 0, g;g = a[f];f++) {
        b == g.nodeName && (d[e++] = g);
      }
      d.length = e;
      return d;
    }
    return a;
  }
  a = a.getElementsByTagName(b || "*");
  if (c) {
    d = {};
    for (f = e = 0;g = a[f];f++) {
      b = g.className, "function" == typeof b.split && goog.array.contains(b.split(/\s+/), c) && (d[e++] = g);
    }
    d.length = e;
    return d;
  }
  return a;
};
goog.dom.$$ = goog.dom.getElementsByTagNameAndClass;
goog.dom.setProperties = function(a, b) {
  goog.object.forEach(b, function(b, d) {
    "style" == d ? a.style.cssText = b : "class" == d ? a.className = b : "for" == d ? a.htmlFor = b : d in goog.dom.DIRECT_ATTRIBUTE_MAP_ ? a.setAttribute(goog.dom.DIRECT_ATTRIBUTE_MAP_[d], b) : goog.string.startsWith(d, "aria-") || goog.string.startsWith(d, "data-") ? a.setAttribute(d, b) : a[d] = b;
  });
};
goog.dom.DIRECT_ATTRIBUTE_MAP_ = {cellpadding:"cellPadding", cellspacing:"cellSpacing", colspan:"colSpan", frameborder:"frameBorder", height:"height", maxlength:"maxLength", role:"role", rowspan:"rowSpan", type:"type", usemap:"useMap", valign:"vAlign", width:"width"};
goog.dom.getViewportSize = function(a) {
  return goog.dom.getViewportSize_(a || window);
};
goog.dom.getViewportSize_ = function(a) {
  a = a.document;
  a = goog.dom.isCss1CompatMode_(a) ? a.documentElement : a.body;
  return new goog.math.Size(a.clientWidth, a.clientHeight);
};
goog.dom.getDocumentHeight = function() {
  return goog.dom.getDocumentHeight_(window);
};
goog.dom.getDocumentHeight_ = function(a) {
  var b = a.document, c = 0;
  if (b) {
    var c = b.body, d = b.documentElement;
    if (!d || !c) {
      return 0;
    }
    a = goog.dom.getViewportSize_(a).height;
    if (goog.dom.isCss1CompatMode_(b) && d.scrollHeight) {
      c = d.scrollHeight != a ? d.scrollHeight : d.offsetHeight;
    } else {
      var b = d.scrollHeight, e = d.offsetHeight;
      d.clientHeight != e && (b = c.scrollHeight, e = c.offsetHeight);
      c = b > a ? b > e ? b : e : b < e ? b : e;
    }
  }
  return c;
};
goog.dom.getPageScroll = function(a) {
  return goog.dom.getDomHelper((a || goog.global || window).document).getDocumentScroll();
};
goog.dom.getDocumentScroll = function() {
  return goog.dom.getDocumentScroll_(document);
};
goog.dom.getDocumentScroll_ = function(a) {
  var b = goog.dom.getDocumentScrollElement_(a);
  a = goog.dom.getWindow_(a);
  return goog.userAgent.IE && goog.userAgent.isVersionOrHigher("10") && a.pageYOffset != b.scrollTop ? new goog.math.Coordinate(b.scrollLeft, b.scrollTop) : new goog.math.Coordinate(a.pageXOffset || b.scrollLeft, a.pageYOffset || b.scrollTop);
};
goog.dom.getDocumentScrollElement = function() {
  return goog.dom.getDocumentScrollElement_(document);
};
goog.dom.getDocumentScrollElement_ = function(a) {
  return!goog.userAgent.WEBKIT && goog.dom.isCss1CompatMode_(a) ? a.documentElement : a.body || a.documentElement;
};
goog.dom.getWindow = function(a) {
  return a ? goog.dom.getWindow_(a) : window;
};
goog.dom.getWindow_ = function(a) {
  return a.parentWindow || a.defaultView;
};
goog.dom.createDom = function(a, b, c) {
  return goog.dom.createDom_(document, arguments);
};
goog.dom.createDom_ = function(a, b) {
  var c = b[0], d = b[1];
  if (!goog.dom.BrowserFeature.CAN_ADD_NAME_OR_TYPE_ATTRIBUTES && d && (d.name || d.type)) {
    c = ["<", c];
    d.name && c.push(' name="', goog.string.htmlEscape(d.name), '"');
    if (d.type) {
      c.push(' type="', goog.string.htmlEscape(d.type), '"');
      var e = {};
      goog.object.extend(e, d);
      delete e.type;
      d = e;
    }
    c.push(">");
    c = c.join("");
  }
  c = a.createElement(c);
  d && (goog.isString(d) ? c.className = d : goog.isArray(d) ? c.className = d.join(" ") : goog.dom.setProperties(c, d));
  2 < b.length && goog.dom.append_(a, c, b, 2);
  return c;
};
goog.dom.append_ = function(a, b, c, d) {
  function e(c) {
    c && b.appendChild(goog.isString(c) ? a.createTextNode(c) : c);
  }
  for (;d < c.length;d++) {
    var f = c[d];
    goog.isArrayLike(f) && !goog.dom.isNodeLike(f) ? goog.array.forEach(goog.dom.isNodeList(f) ? goog.array.toArray(f) : f, e) : e(f);
  }
};
goog.dom.$dom = goog.dom.createDom;
goog.dom.createElement = function(a) {
  return document.createElement(a);
};
goog.dom.createTextNode = function(a) {
  return document.createTextNode(String(a));
};
goog.dom.createTable = function(a, b, c) {
  return goog.dom.createTable_(document, a, b, !!c);
};
goog.dom.createTable_ = function(a, b, c, d) {
  for (var e = ["<tr>"], f = 0;f < c;f++) {
    e.push(d ? "<td>&nbsp;</td>" : "<td></td>");
  }
  e.push("</tr>");
  e = e.join("");
  c = ["<table>"];
  for (f = 0;f < b;f++) {
    c.push(e);
  }
  c.push("</table>");
  a = a.createElement(goog.dom.TagName.DIV);
  a.innerHTML = c.join("");
  return a.removeChild(a.firstChild);
};
goog.dom.htmlToDocumentFragment = function(a) {
  return goog.dom.htmlToDocumentFragment_(document, a);
};
goog.dom.htmlToDocumentFragment_ = function(a, b) {
  var c = a.createElement("div");
  goog.dom.BrowserFeature.INNER_HTML_NEEDS_SCOPED_ELEMENT ? (c.innerHTML = "<br>" + b, c.removeChild(c.firstChild)) : c.innerHTML = b;
  if (1 == c.childNodes.length) {
    return c.removeChild(c.firstChild);
  }
  for (var d = a.createDocumentFragment();c.firstChild;) {
    d.appendChild(c.firstChild);
  }
  return d;
};
goog.dom.isCss1CompatMode = function() {
  return goog.dom.isCss1CompatMode_(document);
};
goog.dom.isCss1CompatMode_ = function(a) {
  return goog.dom.COMPAT_MODE_KNOWN_ ? goog.dom.ASSUME_STANDARDS_MODE : "CSS1Compat" == a.compatMode;
};
goog.dom.canHaveChildren = function(a) {
  if (a.nodeType != goog.dom.NodeType.ELEMENT) {
    return!1;
  }
  switch(a.tagName) {
    case goog.dom.TagName.APPLET:
    ;
    case goog.dom.TagName.AREA:
    ;
    case goog.dom.TagName.BASE:
    ;
    case goog.dom.TagName.BR:
    ;
    case goog.dom.TagName.COL:
    ;
    case goog.dom.TagName.COMMAND:
    ;
    case goog.dom.TagName.EMBED:
    ;
    case goog.dom.TagName.FRAME:
    ;
    case goog.dom.TagName.HR:
    ;
    case goog.dom.TagName.IMG:
    ;
    case goog.dom.TagName.INPUT:
    ;
    case goog.dom.TagName.IFRAME:
    ;
    case goog.dom.TagName.ISINDEX:
    ;
    case goog.dom.TagName.KEYGEN:
    ;
    case goog.dom.TagName.LINK:
    ;
    case goog.dom.TagName.NOFRAMES:
    ;
    case goog.dom.TagName.NOSCRIPT:
    ;
    case goog.dom.TagName.META:
    ;
    case goog.dom.TagName.OBJECT:
    ;
    case goog.dom.TagName.PARAM:
    ;
    case goog.dom.TagName.SCRIPT:
    ;
    case goog.dom.TagName.SOURCE:
    ;
    case goog.dom.TagName.STYLE:
    ;
    case goog.dom.TagName.TRACK:
    ;
    case goog.dom.TagName.WBR:
      return!1;
  }
  return!0;
};
goog.dom.appendChild = function(a, b) {
  a.appendChild(b);
};
goog.dom.append = function(a, b) {
  goog.dom.append_(goog.dom.getOwnerDocument(a), a, arguments, 1);
};
goog.dom.removeChildren = function(a) {
  for (var b;b = a.firstChild;) {
    a.removeChild(b);
  }
};
goog.dom.insertSiblingBefore = function(a, b) {
  b.parentNode && b.parentNode.insertBefore(a, b);
};
goog.dom.insertSiblingAfter = function(a, b) {
  b.parentNode && b.parentNode.insertBefore(a, b.nextSibling);
};
goog.dom.insertChildAt = function(a, b, c) {
  a.insertBefore(b, a.childNodes[c] || null);
};
goog.dom.removeNode = function(a) {
  return a && a.parentNode ? a.parentNode.removeChild(a) : null;
};
goog.dom.replaceNode = function(a, b) {
  var c = b.parentNode;
  c && c.replaceChild(a, b);
};
goog.dom.flattenElement = function(a) {
  var b, c = a.parentNode;
  if (c && c.nodeType != goog.dom.NodeType.DOCUMENT_FRAGMENT) {
    if (a.removeNode) {
      return a.removeNode(!1);
    }
    for (;b = a.firstChild;) {
      c.insertBefore(b, a);
    }
    return goog.dom.removeNode(a);
  }
};
goog.dom.getChildren = function(a) {
  return goog.dom.BrowserFeature.CAN_USE_CHILDREN_ATTRIBUTE && void 0 != a.children ? a.children : goog.array.filter(a.childNodes, function(a) {
    return a.nodeType == goog.dom.NodeType.ELEMENT;
  });
};
goog.dom.getFirstElementChild = function(a) {
  return void 0 != a.firstElementChild ? a.firstElementChild : goog.dom.getNextElementNode_(a.firstChild, !0);
};
goog.dom.getLastElementChild = function(a) {
  return void 0 != a.lastElementChild ? a.lastElementChild : goog.dom.getNextElementNode_(a.lastChild, !1);
};
goog.dom.getNextElementSibling = function(a) {
  return void 0 != a.nextElementSibling ? a.nextElementSibling : goog.dom.getNextElementNode_(a.nextSibling, !0);
};
goog.dom.getPreviousElementSibling = function(a) {
  return void 0 != a.previousElementSibling ? a.previousElementSibling : goog.dom.getNextElementNode_(a.previousSibling, !1);
};
goog.dom.getNextElementNode_ = function(a, b) {
  for (;a && a.nodeType != goog.dom.NodeType.ELEMENT;) {
    a = b ? a.nextSibling : a.previousSibling;
  }
  return a;
};
goog.dom.getNextNode = function(a) {
  if (!a) {
    return null;
  }
  if (a.firstChild) {
    return a.firstChild;
  }
  for (;a && !a.nextSibling;) {
    a = a.parentNode;
  }
  return a ? a.nextSibling : null;
};
goog.dom.getPreviousNode = function(a) {
  if (!a) {
    return null;
  }
  if (!a.previousSibling) {
    return a.parentNode;
  }
  for (a = a.previousSibling;a && a.lastChild;) {
    a = a.lastChild;
  }
  return a;
};
goog.dom.isNodeLike = function(a) {
  return goog.isObject(a) && 0 < a.nodeType;
};
goog.dom.isElement = function(a) {
  return goog.isObject(a) && a.nodeType == goog.dom.NodeType.ELEMENT;
};
goog.dom.isWindow = function(a) {
  return goog.isObject(a) && a.window == a;
};
goog.dom.getParentElement = function(a) {
  var b;
  if (goog.dom.BrowserFeature.CAN_USE_PARENT_ELEMENT_PROPERTY && !(goog.userAgent.IE && goog.userAgent.isVersionOrHigher("9") && !goog.userAgent.isVersionOrHigher("10") && goog.global.SVGElement && a instanceof goog.global.SVGElement) && (b = a.parentElement)) {
    return b;
  }
  b = a.parentNode;
  return goog.dom.isElement(b) ? b : null;
};
goog.dom.contains = function(a, b) {
  if (a.contains && b.nodeType == goog.dom.NodeType.ELEMENT) {
    return a == b || a.contains(b);
  }
  if ("undefined" != typeof a.compareDocumentPosition) {
    return a == b || Boolean(a.compareDocumentPosition(b) & 16);
  }
  for (;b && a != b;) {
    b = b.parentNode;
  }
  return b == a;
};
goog.dom.compareNodeOrder = function(a, b) {
  if (a == b) {
    return 0;
  }
  if (a.compareDocumentPosition) {
    return a.compareDocumentPosition(b) & 2 ? 1 : -1;
  }
  if (goog.userAgent.IE && !goog.userAgent.isDocumentModeOrHigher(9)) {
    if (a.nodeType == goog.dom.NodeType.DOCUMENT) {
      return-1;
    }
    if (b.nodeType == goog.dom.NodeType.DOCUMENT) {
      return 1;
    }
  }
  if ("sourceIndex" in a || a.parentNode && "sourceIndex" in a.parentNode) {
    var c = a.nodeType == goog.dom.NodeType.ELEMENT, d = b.nodeType == goog.dom.NodeType.ELEMENT;
    if (c && d) {
      return a.sourceIndex - b.sourceIndex;
    }
    var e = a.parentNode, f = b.parentNode;
    return e == f ? goog.dom.compareSiblingOrder_(a, b) : !c && goog.dom.contains(e, b) ? -1 * goog.dom.compareParentsDescendantNodeIe_(a, b) : !d && goog.dom.contains(f, a) ? goog.dom.compareParentsDescendantNodeIe_(b, a) : (c ? a.sourceIndex : e.sourceIndex) - (d ? b.sourceIndex : f.sourceIndex);
  }
  d = goog.dom.getOwnerDocument(a);
  c = d.createRange();
  c.selectNode(a);
  c.collapse(!0);
  d = d.createRange();
  d.selectNode(b);
  d.collapse(!0);
  return c.compareBoundaryPoints(goog.global.Range.START_TO_END, d);
};
goog.dom.compareParentsDescendantNodeIe_ = function(a, b) {
  var c = a.parentNode;
  if (c == b) {
    return-1;
  }
  for (var d = b;d.parentNode != c;) {
    d = d.parentNode;
  }
  return goog.dom.compareSiblingOrder_(d, a);
};
goog.dom.compareSiblingOrder_ = function(a, b) {
  for (var c = b;c = c.previousSibling;) {
    if (c == a) {
      return-1;
    }
  }
  return 1;
};
goog.dom.findCommonAncestor = function(a) {
  var b, c = arguments.length;
  if (!c) {
    return null;
  }
  if (1 == c) {
    return arguments[0];
  }
  var d = [], e = Infinity;
  for (b = 0;b < c;b++) {
    for (var f = [], g = arguments[b];g;) {
      f.unshift(g), g = g.parentNode;
    }
    d.push(f);
    e = Math.min(e, f.length);
  }
  f = null;
  for (b = 0;b < e;b++) {
    for (var g = d[0][b], h = 1;h < c;h++) {
      if (g != d[h][b]) {
        return f;
      }
    }
    f = g;
  }
  return f;
};
goog.dom.getOwnerDocument = function(a) {
  goog.asserts.assert(a, "Node cannot be null or undefined.");
  return a.nodeType == goog.dom.NodeType.DOCUMENT ? a : a.ownerDocument || a.document;
};
goog.dom.getFrameContentDocument = function(a) {
  return a.contentDocument || a.contentWindow.document;
};
goog.dom.getFrameContentWindow = function(a) {
  return a.contentWindow || goog.dom.getWindow(goog.dom.getFrameContentDocument(a));
};
goog.dom.setTextContent = function(a, b) {
  goog.asserts.assert(null != a, "goog.dom.setTextContent expects a non-null value for node");
  if ("textContent" in a) {
    a.textContent = b;
  } else {
    if (a.nodeType == goog.dom.NodeType.TEXT) {
      a.data = b;
    } else {
      if (a.firstChild && a.firstChild.nodeType == goog.dom.NodeType.TEXT) {
        for (;a.lastChild != a.firstChild;) {
          a.removeChild(a.lastChild);
        }
        a.firstChild.data = b;
      } else {
        goog.dom.removeChildren(a);
        var c = goog.dom.getOwnerDocument(a);
        a.appendChild(c.createTextNode(String(b)));
      }
    }
  }
};
goog.dom.getOuterHtml = function(a) {
  if ("outerHTML" in a) {
    return a.outerHTML;
  }
  var b = goog.dom.getOwnerDocument(a).createElement("div");
  b.appendChild(a.cloneNode(!0));
  return b.innerHTML;
};
goog.dom.findNode = function(a, b) {
  var c = [];
  return goog.dom.findNodes_(a, b, c, !0) ? c[0] : void 0;
};
goog.dom.findNodes = function(a, b) {
  var c = [];
  goog.dom.findNodes_(a, b, c, !1);
  return c;
};
goog.dom.findNodes_ = function(a, b, c, d) {
  if (null != a) {
    for (a = a.firstChild;a;) {
      if (b(a) && (c.push(a), d) || goog.dom.findNodes_(a, b, c, d)) {
        return!0;
      }
      a = a.nextSibling;
    }
  }
  return!1;
};
goog.dom.TAGS_TO_IGNORE_ = {SCRIPT:1, STYLE:1, HEAD:1, IFRAME:1, OBJECT:1};
goog.dom.PREDEFINED_TAG_VALUES_ = {IMG:" ", BR:"\n"};
goog.dom.isFocusableTabIndex = function(a) {
  return goog.dom.hasSpecifiedTabIndex_(a) && goog.dom.isTabIndexFocusable_(a);
};
goog.dom.setFocusableTabIndex = function(a, b) {
  b ? a.tabIndex = 0 : (a.tabIndex = -1, a.removeAttribute("tabIndex"));
};
goog.dom.isFocusable = function(a) {
  var b;
  return(b = goog.dom.nativelySupportsFocus_(a) ? !a.disabled && (!goog.dom.hasSpecifiedTabIndex_(a) || goog.dom.isTabIndexFocusable_(a)) : goog.dom.isFocusableTabIndex(a)) && goog.userAgent.IE ? goog.dom.hasNonZeroBoundingRect_(a) : b;
};
goog.dom.hasSpecifiedTabIndex_ = function(a) {
  a = a.getAttributeNode("tabindex");
  return goog.isDefAndNotNull(a) && a.specified;
};
goog.dom.isTabIndexFocusable_ = function(a) {
  a = a.tabIndex;
  return goog.isNumber(a) && 0 <= a && 32768 > a;
};
goog.dom.nativelySupportsFocus_ = function(a) {
  return a.tagName == goog.dom.TagName.A || a.tagName == goog.dom.TagName.INPUT || a.tagName == goog.dom.TagName.TEXTAREA || a.tagName == goog.dom.TagName.SELECT || a.tagName == goog.dom.TagName.BUTTON;
};
goog.dom.hasNonZeroBoundingRect_ = function(a) {
  a = goog.isFunction(a.getBoundingClientRect) ? a.getBoundingClientRect() : {height:a.offsetHeight, width:a.offsetWidth};
  return goog.isDefAndNotNull(a) && 0 < a.height && 0 < a.width;
};
goog.dom.getTextContent = function(a) {
  if (goog.dom.BrowserFeature.CAN_USE_INNER_TEXT && "innerText" in a) {
    a = goog.string.canonicalizeNewlines(a.innerText);
  } else {
    var b = [];
    goog.dom.getTextContent_(a, b, !0);
    a = b.join("");
  }
  a = a.replace(/ \xAD /g, " ").replace(/\xAD/g, "");
  a = a.replace(/\u200B/g, "");
  goog.dom.BrowserFeature.CAN_USE_INNER_TEXT || (a = a.replace(/ +/g, " "));
  " " != a && (a = a.replace(/^\s*/, ""));
  return a;
};
goog.dom.getRawTextContent = function(a) {
  var b = [];
  goog.dom.getTextContent_(a, b, !1);
  return b.join("");
};
goog.dom.getTextContent_ = function(a, b, c) {
  if (!(a.nodeName in goog.dom.TAGS_TO_IGNORE_)) {
    if (a.nodeType == goog.dom.NodeType.TEXT) {
      c ? b.push(String(a.nodeValue).replace(/(\r\n|\r|\n)/g, "")) : b.push(a.nodeValue);
    } else {
      if (a.nodeName in goog.dom.PREDEFINED_TAG_VALUES_) {
        b.push(goog.dom.PREDEFINED_TAG_VALUES_[a.nodeName]);
      } else {
        for (a = a.firstChild;a;) {
          goog.dom.getTextContent_(a, b, c), a = a.nextSibling;
        }
      }
    }
  }
};
goog.dom.getNodeTextLength = function(a) {
  return goog.dom.getTextContent(a).length;
};
goog.dom.getNodeTextOffset = function(a, b) {
  for (var c = b || goog.dom.getOwnerDocument(a).body, d = [];a && a != c;) {
    for (var e = a;e = e.previousSibling;) {
      d.unshift(goog.dom.getTextContent(e));
    }
    a = a.parentNode;
  }
  return goog.string.trimLeft(d.join("")).replace(/ +/g, " ").length;
};
goog.dom.getNodeAtOffset = function(a, b, c) {
  a = [a];
  for (var d = 0, e = null;0 < a.length && d < b;) {
    if (e = a.pop(), !(e.nodeName in goog.dom.TAGS_TO_IGNORE_)) {
      if (e.nodeType == goog.dom.NodeType.TEXT) {
        var f = e.nodeValue.replace(/(\r\n|\r|\n)/g, "").replace(/ +/g, " "), d = d + f.length
      } else {
        if (e.nodeName in goog.dom.PREDEFINED_TAG_VALUES_) {
          d += goog.dom.PREDEFINED_TAG_VALUES_[e.nodeName].length;
        } else {
          for (f = e.childNodes.length - 1;0 <= f;f--) {
            a.push(e.childNodes[f]);
          }
        }
      }
    }
  }
  goog.isObject(c) && (c.remainder = e ? e.nodeValue.length + b - d - 1 : 0, c.node = e);
  return e;
};
goog.dom.isNodeList = function(a) {
  if (a && "number" == typeof a.length) {
    if (goog.isObject(a)) {
      return "function" == typeof a.item || "string" == typeof a.item;
    }
    if (goog.isFunction(a)) {
      return "function" == typeof a.item;
    }
  }
  return!1;
};
goog.dom.getAncestorByTagNameAndClass = function(a, b, c, d) {
  if (!b && !c) {
    return null;
  }
  var e = b ? b.toUpperCase() : null;
  return goog.dom.getAncestor(a, function(a) {
    return(!e || a.nodeName == e) && (!c || goog.isString(a.className) && goog.array.contains(a.className.split(/\s+/), c));
  }, !0, d);
};
goog.dom.getAncestorByClass = function(a, b, c) {
  return goog.dom.getAncestorByTagNameAndClass(a, null, b, c);
};
goog.dom.getAncestor = function(a, b, c, d) {
  c || (a = a.parentNode);
  c = null == d;
  for (var e = 0;a && (c || e <= d);) {
    if (b(a)) {
      return a;
    }
    a = a.parentNode;
    e++;
  }
  return null;
};
goog.dom.getActiveElement = function(a) {
  try {
    return a && a.activeElement;
  } catch (b) {
  }
  return null;
};
goog.dom.getPixelRatio = function() {
  var a = goog.dom.getWindow(), b = goog.userAgent.GECKO && goog.userAgent.MOBILE;
  return goog.isDef(a.devicePixelRatio) && !b ? a.devicePixelRatio : a.matchMedia ? goog.dom.matchesPixelRatio_(.75) || goog.dom.matchesPixelRatio_(1.5) || goog.dom.matchesPixelRatio_(2) || goog.dom.matchesPixelRatio_(3) || 1 : 1;
};
goog.dom.matchesPixelRatio_ = function(a) {
  return goog.dom.getWindow().matchMedia("(-webkit-min-device-pixel-ratio: " + a + "),(min--moz-device-pixel-ratio: " + a + "),(min-resolution: " + a + "dppx)").matches ? a : 0;
};
goog.dom.DomHelper = function(a) {
  this.document_ = a || goog.global.document || document;
};
goog.dom.DomHelper.prototype.getDomHelper = goog.dom.getDomHelper;
goog.dom.DomHelper.prototype.setDocument = function(a) {
  this.document_ = a;
};
goog.dom.DomHelper.prototype.getDocument = function() {
  return this.document_;
};
goog.dom.DomHelper.prototype.getElement = function(a) {
  return goog.dom.getElementHelper_(this.document_, a);
};
goog.dom.DomHelper.prototype.getRequiredElement = function(a) {
  return goog.dom.getRequiredElementHelper_(this.document_, a);
};
goog.dom.DomHelper.prototype.$ = goog.dom.DomHelper.prototype.getElement;
goog.dom.DomHelper.prototype.getElementsByTagNameAndClass = function(a, b, c) {
  return goog.dom.getElementsByTagNameAndClass_(this.document_, a, b, c);
};
goog.dom.DomHelper.prototype.getElementsByClass = function(a, b) {
  return goog.dom.getElementsByClass(a, b || this.document_);
};
goog.dom.DomHelper.prototype.getElementByClass = function(a, b) {
  return goog.dom.getElementByClass(a, b || this.document_);
};
goog.dom.DomHelper.prototype.getRequiredElementByClass = function(a, b) {
  return goog.dom.getRequiredElementByClass(a, b || this.document_);
};
goog.dom.DomHelper.prototype.$$ = goog.dom.DomHelper.prototype.getElementsByTagNameAndClass;
goog.dom.DomHelper.prototype.setProperties = goog.dom.setProperties;
goog.dom.DomHelper.prototype.getViewportSize = function(a) {
  return goog.dom.getViewportSize(a || this.getWindow());
};
goog.dom.DomHelper.prototype.getDocumentHeight = function() {
  return goog.dom.getDocumentHeight_(this.getWindow());
};
goog.dom.DomHelper.prototype.createDom = function(a, b, c) {
  return goog.dom.createDom_(this.document_, arguments);
};
goog.dom.DomHelper.prototype.$dom = goog.dom.DomHelper.prototype.createDom;
goog.dom.DomHelper.prototype.createElement = function(a) {
  return this.document_.createElement(a);
};
goog.dom.DomHelper.prototype.createTextNode = function(a) {
  return this.document_.createTextNode(String(a));
};
goog.dom.DomHelper.prototype.createTable = function(a, b, c) {
  return goog.dom.createTable_(this.document_, a, b, !!c);
};
goog.dom.DomHelper.prototype.htmlToDocumentFragment = function(a) {
  return goog.dom.htmlToDocumentFragment_(this.document_, a);
};
goog.dom.DomHelper.prototype.isCss1CompatMode = function() {
  return goog.dom.isCss1CompatMode_(this.document_);
};
goog.dom.DomHelper.prototype.getWindow = function() {
  return goog.dom.getWindow_(this.document_);
};
goog.dom.DomHelper.prototype.getDocumentScrollElement = function() {
  return goog.dom.getDocumentScrollElement_(this.document_);
};
goog.dom.DomHelper.prototype.getDocumentScroll = function() {
  return goog.dom.getDocumentScroll_(this.document_);
};
goog.dom.DomHelper.prototype.getActiveElement = function(a) {
  return goog.dom.getActiveElement(a || this.document_);
};
goog.dom.DomHelper.prototype.appendChild = goog.dom.appendChild;
goog.dom.DomHelper.prototype.append = goog.dom.append;
goog.dom.DomHelper.prototype.canHaveChildren = goog.dom.canHaveChildren;
goog.dom.DomHelper.prototype.removeChildren = goog.dom.removeChildren;
goog.dom.DomHelper.prototype.insertSiblingBefore = goog.dom.insertSiblingBefore;
goog.dom.DomHelper.prototype.insertSiblingAfter = goog.dom.insertSiblingAfter;
goog.dom.DomHelper.prototype.insertChildAt = goog.dom.insertChildAt;
goog.dom.DomHelper.prototype.removeNode = goog.dom.removeNode;
goog.dom.DomHelper.prototype.replaceNode = goog.dom.replaceNode;
goog.dom.DomHelper.prototype.flattenElement = goog.dom.flattenElement;
goog.dom.DomHelper.prototype.getChildren = goog.dom.getChildren;
goog.dom.DomHelper.prototype.getFirstElementChild = goog.dom.getFirstElementChild;
goog.dom.DomHelper.prototype.getLastElementChild = goog.dom.getLastElementChild;
goog.dom.DomHelper.prototype.getNextElementSibling = goog.dom.getNextElementSibling;
goog.dom.DomHelper.prototype.getPreviousElementSibling = goog.dom.getPreviousElementSibling;
goog.dom.DomHelper.prototype.getNextNode = goog.dom.getNextNode;
goog.dom.DomHelper.prototype.getPreviousNode = goog.dom.getPreviousNode;
goog.dom.DomHelper.prototype.isNodeLike = goog.dom.isNodeLike;
goog.dom.DomHelper.prototype.isElement = goog.dom.isElement;
goog.dom.DomHelper.prototype.isWindow = goog.dom.isWindow;
goog.dom.DomHelper.prototype.getParentElement = goog.dom.getParentElement;
goog.dom.DomHelper.prototype.contains = goog.dom.contains;
goog.dom.DomHelper.prototype.compareNodeOrder = goog.dom.compareNodeOrder;
goog.dom.DomHelper.prototype.findCommonAncestor = goog.dom.findCommonAncestor;
goog.dom.DomHelper.prototype.getOwnerDocument = goog.dom.getOwnerDocument;
goog.dom.DomHelper.prototype.getFrameContentDocument = goog.dom.getFrameContentDocument;
goog.dom.DomHelper.prototype.getFrameContentWindow = goog.dom.getFrameContentWindow;
goog.dom.DomHelper.prototype.setTextContent = goog.dom.setTextContent;
goog.dom.DomHelper.prototype.getOuterHtml = goog.dom.getOuterHtml;
goog.dom.DomHelper.prototype.findNode = goog.dom.findNode;
goog.dom.DomHelper.prototype.findNodes = goog.dom.findNodes;
goog.dom.DomHelper.prototype.isFocusableTabIndex = goog.dom.isFocusableTabIndex;
goog.dom.DomHelper.prototype.setFocusableTabIndex = goog.dom.setFocusableTabIndex;
goog.dom.DomHelper.prototype.isFocusable = goog.dom.isFocusable;
goog.dom.DomHelper.prototype.getTextContent = goog.dom.getTextContent;
goog.dom.DomHelper.prototype.getNodeTextLength = goog.dom.getNodeTextLength;
goog.dom.DomHelper.prototype.getNodeTextOffset = goog.dom.getNodeTextOffset;
goog.dom.DomHelper.prototype.getNodeAtOffset = goog.dom.getNodeAtOffset;
goog.dom.DomHelper.prototype.isNodeList = goog.dom.isNodeList;
goog.dom.DomHelper.prototype.getAncestorByTagNameAndClass = goog.dom.getAncestorByTagNameAndClass;
goog.dom.DomHelper.prototype.getAncestorByClass = goog.dom.getAncestorByClass;
goog.dom.DomHelper.prototype.getAncestor = goog.dom.getAncestor;
goog.uri = {};
goog.uri.utils = {};
goog.uri.utils.CharCode_ = {AMPERSAND:38, EQUAL:61, HASH:35, QUESTION:63};
goog.uri.utils.buildFromEncodedParts = function(a, b, c, d, e, f, g) {
  var h = "";
  a && (h += a + ":");
  c && (h += "//", b && (h += b + "@"), h += c, d && (h += ":" + d));
  e && (h += e);
  f && (h += "?" + f);
  g && (h += "#" + g);
  return h;
};
goog.uri.utils.splitRe_ = /^(?:([^:/?#.]+):)?(?:\/\/(?:([^/?#]*)@)?([^/#?]*?)(?::([0-9]+))?(?=[/#?]|$))?([^?#]+)?(?:\?([^#]*))?(?:#(.*))?$/;
goog.uri.utils.ComponentIndex = {SCHEME:1, USER_INFO:2, DOMAIN:3, PORT:4, PATH:5, QUERY_DATA:6, FRAGMENT:7};
goog.uri.utils.split = function(a) {
  goog.uri.utils.phishingProtection_();
  return a.match(goog.uri.utils.splitRe_);
};
goog.uri.utils.needsPhishingProtection_ = goog.userAgent.WEBKIT;
goog.uri.utils.phishingProtection_ = function() {
  if (goog.uri.utils.needsPhishingProtection_) {
    goog.uri.utils.needsPhishingProtection_ = !1;
    var a = goog.global.location;
    if (a) {
      var b = a.href;
      if (b && (b = goog.uri.utils.getDomain(b)) && b != a.hostname) {
        throw goog.uri.utils.needsPhishingProtection_ = !0, Error();
      }
    }
  }
};
goog.uri.utils.decodeIfPossible_ = function(a, b) {
  return a ? b ? decodeURI(a) : decodeURIComponent(a) : a;
};
goog.uri.utils.getComponentByIndex_ = function(a, b) {
  return goog.uri.utils.split(b)[a] || null;
};
goog.uri.utils.getScheme = function(a) {
  return goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.SCHEME, a);
};
goog.uri.utils.getEffectiveScheme = function(a) {
  a = goog.uri.utils.getScheme(a);
  !a && self.location && (a = self.location.protocol, a = a.substr(0, a.length - 1));
  return a ? a.toLowerCase() : "";
};
goog.uri.utils.getUserInfoEncoded = function(a) {
  return goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.USER_INFO, a);
};
goog.uri.utils.getUserInfo = function(a) {
  return goog.uri.utils.decodeIfPossible_(goog.uri.utils.getUserInfoEncoded(a));
};
goog.uri.utils.getDomainEncoded = function(a) {
  return goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.DOMAIN, a);
};
goog.uri.utils.getDomain = function(a) {
  return goog.uri.utils.decodeIfPossible_(goog.uri.utils.getDomainEncoded(a), !0);
};
goog.uri.utils.getPort = function(a) {
  return Number(goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.PORT, a)) || null;
};
goog.uri.utils.getPathEncoded = function(a) {
  return goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.PATH, a);
};
goog.uri.utils.getPath = function(a) {
  return goog.uri.utils.decodeIfPossible_(goog.uri.utils.getPathEncoded(a), !0);
};
goog.uri.utils.getQueryData = function(a) {
  return goog.uri.utils.getComponentByIndex_(goog.uri.utils.ComponentIndex.QUERY_DATA, a);
};
goog.uri.utils.getFragmentEncoded = function(a) {
  var b = a.indexOf("#");
  return 0 > b ? null : a.substr(b + 1);
};
goog.uri.utils.setFragmentEncoded = function(a, b) {
  return goog.uri.utils.removeFragment(a) + (b ? "#" + b : "");
};
goog.uri.utils.getFragment = function(a) {
  return goog.uri.utils.decodeIfPossible_(goog.uri.utils.getFragmentEncoded(a));
};
goog.uri.utils.getHost = function(a) {
  a = goog.uri.utils.split(a);
  return goog.uri.utils.buildFromEncodedParts(a[goog.uri.utils.ComponentIndex.SCHEME], a[goog.uri.utils.ComponentIndex.USER_INFO], a[goog.uri.utils.ComponentIndex.DOMAIN], a[goog.uri.utils.ComponentIndex.PORT]);
};
goog.uri.utils.getPathAndAfter = function(a) {
  a = goog.uri.utils.split(a);
  return goog.uri.utils.buildFromEncodedParts(null, null, null, null, a[goog.uri.utils.ComponentIndex.PATH], a[goog.uri.utils.ComponentIndex.QUERY_DATA], a[goog.uri.utils.ComponentIndex.FRAGMENT]);
};
goog.uri.utils.removeFragment = function(a) {
  var b = a.indexOf("#");
  return 0 > b ? a : a.substr(0, b);
};
goog.uri.utils.haveSameDomain = function(a, b) {
  var c = goog.uri.utils.split(a), d = goog.uri.utils.split(b);
  return c[goog.uri.utils.ComponentIndex.DOMAIN] == d[goog.uri.utils.ComponentIndex.DOMAIN] && c[goog.uri.utils.ComponentIndex.SCHEME] == d[goog.uri.utils.ComponentIndex.SCHEME] && c[goog.uri.utils.ComponentIndex.PORT] == d[goog.uri.utils.ComponentIndex.PORT];
};
goog.uri.utils.assertNoFragmentsOrQueries_ = function(a) {
  if (goog.DEBUG && (0 <= a.indexOf("#") || 0 <= a.indexOf("?"))) {
    throw Error("goog.uri.utils: Fragment or query identifiers are not supported: [" + a + "]");
  }
};
goog.uri.utils.appendQueryData_ = function(a) {
  if (a[1]) {
    var b = a[0], c = b.indexOf("#");
    0 <= c && (a.push(b.substr(c)), a[0] = b = b.substr(0, c));
    c = b.indexOf("?");
    0 > c ? a[1] = "?" : c == b.length - 1 && (a[1] = void 0);
  }
  return a.join("");
};
goog.uri.utils.appendKeyValuePairs_ = function(a, b, c) {
  if (goog.isArray(b)) {
    goog.asserts.assertArray(b);
    for (var d = 0;d < b.length;d++) {
      goog.uri.utils.appendKeyValuePairs_(a, String(b[d]), c);
    }
  } else {
    null != b && c.push("&", a, "" === b ? "" : "=", goog.string.urlEncode(b));
  }
};
goog.uri.utils.buildQueryDataBuffer_ = function(a, b, c) {
  goog.asserts.assert(0 == Math.max(b.length - (c || 0), 0) % 2, "goog.uri.utils: Key/value lists must be even in length.");
  for (c = c || 0;c < b.length;c += 2) {
    goog.uri.utils.appendKeyValuePairs_(b[c], b[c + 1], a);
  }
  return a;
};
goog.uri.utils.buildQueryData = function(a, b) {
  var c = goog.uri.utils.buildQueryDataBuffer_([], a, b);
  c[0] = "";
  return c.join("");
};
goog.uri.utils.buildQueryDataBufferFromMap_ = function(a, b) {
  for (var c in b) {
    goog.uri.utils.appendKeyValuePairs_(c, b[c], a);
  }
  return a;
};
goog.uri.utils.buildQueryDataFromMap = function(a) {
  a = goog.uri.utils.buildQueryDataBufferFromMap_([], a);
  a[0] = "";
  return a.join("");
};
goog.uri.utils.appendParams = function(a, b) {
  return goog.uri.utils.appendQueryData_(2 == arguments.length ? goog.uri.utils.buildQueryDataBuffer_([a], arguments[1], 0) : goog.uri.utils.buildQueryDataBuffer_([a], arguments, 1));
};
goog.uri.utils.appendParamsFromMap = function(a, b) {
  return goog.uri.utils.appendQueryData_(goog.uri.utils.buildQueryDataBufferFromMap_([a], b));
};
goog.uri.utils.appendParam = function(a, b, c) {
  a = [a, "&", b];
  goog.isDefAndNotNull(c) && a.push("=", goog.string.urlEncode(c));
  return goog.uri.utils.appendQueryData_(a);
};
goog.uri.utils.findParam_ = function(a, b, c, d) {
  for (var e = c.length;0 <= (b = a.indexOf(c, b)) && b < d;) {
    var f = a.charCodeAt(b - 1);
    if (f == goog.uri.utils.CharCode_.AMPERSAND || f == goog.uri.utils.CharCode_.QUESTION) {
      if (f = a.charCodeAt(b + e), !f || f == goog.uri.utils.CharCode_.EQUAL || f == goog.uri.utils.CharCode_.AMPERSAND || f == goog.uri.utils.CharCode_.HASH) {
        return b;
      }
    }
    b += e + 1;
  }
  return-1;
};
goog.uri.utils.hashOrEndRe_ = /#|$/;
goog.uri.utils.hasParam = function(a, b) {
  return 0 <= goog.uri.utils.findParam_(a, 0, b, a.search(goog.uri.utils.hashOrEndRe_));
};
goog.uri.utils.getParamValue = function(a, b) {
  var c = a.search(goog.uri.utils.hashOrEndRe_), d = goog.uri.utils.findParam_(a, 0, b, c);
  if (0 > d) {
    return null;
  }
  var e = a.indexOf("&", d);
  if (0 > e || e > c) {
    e = c;
  }
  d += b.length + 1;
  return goog.string.urlDecode(a.substr(d, e - d));
};
goog.uri.utils.getParamValues = function(a, b) {
  for (var c = a.search(goog.uri.utils.hashOrEndRe_), d = 0, e, f = [];0 <= (e = goog.uri.utils.findParam_(a, d, b, c));) {
    d = a.indexOf("&", e);
    if (0 > d || d > c) {
      d = c;
    }
    e += b.length + 1;
    f.push(goog.string.urlDecode(a.substr(e, d - e)));
  }
  return f;
};
goog.uri.utils.trailingQueryPunctuationRe_ = /[?&]($|#)/;
goog.uri.utils.removeParam = function(a, b) {
  for (var c = a.search(goog.uri.utils.hashOrEndRe_), d = 0, e, f = [];0 <= (e = goog.uri.utils.findParam_(a, d, b, c));) {
    f.push(a.substring(d, e)), d = Math.min(a.indexOf("&", e) + 1 || c, c);
  }
  f.push(a.substr(d));
  return f.join("").replace(goog.uri.utils.trailingQueryPunctuationRe_, "$1");
};
goog.uri.utils.setParam = function(a, b, c) {
  return goog.uri.utils.appendParam(goog.uri.utils.removeParam(a, b), b, c);
};
goog.uri.utils.appendPath = function(a, b) {
  goog.uri.utils.assertNoFragmentsOrQueries_(a);
  goog.string.endsWith(a, "/") && (a = a.substr(0, a.length - 1));
  goog.string.startsWith(b, "/") && (b = b.substr(1));
  return goog.string.buildString(a, "/", b);
};
goog.uri.utils.setPath = function(a, b) {
  goog.string.startsWith(b, "/") || (b = "/" + b);
  var c = goog.uri.utils.split(a);
  return goog.uri.utils.buildFromEncodedParts(c[goog.uri.utils.ComponentIndex.SCHEME], c[goog.uri.utils.ComponentIndex.USER_INFO], c[goog.uri.utils.ComponentIndex.DOMAIN], c[goog.uri.utils.ComponentIndex.PORT], b, c[goog.uri.utils.ComponentIndex.QUERY_DATA], c[goog.uri.utils.ComponentIndex.FRAGMENT]);
};
goog.uri.utils.StandardQueryParam = {RANDOM:"zx"};
goog.uri.utils.makeUnique = function(a) {
  return goog.uri.utils.setParam(a, goog.uri.utils.StandardQueryParam.RANDOM, goog.string.getRandomString());
};
var pagespeed = {MobUtil:{}};
pagespeed.MobUtil.ASCII_0_ = 48;
pagespeed.MobUtil.ASCII_9_ = 57;
pagespeed.MobUtil.Rect = function() {
  this.height = this.width = this.bottom = this.right = this.left = this.top = 0;
};
pagespeed.MobUtil.Dimensions = function(a, b) {
  this.width = a;
  this.height = b;
};
pagespeed.MobUtil.isDigit = function(a, b) {
  if (a.length <= b) {
    return!1;
  }
  var c = a.charCodeAt(b);
  return c >= pagespeed.MobUtil.ASCII_0_ && c <= pagespeed.MobUtil.ASCII_9_;
};
pagespeed.MobUtil.pixelValue = function(a) {
  var b = null;
  if (a && "string" == typeof a) {
    var c = a.indexOf("px");
    -1 != c && (a = a.substring(0, c));
    pagespeed.MobUtil.isDigit(a, a.length - 1) && (b = parseInt(a, 10), isNaN(b) && (b = null));
  }
  return b;
};
pagespeed.MobUtil.computedDimension = function(a, b) {
  var c = null;
  a && (c = pagespeed.MobUtil.pixelValue(a.getPropertyValue(b)));
  return c;
};
pagespeed.MobUtil.removeProperty = function(a, b) {
  a.style && a.style.removeProperty(b);
  a.removeAttribute(b);
};
pagespeed.MobUtil.findRequestedDimension = function(a, b) {
  var c = null;
  a.style && (c = pagespeed.MobUtil.pixelValue(a.style.getPropertyValue(b)));
  null == c && (c = pagespeed.MobUtil.pixelValue(a.getAttribute(b)));
  return c;
};
pagespeed.MobUtil.setPropertyImportant = function(a, b, c) {
  a.style.setProperty(b, c, "important");
};
pagespeed.MobUtil.aboutEqual = function(a, b) {
  return.95 < (a > b ? b / a : a / b);
};
pagespeed.MobUtil.addStyles = function(a, b) {
  if (b && 0 != b.length) {
    var c = a.getAttribute("style") || "";
    0 < c.length && ";" != c[c.length - 1] && (c += ";");
    a.setAttribute("style", c + b);
  }
};
pagespeed.MobUtil.boundingRect = function(a) {
  a = a.getBoundingClientRect();
  var b = document.body, c = document.documentElement || b.parentNode || b, b = "pageXOffset" in window ? window.pageXOffset : c.scrollLeft, c = "pageYOffset" in window ? window.pageYOffset : c.scrollTop;
  return new goog.math.Box(a.top + c, a.right + b, a.bottom + c, a.left + b);
};
pagespeed.MobUtil.isSinglePixel = function(a) {
  return 1 == a.naturalHeight && 1 == a.naturalWidth;
};
pagespeed.MobUtil.findBackgroundImage = function(a) {
  var b = null;
  return "SCRIPT" != a.tagName && "STYLE" != a.tagName && a.style && (a = window.getComputedStyle(a)) && (b = a.getPropertyValue("background-image"), "none" == b && (b = null), b && 5 < b.length && 0 == b.indexOf("url(") && ")" == b[b.length - 1]) ? b = b.substring(4, b.length - 1) : null;
};
pagespeed.MobUtil.inFriendlyIframe = function() {
  if (null != window.parent && window != window.parent) {
    try {
      if (window.parent.document.domain == document.domain) {
        return!0;
      }
    } catch (a) {
    }
  }
  return!1;
};
pagespeed.MobUtil.possiblyInQuirksMode = function() {
  return "CSS1Compat" !== document.compatMode;
};
pagespeed.MobUtil.hasIntersectingRects = function(a) {
  for (var b = 0;b < a.length;++b) {
    for (var c = b + 1;c < a.length;++c) {
      if (goog.math.Box.intersects(a[b], a[c])) {
        return!0;
      }
    }
  }
  return!1;
};
pagespeed.MobUtil.createXPathFromNode = function(a) {
  for (var b = document.getElementsByTagName("*"), c, d = [], e;goog.dom.isElement(a);a = a.parentNode) {
    if (a.hasAttribute("id")) {
      for (e = c = 0;e < b.length && 1 >= c;++e) {
        b[e].hasAttribute("id") && b[e].id == a.id && ++c;
      }
      if (1 == c) {
        return d.unshift('id("' + a.getAttribute("id") + '")'), d.join("/");
      }
      d.unshift(a.localName.toLowerCase() + '[@id="' + a.getAttribute("id") + '"]');
    } else {
      if (a.hasAttribute("class")) {
        d.unshift(a.localName.toLowerCase() + '[@class="' + a.getAttribute("class") + '"]');
      } else {
        c = 1;
        for (e = a.previousSibling;e;e = e.previousSibling) {
          e.localName == a.localName && c++;
        }
        d.unshift(a.localName.toLowerCase() + "[" + c + "]");
      }
    }
  }
  return d.length ? "/" + d.join("/") : null;
};
pagespeed.MobUtil.countNodes = function(a) {
  var b = 1;
  for (a = a.firstChild;a;a = a.nextSibling) {
    b += pagespeed.MobUtil.countNodes(a);
  }
  return b;
};
pagespeed.MobUtil.castElement = function(a) {
  return goog.dom.isElement(a) ? a : null;
};
pagespeed.MobUtil.ImageSource = {IMG:"IMG", SVG:"SVG", BACKGROUND:"background-image"};
goog.exportSymbol("pagespeed.MobUtil.ImageSource", pagespeed.MobUtil.ImageSource);
pagespeed.MobUtil.ThemeData = function(a, b, c, d) {
  this.menuFrontColor = a;
  this.menuBackColor = b;
  this.menuButton = c;
  this.logoSpan = d;
};
pagespeed.MobUtil.textBetweenBrackets = function(a) {
  var b = a.indexOf("("), c = a.lastIndexOf(")");
  return 0 <= b && c > b ? a.substring(b + 1, c) : null;
};
pagespeed.MobUtil.colorStringToNumbers = function(a) {
  var b = pagespeed.MobUtil.textBetweenBrackets(a);
  if (!b) {
    return "transparent" == a ? [0, 0, 0, 0] : null;
  }
  b = b.split(",");
  a = [];
  for (var c = 0, d = b.length;c < d;++c) {
    if (a[c] = 3 != c ? parseInt(b[c], 10) : parseFloat(b[c]), isNaN(a[c])) {
      return null;
    }
  }
  return 3 == a.length || 4 == a.length ? a : null;
};
pagespeed.MobUtil.colorNumbersToString = function(a) {
  for (var b = 0, c = a.length;b < c;++b) {
    var d = Math.round(a[b]);
    0 > d ? d = 0 : 255 < d && (d = 255);
    a[b] = d;
  }
  return goog.color.rgbArrayToHex(a);
};
pagespeed.MobUtil.stripNonAlphaNumeric = function(a) {
  a = a.toLowerCase();
  for (var b = "", c = 0, d = a.length;c < d;++c) {
    var e = a.charAt(c);
    if ("a" <= e && "z" >= e || "0" <= e && "9" >= e) {
      b += e;
    }
  }
  return b;
};
pagespeed.MobUtil.findPattern = function(a, b) {
  return 0 <= pagespeed.MobUtil.stripNonAlphaNumeric(a).indexOf(pagespeed.MobUtil.stripNonAlphaNumeric(b)) ? 1 : 0;
};
pagespeed.MobUtil.removeSuffixNTimes = function(a, b, c) {
  for (var d = a.length, e = 0;e < c;++e) {
    var f = a.lastIndexOf(b, d - 1);
    if (0 <= f) {
      d = f;
    } else {
      break;
    }
  }
  return 0 <= f ? a.substring(0, d) : a;
};
pagespeed.MobUtil.getSiteOrganization = function() {
  var a = document.domain.toLowerCase().split("."), b = a.length;
  return 4 < b && 2 == a[b - 3].length ? a[b - 5] : 3 < b ? a[b - 4] : null;
};
pagespeed.MobUtil.resourceFileName = function(a) {
  if (!a || 0 <= a.indexOf("data:image/")) {
    return "";
  }
  var b = a.lastIndexOf("/");
  0 > b ? b = 0 : ++b;
  var c = a.indexOf(".", b);
  0 > c && (c = a.length);
  return a.substring(b, c);
};
pagespeed.MobUtil.proxyImageUrl = function(a, b) {
  var c = b || document.location.origin, d = goog.uri.utils.getDomain(c), e = goog.uri.utils.getDomain(a);
  if (null == d || null == e) {
    return a;
  }
  var f = !1;
  if (d == e) {
    f = !0;
  } else {
    if (0 <= d.indexOf(e)) {
      var d = d.split("."), g = d.length;
      3 <= g && (e == d.slice(0, g - 2).join(".") || "www" == d[0] && (e == d.slice(1, g - 2).join(".") || e == d.slice(1, g).join("."))) && (f = !0);
    }
  }
  return f ? (e = a.indexOf(e) + e.length, c + a.substring(e)) : a;
};
pagespeed.MobUtil.extractImage = function(a, b) {
  var c = null;
  switch(b) {
    case pagespeed.MobUtil.ImageSource.IMG:
      a.tagName == b && (c = a.src);
      break;
    case pagespeed.MobUtil.ImageSource.SVG:
      if (a.tagName == b) {
        var d = (new XMLSerializer).serializeToString(a), c = self.URL || self.webkitURL || self, d = new Blob([d], {type:"image/svg+xml;charset=utf-8"}), c = c.createObjectURL(d)
      }
      break;
    case pagespeed.MobUtil.ImageSource.BACKGROUND:
      c = pagespeed.MobUtil.findBackgroundImage(a);
  }
  return c ? pagespeed.MobUtil.proxyImageUrl(c) : null;
};
pagespeed.MobUtil.isCrossOrigin = function(a) {
  return!goog.string.startsWith(a, document.location.origin + "/") && !goog.string.startsWith(a, "data:image/");
};
pagespeed.MobUtil.boundingRectAndSize = function(a) {
  a = pagespeed.MobUtil.boundingRect(a);
  var b = new pagespeed.MobUtil.Rect;
  b.top = a.top;
  b.bottom = a.bottom;
  b.left = a.left;
  b.right = a.right;
  b.height = a.bottom - a.top;
  b.width = a.right - a.left;
  return b;
};
pagespeed.MobUtil.isOffScreen = function(a) {
  var b = pagespeed.MobUtil.pixelValue(a.top);
  a = pagespeed.MobUtil.pixelValue(a.left);
  return null != b && -100 > b || null != a && -100 > a;
};
pagespeed.MobUtil.toCssString1 = function(a) {
  a = a.replace(/\\/g, "\\\\");
  a = a.replace(/"/g, '\\"');
  a = a.replace(/\n/g, "\\a ");
  a = a.replace(/\f/g, "\\c ");
  a = a.replace(/\r/g, "\\d ");
  return'"' + a + '"';
};
pagespeed.MobColor = function() {
  this.numPendingImages_ = 0;
  this.foregroundData_ = this.logo_ = null;
};
pagespeed.MobColor.prototype.EPSILON_ = 1E-10;
pagespeed.MobColor.prototype.MIN_CONTRAST_ = 3;
pagespeed.MobColor.ThemeColors = function(a, b) {
  this.background = a;
  this.foreground = b;
};
pagespeed.MobColor.prototype.distance_ = function(a, b) {
  if (3 != a.length || 3 != b.length) {
    return Infinity;
  }
  var c = a[0] - b[0], d = a[1] - b[1], e = a[2] - b[2];
  return Math.sqrt(c * c + d * d + e * e);
};
pagespeed.MobColor.prototype.srgbToRgb_ = function(a) {
  a /= 255;
  return a = .03928 >= a ? a / 12.92 : Math.pow((a + .055) / 1.055, 2.4);
};
pagespeed.MobColor.prototype.rgbToGray_ = function(a) {
  return.2126 * this.srgbToRgb_(a[0]) + .7152 * this.srgbToRgb_(a[1]) + .0722 * this.srgbToRgb_(a[2]);
};
pagespeed.MobColor.prototype.enhanceColors_ = function(a) {
  var b = a.background, c = a.foreground, d = this.rgbToGray_(b), e = this.rgbToGray_(c);
  if (d < this.EPSILON_ && e < this.EPSILON_) {
    return a;
  }
  d = e / d;
  1 > d && (d = 1 / d);
  if (d > this.MIN_CONTRAST_) {
    return a;
  }
  b = goog.color.rgbArrayToHsv(b);
  c = goog.color.rgbArrayToHsv(c);
  d = a = null;
  b[2] < c[2] ? (a = b[2], d = c[2]) : (a = c[2], d = b[2]);
  e = (this.MIN_CONTRAST_ * a - d) / (this.MIN_CONTRAST_ + 1);
  a = a > e ? a - e : 0;
  d = d < 1 - 2 * e ? d + 2 * e : 255;
  b[2] < c[2] ? (b[2] = a, c[2] = d) : (c[2] = a, b[2] = d);
  b = goog.color.hsvArrayToRgb(b);
  c = goog.color.hsvArrayToRgb(c);
  return new pagespeed.MobColor.ThemeColors(b, c);
};
pagespeed.MobColor.prototype.computeColors_ = function(a, b, c, d) {
  var e = [], f, g, h;
  for (g = 0;g < d;++g) {
    for (f = 0;f < c;++f) {
      var k = 4 * (g * c + f);
      h = 3 * (g * c + f);
      var l = a[k + 3] / 255, m = 1 - l;
      e[h] = l * a[k] + m * b[0];
      e[h + 1] = l * a[k + 1] + m * b[1];
      e[h + 2] = l * a[k + 2] + m * b[2];
    }
  }
  k = [0, 0, 0];
  for (f = l = g = 0;f < c;++f) {
    m = (d - 1) * c + f, h = 3 * m, k[0] += e[h], k[1] += e[h + 1], k[2] += e[h + 2], l += a[4 * m + 3], ++g;
  }
  if (l > 127.5 * g) {
    for (f = 0;3 > f;++f) {
      k[f] = Math.floor(k[f] / g);
    }
  } else {
    k = b;
  }
  a = Math.floor(.25 * c);
  b = Math.floor(.75 * c);
  l = Math.floor(.25 * d);
  d = Math.floor(.75 * d);
  var m = 0, n = [];
  for (g = l;g <= d;++g) {
    for (f = a;f <= b;++f) {
      h = 3 * (g * c + f), n[m] = this.distance_(e.slice(h, h + 3), k), ++m;
    }
  }
  f = n.sort(function(a, b) {
    return a - b;
  });
  var p = Math.max(1, f[Math.floor(.75 * m)]), m = 0, n = [0, 0, 0];
  for (g = l;g <= d;++g) {
    for (f = a;f <= b;++f) {
      h = 3 * (g * c + f), this.distance_(e.slice(h, h + 3), k) >= p && (n[0] += e[h], n[1] += e[h + 1], n[2] += e[h + 2], ++m);
    }
  }
  if (0 < m) {
    for (f = 0;3 > f;++f) {
      n[f] = Math.floor(n[f] / m);
    }
  }
  return this.enhanceColors_(new pagespeed.MobColor.ThemeColors(k, n));
};
pagespeed.MobColor.prototype.synthesizeCallback_ = function() {
  --this.numPendingImages_;
  if (!(0 < this.numPendingImages_)) {
    var a = [255, 255, 255], b = [0, 0, 0], c = this.logo_;
    this.foregroundData_ && this.foregroundData_.data && c && c.foregroundRect && c.backgroundColor ? (b = this.computeColors_(this.foregroundData_.data, c.backgroundColor, c.foregroundRect.width, c.foregroundRect.height), a = b.background, b = b.foreground) : c && c.backgroundColor && (a = c.backgroundColor, b = 178.5 < goog.color.rgbArrayToHsv(a)[2] ? [0, 0, 0] : [255, 255, 255]);
    console.log("Theme color. Background: " + a + " foreground: " + b);
    this.doneCallback_(this.logo_, a, b);
  }
};
pagespeed.MobColor.prototype.getImageDataAndSynthesize_ = function(a, b, c) {
  var d = new Image;
  d.onload = goog.bind(function() {
    var b = document.createElement("canvas"), f = null, g = null;
    c && 0 < c.width && 0 < c.height ? (f = c.width, g = c.height) : (f = d.naturalWidth, g = d.naturalHeight);
    b.width = f;
    b.height = g;
    b = b.getContext("2d");
    b.drawImage(d, 0, 0);
    "foreground" == a && (this.foregroundData_ = b.getImageData(0, 0, f, g));
    this.synthesizeCallback_();
  }, this);
  d.onerror = goog.bind(this.synthesizeCallback_, this);
  d.src = b;
};
pagespeed.MobColor.prototype.run = function(a, b) {
  this.logo_ = a;
  this.doneCallback_ && alert("A callback which was supposed to run after extracting theme color  was not executed.");
  this.doneCallback_ = b;
  a && a.foregroundImage && !pagespeed.MobUtil.isCrossOrigin(a.foregroundImage) ? (this.numPendingImages_ = 1, this.getImageDataAndSynthesize_("foreground", a.foregroundImage, a.foregroundRect), console.log("Found logo. Theme color will be computed from logo.")) : (a && a.foregroundImage ? console.log("Found logo but its origin is different that of HTML. Use default color.") : console.log("Could not find logo. Use default color."), this.synthesizeCallback_());
};
pagespeed.MobLayout = function(a) {
  this.psMob_ = a;
  this.dontTouchIds_ = {};
  this.maxWidth_ = this.computeMaxWidth_();
  console.log("window.pagespeed.MobLayout.maxWidth=" + this.maxWidth_);
};
pagespeed.MobLayout.CLAMPED_STYLES_ = "padding-left padding-bottom padding-right padding-top margin-left margin-bottom margin-right margin-top border-left-width border-bottom-width border-right-width border-top-width left top".split(" ");
pagespeed.MobLayout.FLEXIBLE_WIDTH_TAGS_ = {A:!0, DIV:!0, FORM:!0, H1:!0, H2:!0, H3:!0, H4:!0, P:!0, SPAN:!0, TBODY:!0, TD:!0, TFOOT:!0, TH:!0, THEAD:!0, TR:!0};
pagespeed.MobLayout.NO_PERCENT_ = ["left", "width"];
pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_ = "data-pagespeed-negative-bottom-margin";
pagespeed.MobLayout.prototype.addDontTouchId = function(a) {
  this.dontTouchIds_[a] = !0;
};
pagespeed.MobLayout.prototype.computeMaxWidth_ = function() {
  var a = document.documentElement.clientWidth;
  if (a) {
    for (var b = window.getComputedStyle(document.body), c = ["padding-left", "padding-right"], d = 0;d < c.length;++d) {
      var e = pagespeed.MobUtil.computedDimension(b, c[d]);
      e && (a -= e);
    }
  } else {
    a = 400;
  }
  return a;
};
pagespeed.MobLayout.prototype.dontTouch_ = function(a) {
  if (!a) {
    return!0;
  }
  var b = a.tagName.toUpperCase();
  return "SCRIPT" == b || "STYLE" == b || "IFRAME" == b || a.id && this.dontTouchIds_[a.id] || a.classList.contains("psmob-nav-panel") || a.classList.contains("psmob-header-bar") || a.classList.contains("psmob-header-spacer-div") || a.classList.contains("psmob-logo-span");
};
pagespeed.MobLayout.prototype.getMobilizeElement = function(a) {
  a = pagespeed.MobUtil.castElement(a);
  return this.dontTouch_(a) ? null : a;
};
pagespeed.MobLayout.prototype.childElements_ = function(a) {
  var b = [];
  for (a = a.firstChild;a;a = a.nextSibling) {
    null != pagespeed.MobUtil.castElement(a) && b.push(a);
  }
  return b;
};
pagespeed.MobLayout.prototype.forEachMobilizableChild_ = function(a, b) {
  for (var c = a.firstChild;c;c = c.nextSibling) {
    null != this.getMobilizeElement(c) && b.call(this, c);
  }
};
pagespeed.MobLayout.numberOfPasses = function() {
  return pagespeed.MobLayout.sequence_.length / 2;
};
pagespeed.MobLayout.prototype.isProbablyASprite_ = function(a) {
  if ("auto" == a.getPropertyValue("background-size")) {
    return!1;
  }
  a = a.getPropertyValue("background-position");
  if ("none" == a) {
    return!1;
  }
  a = a.split(" ");
  return 2 == a.length && null != pagespeed.MobUtil.pixelValue(a[0]) && null != pagespeed.MobUtil.pixelValue(a[1]) ? !0 : !1;
};
pagespeed.MobLayout.prototype.shrinkWideElements_ = function(a) {
  var b = window.getComputedStyle(a), c = pagespeed.MobUtil.findBackgroundImage(a);
  if (c && (c = this.psMob_.findImageSize(c)) && c.width && c.height && !pagespeed.MobLayout.prototype.isProbablyASprite_(b)) {
    var d = c.width, c = c.height;
    if (d > this.maxWidth_) {
      var c = Math.round(this.maxWidth_ / d * c), d = "background-size:" + this.maxWidth_ + "px " + c + "px;background-repeat:no-repeat;", e = pagespeed.MobUtil.computedDimension(b, "height");
      c == e && (d += "height:" + c + "px;");
      pagespeed.MobUtil.addStyles(a, d);
    }
    pagespeed.MobUtil.setPropertyImportant(a, "min-height", "" + c + "px");
  }
  if ("PRE" == a.tagName.toUpperCase() || "pre" == b.getPropertyValue("white-space") && a.offsetWidth > this.maxWidth_) {
    a.style.overflowX = "scroll";
  }
  this.forEachMobilizableChild_(a, this.shrinkWideElements_);
};
pagespeed.MobLayout.prototype.computeAllSizingAndResynthesize = function() {
  if (null != document.body) {
    for (var a = 0;a < pagespeed.MobLayout.sequence_.length;++a) {
      pagespeed.MobLayout.sequence_[a].call(this, document.body), ++a, this.psMob_.layoutPassDone(pagespeed.MobLayout.sequence_[a]);
    }
  }
};
pagespeed.MobLayout.prototype.makeHorizontallyScrollable_ = function(a) {
  pagespeed.MobUtil.setPropertyImportant(a, "overflow-x", "auto");
  pagespeed.MobUtil.setPropertyImportant(a, "width", "auto");
  pagespeed.MobUtil.setPropertyImportant(a, "display", "block");
};
pagespeed.MobLayout.prototype.resizeVertically_ = function(a) {
  this.resizeVerticallyAndReturnBottom_(a, 0);
};
pagespeed.MobLayout.prototype.resizeVerticallyAndReturnBottom_ = function(a, b) {
  var c, d;
  if (d = pagespeed.MobUtil.boundingRect(a)) {
    c = d.top, d = d.bottom;
  } else {
    c = b;
    if (a.offsetParent == a.parentNode) {
      c += a.offsetTop;
    } else {
      if (a.offsetParent != a.parentNode.parentNode) {
        return null;
      }
    }
    d = c + a.offsetHeight - 1;
  }
  if (this.dontTouch_(a)) {
    return d;
  }
  d = c - 1;
  var e = window.getComputedStyle(a);
  if (!e) {
    return null;
  }
  var f = pagespeed.MobUtil.computedDimension(e, "min-height");
  null != f && (d += f);
  for (var f = c + a.offsetHeight - 1, g = !1, h = !1, k, l = a.firstChild;l;l = l.nextSibling) {
    if (k = pagespeed.MobUtil.castElement(l)) {
      var m = window.getComputedStyle(k);
      m && ("absolute" != m.position || pagespeed.MobUtil.isOffScreen(m) || "0px" == m.getPropertyValue("height") || "hidden" == m.getPropertyValue("visibility") || (h = !0));
      k = this.resizeVerticallyAndReturnBottom_(k, c);
      null != k && (g = !0, d = Math.max(d, k));
    }
  }
  if ("fixed" == e.getPropertyValue("position") && g) {
    return null;
  }
  e = a.tagName.toUpperCase();
  "BODY" != e && (l = f - c + 1, g ? d != f && (h ? pagespeed.MobUtil.setPropertyImportant(a, "height", "" + (d - c + 1) + "px") : pagespeed.MobUtil.setPropertyImportant(a, "height", "auto")) : ("IMG" != e && 0 < l && "" == a.style.backgroundSize && (pagespeed.MobUtil.removeProperty(a, "height"), pagespeed.MobUtil.setPropertyImportant(a, "height", "auto"), a.offsetHeight && (f = c + a.offsetHeight)), d = f));
  return d;
};
pagespeed.MobLayout.prototype.resizeIfTooWide_ = function(a) {
  for (var b = this.childElements_(a), c = 0;c < b.length;++c) {
    this.resizeIfTooWide_(b[c]);
  }
  if (!(a.offsetWidth <= this.maxWidth_)) {
    if (b = a.tagName.toUpperCase(), "TABLE" == b) {
      this.isDataTable_(a) ? this.makeHorizontallyScrollable_(a) : pagespeed.MobUtil.possiblyInQuirksMode() ? this.reorganizeTableQuirksMode_(a, this.maxWidth_) : this.reorganizeTableNoQuirksMode_(a, this.maxWidth_);
    } else {
      var c = null, d = a.offsetWidth, e = a.offsetHeight, f = "img";
      if ("IMG" == b) {
        c = a.getAttribute("src");
      } else {
        var f = "background-image", c = pagespeed.MobUtil.findBackgroundImage(a), g = null == c ? null : this.psMob_.findImageSize(c);
        g && (d = g.width, e = g.height);
      }
      null != c ? (g = d / this.maxWidth_, 1 < g && (g = e / g, console.log("Shrinking " + f + " " + c + " from " + d + "x" + e + " to " + this.maxWidth_ + "x" + g), "IMG" == b ? (pagespeed.MobUtil.setPropertyImportant(a, "width", "" + this.maxWidth_ + "px"), pagespeed.MobUtil.setPropertyImportant(a, "height", "" + g + "px")) : pagespeed.MobUtil.setPropertyImportant(a, "background-size", "" + this.maxWidth_ + "px " + g + "px"))) : "CODE" == b || "PRE" == b || "UL" == b ? this.makeHorizontallyScrollable_(a) : 
      pagespeed.MobLayout.FLEXIBLE_WIDTH_TAGS_[b] ? (pagespeed.MobUtil.setPropertyImportant(a, "max-width", "100%"), pagespeed.MobUtil.removeProperty(a, "width")) : console.log("Punting on resize of " + b + " which wants to be " + a.offsetWidth + " but this.maxWidth_=" + this.maxWidth_);
    }
  }
};
pagespeed.MobLayout.prototype.countContainers_ = function(a) {
  var b = 0, c = a.tagName.toUpperCase();
  "DIV" != c && "TABLE" != c && "UL" != c || ++b;
  for (a = a.firstChild;a;a = a.nextSibling) {
    c = pagespeed.MobUtil.castElement(a), null != c && (b += this.countContainers_(c));
  }
  return b;
};
pagespeed.MobLayout.prototype.isDataTable_ = function(a) {
  for (var b = 0, c = a.firstChild;c;c = c.nextSibling) {
    for (var d = c.firstChild;d;d = d.nextSibling) {
      var e = c.tagName.toUpperCase();
      if ("THEAD" == e || "TFOOT" == e) {
        return!0;
      }
      for (e = d.firstChild;e;e = e.nextSibling) {
        if (e.tagName && "TH" == e.tagName.toUpperCase()) {
          return!0;
        }
        ++b;
      }
    }
  }
  return 3 * this.countContainers_(a) > b ? !1 : !0;
};
pagespeed.MobLayout.prototype.reorganizeTableQuirksMode_ = function(a, b) {
  var c, d, e, f, g, h, k, l = document.createElement("DIV");
  l.style.display = "inline-block";
  var m = this.childElements_(a);
  for (c = 0;c < m.length;++c) {
    var n = this.childElements_(m[c]);
    for (d = 0;d < n.length;++d) {
      var p = this.childElements_(n[d]);
      for (e = 0;e < p.length;++e) {
        if (h = p[e], 1 == h.childNodes.length) {
          g = h.childNodes[0], h.removeChild(g), l.appendChild(g);
        } else {
          if (1 < h.childNodes.length) {
            k = document.createElement("DIV");
            k.style.display = "inline-block";
            var q = this.childElements_(h);
            for (f = 0;f < q.length;++f) {
              g = q[f], h.removeChild(g), k.appendChild(g);
            }
            l.appendChild(k);
          }
        }
      }
    }
  }
  a.parentNode.replaceChild(l, a);
};
pagespeed.MobLayout.prototype.reorganizeTableNoQuirksMode_ = function(a, b) {
  var c, d, e, f;
  pagespeed.MobUtil.removeProperty(a, "width");
  pagespeed.MobUtil.setPropertyImportant(a, "max-width", "100%");
  for (c = a.firstChild;c;c = c.nextSibling) {
    if (d = pagespeed.MobUtil.castElement(c), null != d) {
      for (pagespeed.MobUtil.removeProperty(d, "width"), pagespeed.MobUtil.setPropertyImportant(d, "max-width", "100%"), d = d.firstChild;d;d = d.nextSibling) {
        if (e = pagespeed.MobUtil.castElement(d), null != e && "TR" == e.tagName.toUpperCase()) {
          for (pagespeed.MobUtil.removeProperty(e, "width"), pagespeed.MobUtil.setPropertyImportant(e, "max-width", "100%"), e = e.firstChild;e;e = e.nextSibling) {
            f = pagespeed.MobUtil.castElement(e), null != f && "TD" == f.tagName.toUpperCase() && (pagespeed.MobUtil.setPropertyImportant(f, "max-width", "100%"), pagespeed.MobUtil.setPropertyImportant(f, "display", "inline-block"));
          }
        }
      }
    }
  }
};
pagespeed.MobLayout.prototype.cleanupStyles_ = function(a) {
  var b = document.body.style.display;
  document.body.style.display = "none";
  this.cleanupStylesHelper_(a);
  document.body.style.display = b;
};
pagespeed.MobLayout.prototype.cleanupStylesHelper_ = function(a) {
  var b = window.getComputedStyle(a);
  "nowrap" == b.getPropertyValue("white-space") && pagespeed.MobUtil.setPropertyImportant(a, "white-space", "normal");
  this.forEachMobilizableChild_(a, this.cleanupStylesHelper_);
  var b = window.getComputedStyle(a), c, d, e;
  for (c = 0;c < pagespeed.MobLayout.NO_PERCENT_.length;++c) {
    d = pagespeed.MobLayout.NO_PERCENT_[c], (e = b.getPropertyValue(d)) && "100%" != e && "auto" != e && 0 < e.length && "%" == e[e.length - 1] && pagespeed.MobUtil.setPropertyImportant(a, d, "auto");
  }
  c = a.tagName.toUpperCase();
  var f = "UL" == c || "OL" == c, g = "BODY" == c, h = !1, k = "";
  for (c = 0;c < pagespeed.MobLayout.CLAMPED_STYLES_.length;++c) {
    d = pagespeed.MobLayout.CLAMPED_STYLES_[c], f && goog.string.endsWith(d, "-left") || g && goog.string.startsWith(d, "margin-") || (e = pagespeed.MobUtil.computedDimension(b, d), null != e && (4 < e ? k += d + ":4px !important;" : 0 > e && (h = !0, "margin-bottom" == d && (h = -30 < e), h ? k += d + ":0px !important;" : a.setAttribute(pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_, "1"))));
  }
  pagespeed.MobUtil.addStyles(a, k);
};
pagespeed.MobLayout.prototype.repairDistortedImages_ = function(a) {
  this.forEachMobilizableChild_(a, this.repairDistortedImages_);
  if ("IMG" == a.tagName.toUpperCase()) {
    var b = window.getComputedStyle(a), c = pagespeed.MobUtil.findRequestedDimension(a, "width"), d = pagespeed.MobUtil.findRequestedDimension(a, "height");
    if (c && d && b) {
      var e = pagespeed.MobUtil.computedDimension(b, "width"), b = pagespeed.MobUtil.computedDimension(b, "height");
      e && b && (e /= c, b /= d, pagespeed.MobUtil.aboutEqual(e, b) || (console.log("aspect ratio problem for " + a.getAttribute("src")), pagespeed.MobUtil.isSinglePixel(a) ? (b = Math.min(e, b), pagespeed.MobUtil.removeProperty(a, "width"), pagespeed.MobUtil.removeProperty(a, "height"), a.style.width = c * b, a.style.height = d * b) : e > b ? pagespeed.MobUtil.removeProperty(a, "height") : (pagespeed.MobUtil.removeProperty(a, "width"), pagespeed.MobUtil.removeProperty(a, "height"), a.style.maxHeight = 
      d)), .25 > e && (console.log("overshrinkage for " + a.getAttribute("src")), this.reallocateWidthToTableData_(a)));
    }
  }
};
pagespeed.MobLayout.prototype.reallocateWidthToTableData_ = function(a) {
  for (;a && a.tagName && "TD" != a.tagName.toUpperCase();) {
    a = a.parentNode;
  }
  if (a) {
    var b = a.parentNode;
    if (b) {
      var c = 0;
      for (a = b.firstChild;a;a = a.nextSibling) {
        a.tagName && "TD" == a.tagName.toUpperCase() && ++c;
      }
      if (1 < c) {
        for (c = "width:" + Math.round(100 / c) + "%;", b = b.firstChild;b;b = b.nextSibling) {
          a = pagespeed.MobUtil.castElement(b), null != a && "TD" == a.tagName.toUpperCase() && pagespeed.MobUtil.addStyles(a, c);
        }
      }
    }
  }
};
pagespeed.MobLayout.prototype.isPossiblyASlideShow_ = function(a) {
  return a.classList.contains("nivoSlider") ? !0 : !1;
};
pagespeed.MobLayout.prototype.stripFloats_ = function(a) {
  var b = window.getComputedStyle(a).getPropertyValue("position");
  if ("fixed" == b) {
    return "fixed";
  }
  if (this.isPossiblyASlideShow_(a)) {
    return b;
  }
  var c, d, e, f, g = [];
  c = null;
  var h, k = !1;
  for (d = a.firstChild;d;d = d.nextSibling) {
    if (e = this.getMobilizeElement(d), null != e) {
      var l = window.getComputedStyle(e);
      f = this.stripFloats_(e);
      if ("fixed" != f && null != l && null != this.getMobilizeElement(e)) {
        "absolute" != f || pagespeed.MobUtil.isOffScreen(l) || pagespeed.MobUtil.setPropertyImportant(e, "position", "relative");
        f = l.getPropertyValue("float");
        var m = "right" == f;
        h = "inline-block";
        if (m || "left" == f) {
          m && "right" == l.getPropertyValue("clear") && (m = !1, h = "block", c && k && pagespeed.MobUtil.setPropertyImportant(c, "margin-bottom", "0px")), pagespeed.MobUtil.setPropertyImportant(e, "float", "none"), "none" != l.getPropertyValue("display") && pagespeed.MobUtil.setPropertyImportant(e, "display", h);
        }
        m && g.push(e);
        c = e;
        e = pagespeed.MobUtil.computedDimension(l, "margin-bottom");
        k = null != e && 0 > e;
      }
    }
  }
  for (c = g.length - 1;0 <= c;--c) {
    d = g[c], a.removeChild(d);
  }
  for (c = g.length - 1;0 <= c;--c) {
    d = g[c], a.appendChild(d);
  }
  return b;
};
pagespeed.MobLayout.prototype.removeWidthConstraint_ = function(a, b) {
  var c = a.tagName.toUpperCase();
  "INPUT" != c && "SELECT" != c && ("" == a.style.backgroundSize && "auto" != b.width && pagespeed.MobUtil.setPropertyImportant(a, "width", "auto"), "IMG" != c && a.removeAttribute("width"), pagespeed.MobUtil.removeProperty(a, "border-left"), pagespeed.MobUtil.removeProperty(a, "border-right"), pagespeed.MobUtil.removeProperty(a, "margin-left"), pagespeed.MobUtil.removeProperty(a, "margin-right"), pagespeed.MobUtil.removeProperty(a, "padding-left"), pagespeed.MobUtil.removeProperty(a, "padding-right"), 
  a.className = "" != a.className ? a.className + " psSingleColumn" : "psSingleColumn");
};
pagespeed.MobLayout.prototype.expandColumns_ = function(a) {
  if ("fixed" != window.getComputedStyle(a).getPropertyValue("position")) {
    var b, c, d, e = [], f = [];
    for (c = a.firstChild;c;c = c.nextSibling) {
      if (d = this.getMobilizeElement(c), null != d) {
        b = window.getComputedStyle(d);
        var g = b.getPropertyValue("position");
        "fixed" != g && "absolute" != g && 0 != c.offsetWidth && (e.push(d), f.push(b));
      }
    }
    var h = null;
    for (c = 0;c < e.length;++c) {
      d = e[c], b = c < e.length - 1 ? e[c + 1] : null, g = d.offsetLeft + d.offsetWidth, (null == h || d.offsetLeft < h) && (null == b || b.offsetLeft < g) && (this.removeWidthConstraint_(d, f[c]), this.expandColumns_(d)), a.getAttribute(pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_) && (a.removeAttribute(pagespeed.MobLayout.NEGATIVE_BOTTOM_MARGIN_ATTR_), b = window.getComputedStyle(a), d = pagespeed.MobUtil.computedDimension(b, "height"), null != d && pagespeed.MobUtil.setPropertyImportant(a, 
      "margin-bottom", "" + -d + "px")), h = g;
    }
  }
};
pagespeed.MobLayout.sequence_ = [pagespeed.MobLayout.prototype.shrinkWideElements_, "shrink wide elements", pagespeed.MobLayout.prototype.stripFloats_, "string floats", pagespeed.MobLayout.prototype.cleanupStyles_, "cleanup styles", pagespeed.MobLayout.prototype.repairDistortedImages_, "repair distored images", pagespeed.MobLayout.prototype.resizeIfTooWide_, "resize if too wide", pagespeed.MobLayout.prototype.expandColumns_, "expand columns", pagespeed.MobLayout.prototype.resizeVertically_, "resize vertically"];
pagespeed.MobLogo = function(a) {
  this.psMob_ = a;
  this.candidates_ = [];
};
pagespeed.MobLogo.LogoRecord = function() {
  this.metric = -1;
  this.foregroundElement = this.logoElement = null;
  this.foregroundImage = "";
  this.backgroundColor = this.backgroundRect = this.backgroundImage = this.backgroundElement = this.foregroundRect = this.foregroundSource = null;
};
pagespeed.MobLogo.prototype.MIN_WIDTH_ = 20;
pagespeed.MobLogo.prototype.MIN_HEIGHT_ = 10;
pagespeed.MobLogo.prototype.MAX_HEIGHT_ = 400;
pagespeed.MobLogo.prototype.MIN_PIXELS_ = 400;
pagespeed.MobLogo.prototype.MAX_TOP_ = 6E3;
pagespeed.MobLogo.prototype.RATIO_AREA_ = .5;
pagespeed.MobLogo.findLogoInFileName = function(a) {
  return a && (a = a.toLowerCase(), 0 <= a.indexOf("logo") && 0 > a.indexOf("logout") && 0 > a.indexOf("no_logo") && 0 > a.indexOf("no-logo")) ? 1 : 0;
};
pagespeed.MobLogo.prototype.findForeground_ = function(a, b, c) {
  var d = pagespeed.MobUtil.boundingRectAndSize(a), e = "hidden" != this.psMob_.getVisibility(a), f = d.width * d.height, g = d.width > this.MIN_WIDTH_ && d.height > this.MIN_HEIGHT_ && f > this.MIN_PIXELS_ && d.top < this.MAX_TOP_ && d.height < this.MAX_HEIGHT_;
  if (e && g && f >= b) {
    var f = e = null, h;
    for (h in pagespeed.MobUtil.ImageSource) {
      if (f = pagespeed.MobUtil.extractImage(a, pagespeed.MobUtil.ImageSource[h])) {
        e = pagespeed.MobUtil.ImageSource[h];
        g = !0;
        if (e == pagespeed.MobUtil.ImageSource.IMG) {
          var k = this.psMob_.findImageSize(a.src);
          k ? (d.width = k.width, d.height = k.height) : a.naturalWidth ? (d.width = a.naturalWidth, d.height = a.naturalHeight) : d.width && d.height || (console.log("Image " + a.src + " may be the logo. It has not been loaded so may be missed."), g = !1);
          g && (d.width <= this.MIN_WIDTH_ || d.height <= this.MIN_HEIGHT_ || d.height >= this.MAX_HEIGHT_) && (g = !1);
        }
        if (g) {
          return b = new pagespeed.MobLogo.LogoRecord, b.foregroundImage = f, b.foregroundElement = a, b.foregroundSource = e, b.foregroundRect = d, b;
        }
      }
    }
  }
  if (c) {
    for (a = a.firstChild;a;a = a.nextSibling) {
      if (d = pagespeed.MobUtil.castElement(a), null != d && (d = this.findForeground_(d, b, c))) {
        return d;
      }
    }
  } else {
    if (a.parentNode && (a = pagespeed.MobUtil.castElement(a.parentNode), null != a)) {
      return this.findForeground_(a, b, c);
    }
  }
  return null;
};
pagespeed.MobLogo.prototype.findLogoNode_ = function(a, b) {
  var c = pagespeed.MobUtil.boundingRectAndSize(a), d = "hidden" != this.psMob_.getVisibility(a);
  if (!(c.top < this.MAX_TOP_ && c.height < this.MAX_HEIGHT_ && d)) {
    return null;
  }
  d = 0;
  a.title && (d += goog.string.caseInsensitiveContains(a.title, "logo"));
  a.id && (d += goog.string.caseInsensitiveContains(a.id, "logo"));
  a.className && (d += goog.string.caseInsensitiveContains(a.className, "logo"));
  a.alt && (d += goog.string.caseInsensitiveContains(a.alt, "logo"));
  var e = pagespeed.MobUtil.getSiteOrganization(), f = 0;
  e && (a.id && (d += goog.string.caseInsensitiveContains(a.id, e)), a.className && (d += goog.string.caseInsensitiveContains(a.className, e)), a.title && (f += pagespeed.MobUtil.findPattern(a.title, e)), a.alt && (f += pagespeed.MobUtil.findPattern(a.alt, e)));
  var g = c.width * c.height, h = g * this.RATIO_AREA_, c = 0;
  a.href && a.href == window.location.origin + window.location.pathname && ++c;
  (h = this.findForeground_(a, h, !0)) || (h = this.findForeground_(a, g, !1));
  return h && (g = pagespeed.MobUtil.resourceFileName(h.foregroundImage), d += pagespeed.MobLogo.findLogoInFileName(g), g && e && (f += pagespeed.MobUtil.findPattern(g, e)), d = d + f + c, 0 < d) ? (h.metric = d, h.logoElement = a, h) : null;
};
pagespeed.MobLogo.prototype.findLogoCandidates_ = function(a, b) {
  var c = this.findLogoNode_(a, b);
  c && (this.candidates_.push(c), ++b);
  for (c = a.firstChild;c;c = c.nextSibling) {
    var d = pagespeed.MobUtil.castElement(c);
    null != d && this.findLogoCandidates_(d, b);
  }
};
pagespeed.MobLogo.prototype.findBestLogo_ = function() {
  var a = null, b = this.candidates_;
  if (!b || 0 == b.length) {
    return null;
  }
  if (1 == b.length) {
    return a = b[0];
  }
  for (var c = 0, d = Infinity, e, f, a = 0;f = b[a];++a) {
    e = f.foregroundRect, d = Math.min(d, e.top), c = Math.max(c, e.bottom);
  }
  for (a = 0;f = b[a];++a) {
    e = f.foregroundRect, e = Math.sqrt((c - e.top) / (c - d)), f.metric *= e;
  }
  for (a = e = 0;f = b[a];++a) {
    e = Math.max(e, f.metric);
  }
  c = [];
  for (a = 0;f = b[a];++a) {
    f.metric == e && c.push(f);
  }
  if (1 == c.length) {
    return a = c[0];
  }
  b = c[0];
  d = b.foregroundRect;
  for (a = 1;f = c[a];++a) {
    if (e = f.foregroundRect, d.top > e.top || d.top == e.top && d.left > e.left || d.top == e.top && d.left == e.left && d.width * d.height > e.width * e.height) {
      b = f, d = b.foregroundRect;
    }
  }
  return b;
};
pagespeed.MobLogo.prototype.extractBackgroundColor_ = function(a) {
  return(a = document.defaultView.getComputedStyle(a, null)) && (a = a.getPropertyValue("background-color")) && (a = pagespeed.MobUtil.colorStringToNumbers(a)) && (3 == a.length || 4 == a.length && 0 != a[3]) ? a : null;
};
pagespeed.MobLogo.prototype.findLogoBackground_ = function(a) {
  if (!a || !a.foregroundElement) {
    return null;
  }
  var b = a.foregroundElement, c = null;
  if (a.foregroundSource == pagespeed.MobUtil.ImageSource.IMG || a.foregroundSource == pagespeed.MobUtil.ImageSource.SVG) {
    c = pagespeed.MobUtil.extractImage(b, pagespeed.MobUtil.ImageSource.BACKGROUND);
  }
  var d = this.extractBackgroundColor_(b), e = null;
  for (b.parentNode && (e = pagespeed.MobUtil.castElement(b.parentNode));e && !c && !d;) {
    b = e, c = pagespeed.MobUtil.extractImage(b, pagespeed.MobUtil.ImageSource.IMG) || pagespeed.MobUtil.extractImage(b, pagespeed.MobUtil.ImageSource.SVG) || pagespeed.MobUtil.extractImage(b, pagespeed.MobUtil.ImageSource.BACKGROUND), d = this.extractBackgroundColor_(b), e = b.parentNode ? pagespeed.MobUtil.castElement(b.parentNode) : null;
  }
  a.backgroundElement = b;
  a.backgroundImage = c;
  a.backgroundColor = d || [255, 255, 255];
  a.backgroundRect = pagespeed.MobUtil.boundingRectAndSize(b);
  return a;
};
pagespeed.MobLogo.prototype.run = function() {
  if (!document.body) {
    return null;
  }
  this.findLogoCandidates_(document.body, 0);
  var a = this.findBestLogo_();
  return this.findLogoBackground_(a);
};
goog.exportSymbol("pagespeed.MobLogo.prototype.run", pagespeed.MobLogo.prototype.run);
pagespeed.MobNav = function() {
  this.navSections_ = [];
  this.useDetectedThemeColor_ = !0;
  this.scrollTimer_ = this.navPanel_ = this.callButton_ = this.menuButton_ = this.logoSpan_ = this.spacerDiv_ = this.headerBar_ = null;
  this.currentTouches_ = 0;
};
pagespeed.MobNav.HEADER_BAR_HEIGHT_ = 60;
pagespeed.MobNav.prototype.ARROW_ICON_ = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAJAAAACQCAQAAABNTyozAAAAIGNIUk0AAHomAACAhAAA+gAAAIDoAAB1MAAA6mAAADqYAAAXcJy6UTwAAAACYktHRAD/h4/MvwAAAAlwSFlzAAALEwAACxMBAJqcGAAAATdJREFUeNrt0klOBTEQREHfDI7FTEn/4M2GBULQo22wK+ICKT1lKQAAAAAAAAAAAAAAAAAA/Jm7rmv3o+W5laU8d1uLspQYLc/SLVF8rsVoefokii9rMVqe9oni21qMlqdtovhhLUbL0y5R/LIWo+VZylKeuuX5t4nW8tRPFBtrMV6gmolic+t9xA/VSjRonl6JBs7TI9HgeVonmiDPvkSPmfO0SjRRnhaJJstTO9GEeWom2s7zVgZVI9HEeWokmjzPvkQPmfNcSZQkz9lEifKcSZQsz9FECfMcSZQ0z95E23ley8S2E6XOcz3R9HmuJUqR53yiNHnOJUqV53iidHmOJUqZZ3+itHn2JXopyd3kOZ9IntVE8qwmkmc1kTyrieRZTSTPaiJ5AAAAAAAAAAAAAAAAAGjgA62rM0XB6dNxAAAAAElFTkSuQmCC";
pagespeed.MobNav.prototype.CALL_BUTTON_ = "R0lGODlhgACAAPAAAAAAAAAAACH5BAEAAAEALAAAAACAAIAAAAL+jI+pCL0Po5y02vuaBrj7D4bMtonmiXrkyqXuC7MsTNefLNv6nuE4D9z5fMFibEg0KkVI5PLZaTah1IlUWs0qrletl9v1VsFcMZQMNivRZHWRnXbz4G25jY621/B1vYuf54cCyCZ4QlhoGIIYqKjC2Oh4AZkoaUEZaWmF2acpwZnpuQAaKjpCWmbag5qqmsAa53oK6yT7SjtkO4r7o7vLS+K7Cuwg/EtsDIGcrMzLHOH8DM0qvUlabY2JXaG9zc3ojYEYLk5IXs53Pgmovo7XfskOTyE//1lv3/yeP53Or0/nH8CAAo/BKTjsIMJb/hYewOcwAMSF5iIamEixYcTMihY5bsRY0GNGkP9Ejtx3poUbk0GCrSR5Z8VLmDRyqBnXMokYnEJq7WT5J8wXni86ZQF3JJYWpCkILiXKBOUYpouAGqEU1eobSCCwHvXqDmxKrmHFPuH07drUbv3UUgHVFtVXuFuijVVLrNjbvLTm8pW79q/bu4LZ7i2M1i9isoEXQz3smObVyBqHUlZ483Kpn5qxCOrs+TNonYZG27RkuoSo1HpXj7YFWtjlZJGlId72l9wy3bjmweI3OJ/hkFqBO7U4KzTyuDKXaykAADs=";
pagespeed.MobNav.prototype.callButtonImage_ = function(a) {
  var b = window.atob(this.CALL_BUTTON_);
  a = b.substring(0, 13) + String.fromCharCode(a[0], a[1], a[2]) + b.substring(16, b.length);
  return "data:image/gif;base64," + window.btoa(a);
};
pagespeed.MobNav.prototype.findNavSections_ = function() {
  var a;
  if (window.pagespeedNavigationalIds) {
    var b = window.pagespeedNavigationalIds.length;
    a = Array(b);
    for (var c = 0;c < b;c++) {
      var d = window.pagespeedNavigationalIds[c];
      a[c] = document.getElementById(d) || document.querySelector("[id=" + pagespeed.MobUtil.toCssString1(d) + "]");
    }
  } else {
    a = [];
  }
  this.navSections_ = a;
};
pagespeed.MobNav.prototype.fixExistingElements_ = function() {
  for (var a = document.getElementsByTagName("*"), b = 0, c;c = a[b];b++) {
    var d = window.getComputedStyle(c);
    if ("fixed" == d.getPropertyValue("position")) {
      var e = c.getBoundingClientRect().top;
      c.style.top = String(pagespeed.MobNav.HEADER_BAR_HEIGHT_ + e) + "px";
    }
    999999 <= d.getPropertyValue("z-index") && (console.log("Element z-index exceeded 999999, setting to 999998."), c.style.zIndex = 999998);
  }
};
pagespeed.MobNav.prototype.redrawHeader_ = function() {
  this.headerBar_.style.top = window.scrollY + "px";
  this.headerBar_.style.left = window.scrollX + "px";
  var a = "scale(" + window.innerWidth / document.documentElement.clientWidth + ")";
  this.headerBar_.style["-webkit-transform"] = a;
  this.headerBar_.style.transform = a;
  goog.dom.classlist.remove(this.headerBar_, "hide");
  a = this.headerBar_.offsetHeight;
  this.menuButton_.style.width = a + "px";
  this.menuButton_.style.height = a + "px";
  this.callButton_.style.width = a + "px";
  this.callButton_.style.height = a + "px";
  this.spacerDiv_.style.height = a + "px";
};
pagespeed.MobNav.prototype.addHeaderBarResizeEvents_ = function() {
  this.redrawHeader_();
  window.addEventListener("scroll", goog.bind(function() {
    null != this.scrollTimer_ && (window.clearTimeout(this.scrollTimer_), this.scrollTimer_ = null);
    this.scrollTimer_ = window.setTimeout(goog.bind(function() {
      0 == this.currentTouches_ && this.redrawHeader_();
      this.scrollTimer_ = null;
    }, this), 50);
  }, this), !1);
  window.addEventListener("touchstart", goog.bind(function(a) {
    this.currentTouches_ = a.targetTouches.length;
  }, this), !1);
  window.addEventListener("touchmove", goog.bind(function() {
    goog.dom.classlist.contains(document.body, "noscroll") || goog.dom.classlist.add(this.headerBar_, "hide");
  }, this), !1);
  window.addEventListener("touchend", goog.bind(function(a) {
    this.currentTouches_ = a.targetTouches.length;
    null == this.scrollTimer_ && 0 == this.currentTouches_ && this.redrawHeader_();
  }, this), !1);
};
pagespeed.MobNav.prototype.addHeaderBar_ = function(a) {
  this.spacerDiv_ = document.createElement("div");
  document.body.insertBefore(this.spacerDiv_, document.body.childNodes[0]);
  goog.dom.classlist.add(this.spacerDiv_, "psmob-header-spacer-div");
  this.headerBar_ = document.createElement("header");
  document.body.insertBefore(this.headerBar_, this.spacerDiv_);
  goog.dom.classlist.add(this.headerBar_, "psmob-header-bar");
  this.menuButton_ = a.menuButton;
  this.headerBar_.appendChild(this.menuButton_);
  this.logoSpan_ = a.logoSpan;
  this.headerBar_.appendChild(this.logoSpan_);
  this.headerBar_.style.borderBottom = "thin solid " + pagespeed.MobUtil.colorNumbersToString(a.menuFrontColor);
  this.headerBar_.style.backgroundColor = pagespeed.MobUtil.colorNumbersToString(a.menuBackColor);
  if (window.psAddCallButton) {
    this.callButton_ = document.createElement("button");
    goog.dom.classlist.add(this.callButton_, "psmob-call-button");
    var b = document.createElement("img");
    b.src = this.callButtonImage_(a.menuFrontColor);
    this.callButton_.appendChild(b);
    this.headerBar_.appendChild(this.callButton_);
  }
  this.addHeaderBarResizeEvents_();
};
pagespeed.MobNav.prototype.addThemeColor_ = function(a) {
  var b = this.useDetectedThemeColor_ ? pagespeed.MobUtil.colorNumbersToString(a.menuBackColor) : "#3c78d8";
  a = this.useDetectedThemeColor_ ? pagespeed.MobUtil.colorNumbersToString(a.menuFrontColor) : "white";
  b = ".psmob-header-bar { background-color: " + b + " }\n.psmob-nav-panel { background-color: " + a + " }\n.psmob-nav-panel > ul li { color: " + b + " }\n.psmob-nav-panel > ul li a { color: " + b + " }\n";
  a = document.createElement("style");
  a.type = "text/css";
  a.appendChild(document.createTextNode(b));
  document.head.appendChild(a);
};
pagespeed.MobNav.prototype.labelNavDepth_ = function(a, b) {
  for (var c = [], d = a.firstChild;d;d = d.nextSibling) {
    "UL" == d.tagName ? c = goog.array.join(c, this.labelNavDepth_(d, b + 1)) : ("A" == d.tagName && (d.setAttribute("data-mobilize-nav-level", b), c.push(d)), c = goog.array.join(c, this.labelNavDepth_(d, b)));
  }
  return c;
};
pagespeed.MobNav.prototype.dedupNavMenuItems_ = function() {
  for (var a = document.querySelector(".psmob-nav-panel > ul a"), b = {}, c = [], d = 0, e;e = a[d];d++) {
    if (e.href in b) {
      var f = e.innerHTML.toLowerCase();
      -1 == b[e.href].indexOf(f) ? b[e.href].push(f) : "LI" == e.parentNode.tagName && c.push(e.parentNode);
    } else {
      b[e.href] = [], b[e.href].push(e.innerHTML.toLowerCase());
    }
  }
  for (d = 0;a = c[d];d++) {
    a.parentNode.removeChild(a);
  }
};
pagespeed.MobNav.prototype.cleanupNavPanel_ = function() {
  for (var a = this.navPanel_.querySelectorAll("*"), b = 0, c;c = a[b];b++) {
    c.removeAttribute("style"), c.removeAttribute("width"), c.removeAttribute("height"), "A" == c.tagName && "" == c.innerText && c.hasAttribute("title") && c.appendChild(document.createTextNode(c.getAttribute("title")));
  }
  a = this.navPanel_.querySelectorAll("img:not(.psmob-menu-expand-icon)");
  for (b = 0;c = a[b];++b) {
    var d = Math.min(2 * c.naturalHeight, 40);
    c.setAttribute("height", d);
  }
};
pagespeed.MobNav.prototype.addNavPanel_ = function() {
  this.navPanel_ = document.createElement("nav");
  document.body.insertBefore(this.navPanel_, this.headerBar_.nextSibling);
  goog.dom.classlist.add(this.navPanel_, "psmob-nav-panel");
  var a = document.createElement("ul");
  this.navPanel_.appendChild(a);
  goog.dom.classlist.add(a, "open");
  for (var b = 0, c;c = this.navSections_[b];b++) {
    c.setAttribute("data-mobilize-nav-section", b);
    var d = this.labelNavDepth_(c, 0), e = [];
    e.push(a);
    for (var f = 0, g = d.length;f < g;f++) {
      var h = d[f].getAttribute("data-mobilize-nav-level"), k = f + 1 == g ? h : d[f + 1].getAttribute("data-mobilize-nav-level");
      if (h < k) {
        var l = document.createElement("li"), h = l.appendChild(document.createElement("div")), k = document.createElement("img");
        h.appendChild(document.createElement("img"));
        k.setAttribute("src", this.ARROW_ICON_);
        goog.dom.classlist.add(k, "psmob-menu-expand-icon");
        h.appendChild(document.createTextNode(d[f].textContent || d[f].innerText));
        e[e.length - 1].appendChild(l);
        h = document.createElement("ul");
        l.appendChild(h);
        e.push(h);
      } else {
        for (l = document.createElement("li"), e[e.length - 1].appendChild(l), l.appendChild(d[f].cloneNode(!0)), l = h - k;0 < l && 1 < e.length;) {
          e.pop(), l--;
        }
      }
    }
    c.parentNode.removeChild(c);
  }
  this.dedupNavMenuItems_();
  this.cleanupNavPanel_();
};
pagespeed.MobNav.prototype.toggleNavPanel_ = function() {
  goog.dom.classlist.toggle(this.headerBar_, "open");
  goog.dom.classlist.toggle(this.navPanel_, "open");
  goog.dom.classlist.toggle(document.body, "noscroll");
};
pagespeed.MobNav.prototype.addMenuButtonEvents_ = function() {
  document.body.addEventListener("click", function(a) {
    this.menuButton_.contains(a.target) ? this.toggleNavPanel_() : goog.dom.classlist.contains(this.navPanel_, "open") && !this.navPanel_.contains(a.target) && (this.toggleNavPanel_(), a.stopPropagation(), a.preventDefault());
  }.bind(this), !0);
};
pagespeed.MobNav.prototype.addNavButtonEvents_ = function() {
  document.querySelector("nav.psmob-nav-panel > ul").addEventListener("click", function(a) {
    a = goog.dom.isElement(a.target) && goog.dom.classlist.contains(a.target, "psmob-menu-expand-icon") ? a.target.parentNode : a.target;
    "DIV" == a.tagName && (goog.dom.classlist.toggle(a.nextSibling, "open"), goog.dom.classlist.toggle(a.firstChild, "open"));
  });
};
pagespeed.MobNav.prototype.Run = function(a) {
  console.log("Starting nav resynthesis.");
  this.findNavSections_();
  this.fixExistingElements_();
  this.addHeaderBar_(a);
  this.addThemeColor_(a);
  0 == this.navSections_.length || pagespeed.MobUtil.inFriendlyIframe() || (this.addNavPanel_(), this.addMenuButtonEvents_(), this.addNavButtonEvents_());
};
pagespeed.MobTheme = function() {
  this.logo = null;
};
pagespeed.MobTheme.createMenuButton_ = function(a) {
  var b = document.createElement("button");
  goog.dom.classlist.add(b, "psmob-menu-button");
  var c = document.createElement("div");
  goog.dom.classlist.add(c, "psmob-hamburger-div");
  b.appendChild(c);
  a = pagespeed.MobUtil.colorNumbersToString(a);
  for (var d = 0;3 > d;++d) {
    var e = document.createElement("div");
    goog.dom.classlist.add(e, "psmob-hamburger-line");
    e.style.backgroundColor = a;
    c.appendChild(e);
  }
  return b;
};
pagespeed.MobTheme.synthesizeLogoSpan_ = function(a, b, c) {
  var d = document.createElement("span");
  d.id = "psmob-logo-span";
  if (a && a.foregroundImage) {
    var e = document.createElement("IMG");
    e.src = a.foregroundImage;
    e.style.backgroundColor = pagespeed.MobUtil.colorNumbersToString(b);
    e.id = "psmob-logo-image";
    d.appendChild(e);
  } else {
    d.textContent = window.location.host, d.style.color = pagespeed.MobUtil.colorNumbersToString(c);
  }
  a = pagespeed.MobTheme.createMenuButton_(c);
  return new pagespeed.MobUtil.ThemeData(c, b, a, d);
};
pagespeed.MobTheme.removeLogoImage_ = function(a) {
  if (a && a.foregroundElement && a.foregroundSource) {
    var b = a.foregroundElement;
    switch(a.foregroundSource) {
      case pagespeed.MobUtil.ImageSource.IMG:
      ;
      case pagespeed.MobUtil.ImageSource.SVG:
        b.parentNode.removeChild(b);
        break;
      case pagespeed.MobUtil.ImageSource.BACKGROUND:
        b.style.backgroundImage = "none";
    }
  }
};
pagespeed.MobTheme.prototype.colorComplete_ = function(a, b, c) {
  b = pagespeed.MobTheme.synthesizeLogoSpan_(a, b, c);
  pagespeed.MobTheme.removeLogoImage_(a);
  this.doneCallback(b);
};
pagespeed.MobTheme.extractTheme = function(a, b) {
  b || alert("Not expecting to start onloads after the callback is called");
  var c = new pagespeed.MobTheme;
  c.doneCallback = b;
  c.logo = (new pagespeed.MobLogo(a)).run();
  (new pagespeed.MobColor).run(c.logo, goog.bind(c.colorComplete_, c));
};
pagespeed.Mob = function() {
  this.activeRequestCount_ = 0;
  this.imageMap_ = {};
  this.startTimeMs_ = Date.now();
  this.debugMode_ = !1;
  this.pendingImageLoadCount_ = this.workDone_ = this.prevPercentage_ = this.totalWork_ = this.domElementCount_ = 0;
  this.mobilizeAfterImageLoad_ = !1;
  this.workPerLayoutPass_ = this.pendingCallbacks_ = 0;
  this.layout_ = new pagespeed.MobLayout(this);
  this.layout_.addDontTouchId(pagespeed.Mob.PROGRESS_SCRIM_ID_);
};
pagespeed.Mob.PROGRESS_SAVE_VISIBLITY_ = "ps-save-visibility";
pagespeed.Mob.PROGRESS_SCRIM_ID_ = "ps-progress-scrim";
pagespeed.Mob.PROGRESS_REMOVE_ID_ = "ps-progress-remove";
pagespeed.Mob.PROGRESS_LOG_ID_ = "ps-progress-log";
pagespeed.Mob.PROGRESS_SPAN_ID_ = "ps-progress-span";
pagespeed.Mob.PROGRESS_SHOW_LOG_ID_ = "ps-progress-show-log";
pagespeed.Mob.IN_TRANSIT_ = new pagespeed.MobUtil.Dimensions(-1, -1);
pagespeed.Mob.COST_PER_IMAGE_ = 1E3;
pagespeed.Mob.prototype.mobilizeSite_ = function() {
  0 == this.pendingImageLoadCount_ ? (console.log("mobilizing site"), window.psNavMode && !pagespeed.MobUtil.inFriendlyIframe() ? (++this.pendingCallbacks_, pagespeed.MobTheme.extractTheme(this, this.logoComplete_.bind(this))) : this.maybeRunLayout()) : this.mobilizeAfterImageLoad_ = !0;
};
pagespeed.Mob.prototype.logoComplete_ = function(a) {
  --this.pendingCallbacks_;
  this.updateProgressBar(this.domElementCount_, "extract theme");
  (new pagespeed.MobNav).Run(a);
  this.updateProgressBar(this.domElementCount_, "navigation");
  this.maybeRunLayout();
};
pagespeed.Mob.prototype.backgroundImageLoaded_ = function(a) {
  this.imageMap_[a.src] = new pagespeed.MobUtil.Dimensions(a.width, a.height);
  --this.pendingImageLoadCount_;
  this.updateProgressBar(pagespeed.Mob.COST_PER_IMAGE_, "background image");
  0 == this.pendingImageLoadCount_ && this.mobilizeAfterImageLoad_ && (this.mobilizeSite_(), this.mobilizeAfterImageLoad_ = !1);
};
pagespeed.Mob.prototype.collectBackgroundImages_ = function(a) {
  a = this.layout_.getMobilizeElement(a);
  if (null != a) {
    var b = pagespeed.MobUtil.findBackgroundImage(a);
    if (b && (goog.string.startsWith(b, "http://") || goog.string.startsWith(b, "https://")) && !this.imageMap_[b]) {
      this.imageMap_[b] = pagespeed.Mob.IN_TRANSIT_;
      var c = new Image;
      ++this.pendingImageLoadCount_;
      c.onload = this.backgroundImageLoaded_.bind(this, c);
      c.onerror = c.onload;
      c.src = b;
    }
    for (a = a.firstChild;a;a = a.nextSibling) {
      this.collectBackgroundImages_(a);
    }
  }
};
pagespeed.Mob.prototype.xhrSendHook = function() {
  ++this.activeRequestCount_;
};
pagespeed.Mob.prototype.xhrResponseHook = function(a) {
  --this.activeRequestCount_;
  this.addExtraWorkForDom();
  this.maybeRunLayout();
};
pagespeed.Mob.prototype.initiateMobilization = function() {
  this.setDebugMode(window.psDebugMode);
  this.domElementCount_ = pagespeed.MobUtil.countNodes(document.body);
  this.workPerLayoutPass_ = this.domElementCount_ * pagespeed.MobLayout.numberOfPasses();
  this.addExtraWorkForDom();
  window.psNavMode && pagespeed.MobUtil.inFriendlyIframe() && (this.totalWork_ += this.domElementCount_, this.totalWork_ += this.domElementCount_);
  if (null != document.body) {
    for (var a in window.psMobStaticImageInfo) {
      var b = window.psMobStaticImageInfo[a];
      this.imageMap_[a] = new pagespeed.MobUtil.Dimensions(b.w, b.h);
    }
    this.collectBackgroundImages_(document.body);
  }
  this.totalWork_ += this.pendingImageLoadCount_ * pagespeed.Mob.COST_PER_IMAGE_;
  window.psLayoutMode && window.pagespeedXhrHijackSetListener(this);
  this.mobilizeSite_();
};
pagespeed.Mob.prototype.isReady = function() {
  return 0 == this.activeRequestCount_ && 0 == this.pendingCallbacks_ && 0 == this.pendingImageLoadCount_;
};
pagespeed.Mob.prototype.maybeRunLayout = function() {
  if (this.isReady()) {
    if (window.psLayoutMode && this.layout_.computeAllSizingAndResynthesize(), this.debugMode_) {
      var a = document.getElementById(pagespeed.Mob.PROGRESS_REMOVE_ID_);
      a && (a.textContent = "Remove Progress Bar and show mobilized site");
    } else {
      this.removeProgressBar();
    }
  }
};
pagespeed.Mob.prototype.layoutPassDone = function(a) {
  this.updateProgressBar(this.domElementCount_, a);
};
pagespeed.Mob.prototype.findImageSize = function(a) {
  a = this.imageMap_[a];
  a == pagespeed.Mob.IN_TRANSIT_ && (a = null);
  return a;
};
pagespeed.Mob.prototype.addExtraWorkForDom = function() {
  this.totalWork_ += this.workPerLayoutPass_;
};
pagespeed.Mob.prototype.setDebugMode = function(a) {
  this.debugMode_ = a;
  var b = document.getElementById(pagespeed.Mob.PROGRESS_LOG_ID_);
  b && (b.style.color = a ? "#333" : "white");
  a && (a = document.getElementById(pagespeed.Mob.PROGRESS_SHOW_LOG_ID_)) && (a.style.display = "none");
};
pagespeed.Mob.prototype.getVisibility = function(a) {
  var b = a.getAttribute(pagespeed.Mob.PROGRESS_SAVE_VISIBLITY_);
  b || ((a = window.getComputedStyle(a)) && (b = a.getPropertyValue("visibility")), b || (b = "visible"));
  return b;
};
pagespeed.Mob.prototype.updateProgressBar = function(a, b) {
  this.workDone_ += a;
  var c = 100;
  0 < this.totalWork_ && (c = Math.round(100 * this.workDone_ / this.totalWork_), 100 < c && (c = 100));
  if (c != this.prevPercentage_) {
    var d = document.getElementById(pagespeed.Mob.PROGRESS_SPAN_ID_);
    d && (d.style.width = c + "%");
    this.prevPercentage_ = c;
  }
  d = Date.now() - this.startTimeMs_;
  c = "" + c + "% " + d + "ms: " + b;
  console.log(c);
  if (d = document.getElementById(pagespeed.Mob.PROGRESS_LOG_ID_)) {
    d.textContent += c + "\n";
  }
};
pagespeed.Mob.prototype.removeProgressBar = function() {
  var a = document.getElementById(pagespeed.Mob.PROGRESS_SCRIM_ID_);
  a && (a.style.display = "none", a.parentNode.removeChild(a));
};
var psMob = new pagespeed.Mob;
window.addEventListener("load", goog.bind(psMob.initiateMobilization, psMob));
function psSetDebugMode() {
  psMob.setDebugMode(!0);
}
goog.exportSymbol("psSetDebugMode", psSetDebugMode);
function psRemoveProgressBar() {
  psMob.removeProgressBar();
}
goog.exportSymbol("psRemoveProgressBar", psRemoveProgressBar);
})();
