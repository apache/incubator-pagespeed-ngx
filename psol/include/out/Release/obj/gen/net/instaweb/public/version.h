// Copyright (c) 2010 The Chromium Authors.  All rights reserved.
// Use of this source is governed by a BSD-style license that can be
// found in the LICENSE file.

// version.h is generated from version.h.in.  Edit the source!

#pragma once

// Version Information

#define MOD_PAGESPEED_VERSION 1,4,0,0
#define MOD_PAGESPEED_VERSION_STRING "1.4.0.0"

// Branding Information

#define COMPANY_FULLNAME_STRING "@COMPANY_FULLNAME@"
#define COMPANY_SHORTNAME_STRING "@COMPANY_SHORTNAME@"
#define PRODUCT_FULLNAME_STRING "@PRODUCT_FULLNAME@"
#define PRODUCT_SHORTNAME_STRING "@PRODUCT_SHORTNAME@"
#define COPYRIGHT_STRING "@COPYRIGHT@"
#define OFFICIAL_BUILD_STRING "0"

// Changelist Information

#define LASTCHANGE_STRING "2748"

namespace net_instaweb {

// TODO(jmarantz): determine the version-number from SVN at build time.
const char kModPagespeedVersion[] =
    MOD_PAGESPEED_VERSION_STRING "-" LASTCHANGE_STRING;

}
