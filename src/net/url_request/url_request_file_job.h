// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
#define NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "net/base/net_errors.h"
#include "net/base/net_export.h"
#include "net/http/http_byte_range.h"
#include "net/url_request/url_request.h"
#include "net/url_request/url_request_job.h"

namespace base {
class TaskRunner;
}

namespace net {

class FileStream;

// A request job that handles reading file URLs
class NET_EXPORT URLRequestFileJob : public URLRequestJob {
 public:
  URLRequestFileJob(URLRequest* request,
                    NetworkDelegate* network_delegate,
                    const base::FilePath& file_path,
                    const scoped_refptr<base::TaskRunner>& file_task_runner);

  // URLRequestJob:
  void Start() override;
  void Kill() override;
  int ReadRawData(IOBuffer* buf, int buf_size) override;
  bool IsRedirectResponse(GURL* location,
                          int* http_status_code,
                          bool* insecure_scheme_was_upgraded) override;
  bool GetMimeType(std::string* mime_type) const override;
  void SetExtraRequestHeaders(const HttpRequestHeaders& headers) override;
  void ShouldServeMimeTypeAsContentTypeHeader() {
    serve_mime_type_as_content_type_ = true;
  };
  void GetResponseInfo(HttpResponseInfo* info) override;

  // base::PowerObserver:
  void OnSuspend() override;

  // An interface for subclasses who wish to monitor read operations.
  //
  // |result| is the net::Error code resulting from attempting to open the file.
  // Called before OnSeekComplete, only called if the request advanced to the
  // point the file was opened, without being canceled.
  virtual void OnOpenComplete(int result);
  // Called at most once.  On success, |result| is the non-negative offset into
  // the file that the request will read from.  On seek failure, it's a negative
  // net:Error code.
  virtual void OnSeekComplete(int64_t result);
  // Called once per read attempt.  |buf| contains the read data, if any.
  // |result| is the number of read bytes.  0 (net::OK) indicates EOF, negative
  // numbers indicate it's a net::Error code.
  virtual void OnReadComplete(IOBuffer* buf, int result);

 protected:
  ~URLRequestFileJob() override;

  // URLRequestJob implementation.
  std::unique_ptr<SourceStream> SetUpSourceStream() override;

  int64_t remaining_bytes() const { return remaining_bytes_; }

  // The OS-specific full path name of the file
  base::FilePath file_path_;

 private:
  // This class checks if a path is accessible via file: scheme, with
  // NetworkDelegate. Subclasses can disable the check if needed.
  virtual bool CanAccessFile(const base::FilePath& original_path,
                             const base::FilePath& absolute_path);

  // Meta information about the file. It's used as a member in the
  // URLRequestFileJob and also passed between threads because disk access is
  // necessary to obtain it.
  struct FileMetaInfo {
    FileMetaInfo();

    // Size of the file.
    int64_t file_size;
    // Mime type associated with the file.
    std::string mime_type;
    // Result returned from GetMimeTypeFromFile(), i.e. flag showing whether
    // obtaining of the mime type was successful.
    bool mime_type_result;
    // Flag showing whether the file exists.
    bool file_exists;
    // Flag showing whether the file name actually refers to a directory.
    bool is_directory;
    // Absolute path of the file (i.e. symbolic link is resolved).
    base::FilePath absolute_path;
#if defined(USE_FILESCHEME_CODECACHE)
    // The last modified time of a file
    base::Time last_modified;
#endif
  };

  // Fetches file info on a background thread.
  static void FetchMetaInfo(const base::FilePath& file_path,
                            FileMetaInfo* meta_info);

  // Callback after fetching file info on a background thread.
  void DidFetchMetaInfo(const FileMetaInfo* meta_info);

  // Callback after opening file on a background thread.
  void DidOpen(int result);

  // Callback after seeking to the beginning of |byte_range_| in the file
  // on a background thread.
  void DidSeek(int64_t result);

  // Callback after data is asynchronously read from the file into |buf|.
  void DidRead(scoped_refptr<IOBuffer> buf, int result);

  std::unique_ptr<FileStream> stream_;
  FileMetaInfo meta_info_;
  const scoped_refptr<base::TaskRunner> file_task_runner_;

  std::vector<HttpByteRange> byte_ranges_;
  HttpByteRange byte_range_;
  int64_t remaining_bytes_;
  bool serve_mime_type_as_content_type_ = false;

  Error range_parse_result_;

  base::WeakPtrFactory<URLRequestFileJob> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(URLRequestFileJob);
};

}  // namespace net

#endif  // NET_URL_REQUEST_URL_REQUEST_FILE_JOB_H_
