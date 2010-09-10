// Copyright 2010 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "net/instaweb/apache/apr_file_system.h"

#include <string>

#include "apr_file_info.h"
#include "apr_file_io.h"
#include "apr_pools.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "net/instaweb/util/public/message_handler.h"

using net_instaweb::MessageHandler;

namespace {
const int kErrorMessageBufferSize = 1024;

void AprReportError(MessageHandler* message_handler, const char* filename,
                    const char* message, int error_code) {
  char buf[kErrorMessageBufferSize];
  apr_strerror(error_code, buf, sizeof(buf));
  std::string error_format(message);
  error_format.append(" (code=%d %s)");
  message_handler->Error(filename, 0, error_format.c_str(), error_code, buf);
}

}  // namespace

namespace html_rewriter {

// Helper class to factor out common implementation details between Input and
// Output files, in lieu of multiple inheritance.
class FileHelper {
 public:
  FileHelper(apr_file_t* file, const char* filename)
      : file_(file),
        filename_(filename) {
  }

  void ReportError(MessageHandler* message_handler, const char* format,
                   int error_code) {
    AprReportError(message_handler, filename_.c_str(), format, error_code);
  }

  bool Close(MessageHandler* message_handler);
  apr_file_t* file() { return file_; }
  const std::string& filename() const { return filename_; }

 private:
  apr_file_t* const file_;
  const std::string filename_;
};

bool FileHelper::Close(MessageHandler* message_handler) {
  apr_status_t ret = apr_file_close(file_);
  if (ret != APR_SUCCESS) {
    ReportError(message_handler, "close file", ret);
    return false;
  } else {
    return true;
  }
}

class HtmlWriterInputFile : public FileSystem::InputFile {
 public:
  HtmlWriterInputFile(apr_file_t* file, const char* filename);
  virtual int Read(char* buf, int size, MessageHandler* message_handler);
  virtual bool Close(MessageHandler* message_handler) {
    return helper_.Close(message_handler);
  }
  virtual const char* filename() { return helper_.filename().c_str(); }
 private:
  FileHelper helper_;
};

class HtmlWriterOutputFile : public FileSystem::OutputFile {
 public:
  HtmlWriterOutputFile(apr_file_t* file, const char* filename);
  virtual bool Write(const net_instaweb::StringPiece& buf,
                     MessageHandler* message_handler);
  virtual bool Flush(MessageHandler* message_handler);
  virtual bool Close(MessageHandler* message_handler) {
    return helper_.Close(message_handler);
  }
  virtual bool SetWorldReadable(MessageHandler* message_handler);
  virtual const char* filename() { return helper_.filename().c_str(); }
 private:
  FileHelper helper_;
};

HtmlWriterInputFile::HtmlWriterInputFile(apr_file_t* file, const char* filename)
    : helper_(file, filename) {
}

int HtmlWriterInputFile::Read(char* buf,
                              int size,
                              MessageHandler* message_handler) {
  apr_size_t bytes = size;
  apr_status_t ret = apr_file_read(helper_.file(), buf, &bytes);
  if (ret == APR_EOF) {
    return 0;
  }
  if (ret != APR_SUCCESS) {
    bytes = 0;
    helper_.ReportError(message_handler, "read file", ret);
  }
  return bytes;
}

HtmlWriterOutputFile::HtmlWriterOutputFile(apr_file_t* file,
                                           const char* filename)
    : helper_(file, filename) {
}

bool HtmlWriterOutputFile::Write(const net_instaweb::StringPiece& buf,
                                 MessageHandler* message_handler) {
  bool success = false;
  apr_size_t bytes = buf.size();
  apr_status_t ret = apr_file_write(helper_.file(), buf.data(), &bytes);
  if (ret != APR_SUCCESS) {
    helper_.ReportError(message_handler, "write file", ret);
  } else if (bytes != buf.size()) {
    helper_.ReportError(message_handler, "write file partial", ret);
  } else {
    success = true;
  }
  return success;
}

bool HtmlWriterOutputFile::Flush(MessageHandler* message_handler) {
  apr_status_t ret = apr_file_flush(helper_.file());
  if (ret != APR_SUCCESS) {
    helper_.ReportError(message_handler, "flush file", ret);
    return false;
  }
  return true;
}

bool HtmlWriterOutputFile::SetWorldReadable(MessageHandler* message_handler) {
  apr_status_t ret = apr_file_perms_set(helper_.filename().c_str(),
                                        APR_FPROT_UREAD | APR_FPROT_UWRITE |
                                        APR_FPROT_GREAD | APR_FPROT_WREAD);
  if (ret != APR_SUCCESS) {
    helper_.ReportError(message_handler, "set permission", ret);
    return false;
  }
  return true;
}

AprFileSystem::AprFileSystem(apr_pool_t* pool)
    : pool_(NULL) {
  apr_pool_create(&pool_, pool);
}

AprFileSystem::~AprFileSystem() {
  apr_pool_destroy(pool_);
}

FileSystem::InputFile* AprFileSystem::OpenInputFile(
    const char* filename, MessageHandler* message_handler) {
  apr_file_t* file;
  apr_status_t ret = apr_file_open(&file, filename, APR_FOPEN_READ,
                                   APR_OS_DEFAULT, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(message_handler, filename, "open input file",
                   ret);
    return NULL;
  }
  return new HtmlWriterInputFile(file, filename);
}

FileSystem::OutputFile* AprFileSystem::OpenOutputFileHelper(
    const char* filename, MessageHandler* message_handler) {
  apr_file_t* file;
  apr_status_t ret = apr_file_open(&file, filename,
                                   APR_WRITE | APR_CREATE | APR_TRUNCATE,
                                   APR_OS_DEFAULT, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(message_handler, filename, "open output file",
                   ret);
    return NULL;
  }
  return new HtmlWriterOutputFile(file, filename);
}

FileSystem::OutputFile* AprFileSystem::OpenTempFileHelper(
    const net_instaweb::StringPiece& prefix_name,
    MessageHandler* message_handler) {
  static const char mkstemp_hook[] = "XXXXXX";
  scoped_array<char> template_name(
      new char[prefix_name.size() + sizeof(mkstemp_hook)]);
  memcpy(template_name.get(), prefix_name.data(), prefix_name.size());
  memcpy(template_name.get() + prefix_name.size(), mkstemp_hook,
         sizeof(mkstemp_hook));

  apr_file_t* file;
  // A temp file will be generated with the XXXXXX part of template_name being
  // replaced.
  // Do not use flag APR_DELONCLOSE to delete the temp file on close, it will
  // be renamed for later use.
  apr_status_t ret = apr_file_mktemp(
      &file, template_name.get(),
      APR_CREATE | APR_READ | APR_WRITE | APR_EXCL, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(message_handler, template_name.get(),
                   "open temp file", ret);
    return NULL;
  }
  return new HtmlWriterOutputFile(file, template_name.get());
}

bool AprFileSystem::RenameFileHelper(
    const char* old_filename, const char* new_filename,
    MessageHandler* message_handler) {
  apr_status_t ret = apr_file_rename(old_filename, new_filename, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(message_handler, new_filename,
                   "renaming temp file", ret);
    return false;
  }
  return true;
}
bool AprFileSystem::RemoveFile(const char* filename,
                               MessageHandler* message_handler) {
  apr_status_t ret = apr_file_remove(filename, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(message_handler, filename,
                   "removing file", ret);
    return false;
  }
  return true;
}

bool AprFileSystem::MakeDir(const char* directory_path,
                            MessageHandler* handler) {
  apr_status_t ret = apr_dir_make(directory_path, APR_FPROT_OS_DEFAULT, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(handler, directory_path, "creating dir", ret);
    return false;
  }
  return true;
}

BoolOrError AprFileSystem::Exists(const char* path, MessageHandler* handler) {
  BoolOrError exists;  // Error is the default state.
  apr_int32_t wanted = APR_FINFO_TYPE;
  apr_finfo_t finfo;
  apr_status_t ret = apr_stat(&finfo, path, wanted, pool_);
  if (ret != APR_SUCCESS && ret != APR_ENOENT) {
      AprReportError(handler, path, "failed to stat", ret);
      exists.set_error();
  } else {
    exists.set(ret == APR_SUCCESS);
  }
  return exists;
}

BoolOrError AprFileSystem::IsDir(const char* path, MessageHandler* handler) {
  BoolOrError is_dir;  // Error is the default state.
  apr_int32_t wanted = APR_FINFO_TYPE;
  apr_finfo_t finfo;
  apr_status_t ret = apr_stat(&finfo, path, wanted, pool_);
  if (ret != APR_SUCCESS && ret != APR_ENOENT) {
      AprReportError(handler, path, "failed to stat", ret);
      is_dir.set_error();
  } else {
    is_dir.set(ret == APR_SUCCESS && finfo.filetype == APR_DIR);
  }
  return is_dir;
}

bool AprFileSystem::ListContents(const net_instaweb::StringPiece& dir,
                                 net_instaweb::StringVector* files,
                                 MessageHandler* handler) {
  std::string dirString = dir.as_string();
  net_instaweb::EnsureEndsInSlash(&dirString);
  const char* dirname = dirString.c_str();
  apr_dir_t* mydir;
  apr_status_t ret = apr_dir_open(&mydir, dirname, pool_);
  if (ret != APR_SUCCESS) {
      AprReportError(handler, dirname, "failed to opendir", ret);
      return false;
  } else {
    apr_finfo_t finfo;
    apr_int32_t wanted = APR_FINFO_NAME;
    while (apr_dir_read(&finfo, wanted, mydir) != APR_ENOENT) {
      if ((strcmp(finfo.name, ".") != 0) &&
          (strcmp(finfo.name, "..") != 0)) {
        files->push_back(dirString + finfo.name);
      }
    }
  }
  ret = apr_dir_close(mydir);
  if (ret != APR_SUCCESS) {
      AprReportError(handler, dirname, "failed to closedir", ret);
      return false;
  }
  return true;
}

bool AprFileSystem::Atime(const net_instaweb::StringPiece& path,
                            int64* timestamp_sec, MessageHandler* handler) {
  // TODO(abliss): there are some situations where this doesn't work
  // -- e.g. if the filesystem is mounted noatime.
  const std::string path_string = path.as_string();
  const char* path_str = path_string.c_str();
  apr_int32_t wanted = APR_FINFO_ATIME;
  apr_finfo_t finfo;
  apr_status_t ret = apr_stat(&finfo, path_str, wanted, pool_);
  if (ret != APR_SUCCESS) {
      AprReportError(handler, path_str, "failed to stat", ret);
      return false;
  } else {
    *timestamp_sec = finfo.atime;
    return true;
  }
}

bool AprFileSystem::Size(const net_instaweb::StringPiece& path, int64* size,
                           MessageHandler* handler) {
  const std::string path_string = path.as_string();
  const char* path_str = path_string.c_str();
  apr_int32_t wanted = APR_FINFO_SIZE;
  apr_finfo_t finfo;
  apr_status_t ret = apr_stat(&finfo, path_str, wanted, pool_);
  if (ret != APR_SUCCESS) {
    AprReportError(handler, path_str, "failed to stat", ret);
    return false;
  } else {
    *size = finfo.size;
    return true;
  }
}

}  // namespace html_rewriter
