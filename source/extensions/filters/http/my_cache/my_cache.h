#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <vector>
#include <unordered_map>
#include "envoy/http/header_map.h"
#include "envoy/singleton/instance.h"
#include "absl/synchronization/mutex.h"
#include "absl/base/thread_annotations.h"
#include "source/common/http/header_map_impl.h"
#include "envoy/buffer/buffer.h"
#include "source/common/buffer/buffer_impl.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {
// needed because of circular includes with my_cache_filter
class MyCacheFilter;


// class for storing a response (headers, body)
class Response {
public:
  Response() = default;
  Response(Http::ResponseHeaderMapPtr headers, const Buffer::Instance& body);
  // copy constructor
  Response(const Response& other)
      : headers_(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*other.headers_)) {
          body_.add(other.body_);
        }
  // = const
  Response& operator=(const Response& other);

  // returns the headers for this response
  // we cannot return pointer, because ResponseHeaderMapPtr is a unique ptr
  const Http::ResponseHeaderMap& getHeaders() const { return *headers_; }
  const Buffer::Instance& getBody() const { return body_; }

private:
  Http::ResponseHeaderMapPtr headers_;
  Buffer::OwnedImpl body_;
};

// class for storing one RingBuffer storing all cached requests to one host
class RingBuffer {
public:
  RingBuffer(int capacity);

  std::optional<Response> getFromBuffer(const std::string& path);
  void storeToBuffer(const std::string& path, Response response);

private:
  int capacity_;
  int curSize_ = 0;
  int startId_ = 0;
  std::vector<Response> buffer_ ABSL_GUARDED_BY(BufMutex_);
  // for O(1) getFromBuffer
  std::unordered_map<std::string, int> pathToId_ ABSL_GUARDED_BY(BufMutex_);
  // for O(1) storToBuffer even when the buffer is full
  std::unordered_map<int, std::string> idToPath_ ABSL_GUARDED_BY(BufMutex_);
  absl::Mutex BufMutex_;
};

// class for storing all coalesced requests waiting for the same response
// manages each filter's offset, makes sure to send headers exactly once
class Coalescing {
public:
  Coalescing() = default;

  void setHeaders(Http::ResponseHeaderMapPtr headers);
  void addData(const Buffer::Instance& data);
  // sends new data to each filter according to what they already received
  void send(bool end_stream);
  void addFilter(std::shared_ptr<MyCacheFilter> filter);

  // stores info about one waiting filter
  struct FilterInfo {
    std::shared_ptr<MyCacheFilter> filter;
    uint64_t offset = 0;
    bool headersSent = false;
  };

private:
  Http::ResponseHeaderMapPtr headers_ ABSL_GUARDED_BY(mutex_);
  Buffer::OwnedImpl buffer_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::shared_ptr<FilterInfo>> filters_ ABSL_GUARDED_BY(mutex_);
  absl::Mutex mutex_;
};

class MyCache : public Singleton::Instance {
public:
  MyCache(int buffer_size) : buffer_size_(buffer_size) {}

  // returns the cached response, or nullptr if id is not a stored key
  std::optional<Response> getFromCache(const std::string& host, const std::string& path);
  // saves response into the cache structure
  void storeToCache(const std::string& host, const std::string& path, Response response);
  // used for coalescing, checks whether this is the first time this request was made or whether others are already waiting
  std::shared_ptr<Coalescing> checkRequest(const std::string& key, std::shared_ptr<MyCacheFilter> filter);
  void deleteCoal(const std::string& key);

private:
  int buffer_size_;
  std::unordered_map<std::string, std::unique_ptr<RingBuffer>>hostToBuffer_ ABSL_GUARDED_BY(hostToBufferMutex_);
  absl::Mutex hostToBufferMutex_;
  std::unordered_map<std::string, std::shared_ptr<Coalescing>> coalescing_map_ ABSL_GUARDED_BY(coalescingMutex_);
  absl::Mutex coalescingMutex_;
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
