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
//#include "source/extensions/filters/http/my_cache/my_cache_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// helper class for storing the response
// TODO: later add some timestamp when the data go invalid
class Response {
public:
    Response() = default;
    Response(Http::ResponseHeaderMapPtr headers, std::string body);

    //copy constructor
    Response(const Response& other)
        : headers_(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*other.headers_)),
          body_(other.body_) {}
    // = const      
    Response& operator=(const Response& other);

    const Http::ResponseHeaderMap& getHeaders() const { return *headers_; }
    const std::string& getBody() const { return body_; }

private:
    Http::ResponseHeaderMapPtr headers_;
    std::string body_;
};


// helper class for storing one RingBuffer
// each one stores requests to one host
class RingBuffer {
public:
    RingBuffer(int capacity);

    
    std::optional<Response> getFromBuffer(const std::string& path);
    void storeToBuffer(std::string path, Response response);

private:
    int capacity_; // default value if nothing is provided
    int curSize_ = 0;
    int startId_ = 0;
    std::vector<Response> buffer_ ABSL_GUARDED_BY(BufMutex_);
    std::unordered_map<std::string, int> pathToId_ ABSL_GUARDED_BY(BufMutex_);
    absl::Mutex BufMutex_;
};


class MyCache : public Singleton::Instance {
public:
  MyCache(int buffer_size) : buffer_size_(buffer_size) {}

  // returns the cached response, or nullptr if id is not a stored key
  std::optional<Response> getFromCache(std::string host, std::string path);

  // saves response into the cache structure
  void storeToCache(std::string host, std::string path, Response response);

private:
  int buffer_size_;
  std::unordered_map<std::string, std::unique_ptr<RingBuffer>> hostToBuffer_ ABSL_GUARDED_BY(mutex_);
  absl::Mutex mutex_;
};

// cache singleton class creation
std::shared_ptr<Singleton::Instance> createMyCache(int size);

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
