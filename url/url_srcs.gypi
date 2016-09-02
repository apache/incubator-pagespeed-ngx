# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

{
  'variables': {
    'gurl_sources': [
      '<(chromium_root)/googleurl/src/gurl.cc',
      '<(chromium_root)/googleurl/src/gurl.h',
      '<(chromium_root)/googleurl/src/url_parse.cc',
      '<(chromium_root)/googleurl/src/url_parse.h',
      '<(chromium_root)/googleurl/src/url_canon.h',
      '<(chromium_root)/googleurl/src/url_canon_etc.cc',
      '<(chromium_root)/googleurl/src/url_canon_filesystemurl.cc',
      '<(chromium_root)/googleurl/src/url_canon_fileurl.cc',
      '<(chromium_root)/googleurl/src/url_canon_host.cc',
      './url_canon_icu.cc',
      '<(chromium_root)/googleurl/src/url_canon_icu.h',
      '<(chromium_root)/googleurl/src/url_canon_internal.cc',
      '<(chromium_root)/googleurl/src/url_canon_internal.h',
      '<(chromium_root)/googleurl/src/url_canon_internal_file.h',
      '<(chromium_root)/googleurl/src/url_canon_ip.cc',
      '<(chromium_root)/googleurl/src/url_canon_ip.h',
      '<(chromium_root)/googleurl/src/url_canon_mailtourl.cc',
      '<(chromium_root)/googleurl/src/url_canon_path.cc',
      '<(chromium_root)/googleurl/src/url_canon_pathurl.cc',
      '<(chromium_root)/googleurl/src/url_canon_query.cc',
      '<(chromium_root)/googleurl/src/url_canon_relative.cc',
      '<(chromium_root)/googleurl/src/url_canon_stdstring.h',
      '<(chromium_root)/googleurl/src/url_canon_stdurl.cc',
      '<(chromium_root)/googleurl/src/url_file.h',
      '<(chromium_root)/googleurl/src/url_parse_file.cc',
      '<(chromium_root)/googleurl/src/url_parse_internal.h',
      '<(chromium_root)/googleurl/src/url_util.cc',
      '<(chromium_root)/googleurl/src/url_util.h',
    ],
  },
}
