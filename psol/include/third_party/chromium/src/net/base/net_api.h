// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_NET_API_H_
#define NET_BASE_NET_API_H_
#pragma once

// Defines NET_API so that funtionality implemented by the net module can be
// exported to consumers, and NET_TEST that allows unit tests to access features
// not intended to be used directly by real consumers.

#if defined(WIN32) && defined(NET_DLL)
#if defined(NET_IMPLEMENTATION)
#define NET_API __declspec(dllexport)
#define NET_TEST __declspec(dllexport)
#else
#define NET_API __declspec(dllimport)
#define NET_TEST __declspec(dllimport)
#endif  // defined(NET_IMPLEMENTATION)
#else
#define NET_API
#define NET_TEST
#endif

#endif  // NET_BASE_NET_API_H_
