/*
 * Copyright 2010 Google Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Author: jmarantz@google.com (Joshua Marantz)

#include "net/instaweb/util/public/stdio_file_system.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/errno.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "net/instaweb/util/public/message_handler.h"
#include "net/instaweb/util/public/string.h"

namespace net_instaweb {

// Helper class to factor out common implementation details between Input and
// Output files, in lieu of multiple inheritance.
class StdioFileHelper {
 public:
  StdioFileHelper(FILE* f, const StringPiece& filename)
      : file_(f),
        line_(1) {
    filename.CopyToString(&filename_);
  }

  ~StdioFileHelper() {
    CHECK(file_ == NULL);
  }

  void CountNewlines(const char* buf, int size) {
    for (int i = 0; i < size; ++i, ++buf) {
      line_ += (*buf == '\n');
    }
  }

  void ReportError(MessageHandler* message_handler, const char* format) {
    message_handler->Error(filename_.c_str(), line_, format, strerror(errno));
  }

  bool Close(MessageHandler* message_handler) {
    bool ret = true;
    if (file_ != stdout && file_ != stderr && file_ != stdin) {
      if (fclose(file_) != 0) {
        ReportError(message_handler, "closing file: %s");
        ret = false;
      }
    }
    file_ = NULL;
    return ret;
  }

  FILE* file_;
  GoogleString filename_;
  int line_;

 private:
  DISALLOW_COPY_AND_ASSIGN(StdioFileHelper);
};

class StdioInputFile : public FileSystem::InputFile {
 public:
  StdioInputFile(FILE* f, const StringPiece& filename)
      : file_helper_(f, filename) {
  }

  virtual int Read(char* buf, int size, MessageHandler* message_handler) {
    int ret = fread(buf, 1, size, file_helper_.file_);
    file_helper_.CountNewlines(buf, ret);
    if ((ret == 0) && (ferror(file_helper_.file_) != 0)) {
      file_helper_.ReportError(message_handler, "reading file: %s");
    }
    return ret;
  }

  virtual bool Close(MessageHandler* message_handler) {
    return file_helper_.Close(message_handler);
  }

  virtual const char* filename() { return file_helper_.filename_.c_str(); }

 private:
  StdioFileHelper file_helper_;

  DISALLOW_COPY_AND_ASSIGN(StdioInputFile);
};

class StdioOutputFile : public FileSystem::OutputFile {
 public:
  StdioOutputFile(FILE* f, const StringPiece& filename)
      : file_helper_(f, filename) {
  }

  virtual bool Write(const StringPiece& buf, MessageHandler* handler) {
    size_t bytes_written =
        fwrite(buf.data(), 1, buf.size(), file_helper_.file_);
    file_helper_.CountNewlines(buf.data(), bytes_written);
    bool ret = (bytes_written == buf.size());
    if (!ret) {
      file_helper_.ReportError(handler, "writing file: %s");
    }
    return ret;
  }

  virtual bool Flush(MessageHandler* message_handler) {
    bool ret = true;
    if (fflush(file_helper_.file_) != 0) {
      file_helper_.ReportError(message_handler, "flushing file: %s");
      ret = false;
    }
    return ret;
  }

  virtual bool Close(MessageHandler* message_handler) {
    return file_helper_.Close(message_handler);
  }

  virtual const char* filename() { return file_helper_.filename_.c_str(); }

  virtual bool SetWorldReadable(MessageHandler* message_handler) {
    bool ret = true;
    int fd = fileno(file_helper_.file_);
    int status = fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (status != 0) {
      ret = false;
      file_helper_.ReportError(message_handler, "setting world-readble: %s");
    }
    return ret;
  }

 private:
  StdioFileHelper file_helper_;

  DISALLOW_COPY_AND_ASSIGN(StdioOutputFile);
};

StdioFileSystem::~StdioFileSystem() {
}

FileSystem::InputFile* StdioFileSystem::OpenInputFile(
    const char* filename, MessageHandler* message_handler) {
  FileSystem::InputFile* input_file = NULL;
  FILE* f = fopen(filename, "r");
  if (f == NULL) {
    message_handler->Error(filename, 0, "opening input file: %s",
                           strerror(errno));
  } else {
    input_file = new StdioInputFile(f, filename);
  }
  return input_file;
}


FileSystem::OutputFile* StdioFileSystem::OpenOutputFileHelper(
    const char* filename, MessageHandler* message_handler) {
  FileSystem::OutputFile* output_file = NULL;
  if (strcmp(filename, "-") == 0) {
    output_file = new StdioOutputFile(stdout, "<stdout>");
  } else {
    FILE* f = fopen(filename, "w");
    if (f == NULL) {
      message_handler->Error(filename, 0,
                             "opening output file: %s", strerror(errno));
    } else {
      output_file = new StdioOutputFile(f, filename);
    }
  }
  return output_file;
}

FileSystem::OutputFile* StdioFileSystem::OpenTempFileHelper(
    const StringPiece& prefix, MessageHandler* message_handler) {
  // TODO(jmarantz): As jmaessen points out, mkstemp warns "Don't use
  // this function, use tmpfile(3) instead.  It is better defined and
  // more portable."  However, tmpfile does not allow a location to be
  // specified.  I'm not 100% sure if that's going to be work well for
  // us.  More importantly, our usage scenario is that we will be
  // closing the file and renaming it to a permanent name.  tmpfiles
  // automatically are deleted when they are closed.
  int prefix_len = prefix.length();
  static char mkstemp_hook[] = "XXXXXX";
  char* template_name = new char[prefix_len + sizeof(mkstemp_hook)];
  memcpy(template_name, prefix.data(), prefix_len);
  memcpy(template_name + prefix_len, mkstemp_hook, sizeof(mkstemp_hook));
  int fd = mkstemp(template_name);
  OutputFile* output_file = NULL;
  if (fd < 0) {
    message_handler->Error(template_name, 0,
                           "opening temp file: %s", strerror(errno));
  } else {
    FILE* f = fdopen(fd, "w");
    if (f == NULL) {
      close(fd);
      message_handler->Error(template_name, 0,
                             "re-opening temp file: %s", strerror(errno));
    } else {
      output_file = new StdioOutputFile(f, template_name);
    }
  }
  delete [] template_name;
  return output_file;
}


bool StdioFileSystem::RemoveFile(const char* filename,
                                 MessageHandler* handler) {
  bool ret = (remove(filename) == 0);
  if (!ret) {
    handler->Message(kError, "Failed to delete file %s: %s",
                     filename, strerror(errno));
  }
  return ret;
}

bool StdioFileSystem::RenameFileHelper(const char* old_file,
                                       const char* new_file,
                                       MessageHandler* handler) {
  bool ret = (rename(old_file, new_file) == 0);
  if (!ret) {
    handler->Message(kError, "Failed to rename file %s to %s: %s",
                     old_file, new_file, strerror(errno));
  }
  return ret;
}

bool StdioFileSystem::MakeDir(const char* path, MessageHandler* handler) {
  // Mode 0777 makes the file use standard umask permissions.
  bool ret = (mkdir(path, 0777) == 0);
  if (!ret) {
    handler->Message(kError, "Failed to make directory %s: %s",
                     path, strerror(errno));
  }
  return ret;
}

BoolOrError StdioFileSystem::Exists(const char* path, MessageHandler* handler) {
  struct stat statbuf;
  BoolOrError ret(stat(path, &statbuf) == 0);
  if (ret.is_false() && errno != ENOENT) {  // Not error if file doesn't exist.
    handler->Message(kError, "Failed to stat %s: %s",
                     path, strerror(errno));
    ret.set_error();
  }
  return ret;
}

BoolOrError StdioFileSystem::IsDir(const char* path, MessageHandler* handler) {
  struct stat statbuf;
  BoolOrError ret(false);
  if (stat(path, &statbuf) == 0) {
    ret.set(S_ISDIR(statbuf.st_mode));
  } else if (errno != ENOENT) {  // Not an error if file doesn't exist.
    handler->Message(kError, "Failed to stat %s: %s",
                     path, strerror(errno));
    ret.set_error();
  }
  return ret;
}

bool StdioFileSystem::ListContents(const StringPiece& dir, StringVector* files,
                                   MessageHandler* handler) {
  GoogleString dir_string = dir.as_string();
  EnsureEndsInSlash(&dir_string);
  const char* dirname = dir_string.c_str();
  DIR* mydir = opendir(dirname);
  if (mydir == NULL) {
      handler->Error(dirname, 0, "Failed to opendir: %s", strerror(errno));
    return false;
  } else {
    dirent* entry = NULL;
    dirent buffer;
    while (readdir_r(mydir, &buffer, &entry) == 0 && entry != NULL) {
      if ((strcmp(entry->d_name, ".") != 0) &&
          (strcmp(entry->d_name, "..") != 0)) {
        files->push_back(dir_string + entry->d_name);
      }
    }
    if (closedir(mydir) != 0) {
      handler->Error(dirname, 0, "Failed to closedir: %s", strerror(errno));
      return false;
    }
    return true;
  }
}

bool StdioFileSystem::Atime(const StringPiece& path, int64* timestamp_sec,
                            MessageHandler* handler) {
  // TODO(abliss): there are some situations where this doesn't work
  // -- e.g. if the filesystem is mounted noatime.  We should try to
  // detect that and provide a workaround.
  const GoogleString path_string = path.as_string();
  const char* path_str = path_string.c_str();
  struct stat statbuf;
  if (stat(path_str, &statbuf) == 0) {
    *timestamp_sec = statbuf.st_atime;
    return true;
  } else {
    handler->Message(kError, "Failed to stat %s: %s",
                     path_str, strerror(errno));
    return false;
  }
}

bool StdioFileSystem::Size(const StringPiece& path, int64* size,
                           MessageHandler* handler) {
  const GoogleString path_string = path.as_string();
  const char* path_str = path_string.c_str();
  struct stat statbuf;
  if (stat(path_str, &statbuf) == 0) {
    *size = statbuf.st_size;
    return true;
  } else {
    handler->Message(kError, "Failed to stat %s: %s",
                     path_str, strerror(errno));
    return false;
  }
}

BoolOrError StdioFileSystem::TryLock(const StringPiece& lock_name,
                                     MessageHandler* handler) {
  const GoogleString lock_string = lock_name.as_string();
  const char* lock_str = lock_string.c_str();
  // POSIX mkdir is widely believed to be atomic, although I have
  // found no reliable documentation of this fact.
  if (mkdir(lock_str, 0777) == 0) {
    return BoolOrError(true);
  } else if (errno == EEXIST) {
    return BoolOrError(false);
  } else {
    handler->Message(kError, "Failed to mkdir %s: %s",
                     lock_str, strerror(errno));
    return BoolOrError();
  }
}

bool StdioFileSystem::Unlock(const StringPiece& lock_name,
                             MessageHandler* handler) {
  const GoogleString lock_string = lock_name.as_string();
  const char* lock_str = lock_string.c_str();
  if (rmdir(lock_str) == 0) {
    return true;
  } else {
    handler->Message(kError, "Failed to rmdir %s: %s",
                     lock_str, strerror(errno));
    return false;
  }
}

FileSystem::InputFile* StdioFileSystem::Stdin() {
  return new StdioInputFile(stdin, "stdin");
}

FileSystem::OutputFile* StdioFileSystem::Stdout() {
  return new StdioOutputFile(stdout, "stdout");
}

FileSystem::OutputFile* StdioFileSystem::Stderr() {
  return new StdioOutputFile(stderr, "stderr");
}

}  // namespace net_instaweb
