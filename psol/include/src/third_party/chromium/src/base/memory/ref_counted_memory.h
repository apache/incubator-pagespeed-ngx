// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_REF_COUNTED_MEMORY_H_
#define BASE_MEMORY_REF_COUNTED_MEMORY_H_
#pragma once

#include <vector>

#include "base/base_api.h"
#include "base/memory/ref_counted.h"

// TODO(erg): The contents of this file should be in a namespace. This would
// require touching >100 files in chrome/ though.

// A generic interface to memory. This object is reference counted because one
// of its two subclasses own the data they carry, and we need to have
// heterogeneous containers of these two types of memory.
class BASE_API RefCountedMemory
    : public base::RefCountedThreadSafe<RefCountedMemory> {
 public:
  // Retrieves a pointer to the beginning of the data we point to. If the data
  // is empty, this will return NULL.
  virtual const unsigned char* front() const = 0;

  // Size of the memory pointed to.
  virtual size_t size() const = 0;

 protected:
  friend class base::RefCountedThreadSafe<RefCountedMemory>;
  RefCountedMemory();
  virtual ~RefCountedMemory();
};

// An implementation of RefCountedMemory, where the ref counting does not
// matter.
class BASE_API RefCountedStaticMemory : public RefCountedMemory {
 public:
  RefCountedStaticMemory()
      : data_(NULL), length_(0) {}
  RefCountedStaticMemory(const unsigned char* data, size_t length)
      : data_(data), length_(length) {}

  // Overriden from RefCountedMemory:
  virtual const unsigned char* front() const;
  virtual size_t size() const;

 private:
  const unsigned char* data_;
  size_t length_;

  DISALLOW_COPY_AND_ASSIGN(RefCountedStaticMemory);
};

// An implementation of RefCountedMemory, where we own our the data in a
// vector.
class BASE_API RefCountedBytes : public RefCountedMemory {
 public:
  RefCountedBytes();

  // Constructs a RefCountedBytes object by _copying_ from |initializer|.
  RefCountedBytes(const std::vector<unsigned char>& initializer);

  // Constructs a RefCountedBytes object by performing a swap. (To non
  // destructively build a RefCountedBytes, use the constructor that takes a
  // vector.)
  static RefCountedBytes* TakeVector(std::vector<unsigned char>* to_destroy);

  // Overriden from RefCountedMemory:
  virtual const unsigned char* front() const;
  virtual size_t size() const;

  std::vector<unsigned char> data;

 protected:
  friend class base::RefCountedThreadSafe<RefCountedBytes>;
  virtual ~RefCountedBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(RefCountedBytes);
};

#endif  // BASE_MEMORY_REF_COUNTED_MEMORY_H_
