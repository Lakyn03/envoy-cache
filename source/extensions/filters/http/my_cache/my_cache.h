#pragma once

#include <memory>
#include <atomic>
#include <string>
#include <unordered_map>
#include "envoy/http/header_map.h"
#include "envoy/singleton/instance.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// helper class for storing the response
// TODO: later add some timestamp when the data go invalid
class Response {
public:
    Response(Http::ResponseHeaderMapPtr headers, std::string body);

    const Http::ResponseHeaderMap& getHeaders() const { return *headers_; }
    const std::string& getBody() const { return body_; }

private:
    Http::ResponseHeaderMapPtr headers_;
    std::string body_;
};


class MyCache : public Singleton::Instance {
public:
  MyCache() = default;

  // returns the cached response, or nullptr if id is not a stored key
  Response* getCached(std::string id);

  // saves response into the cache structure
  void storeResponse(std::string key, Response response);

private:
  std::unordered_map<std::string, Response> storage_;
};

// cache singleton class creation
std::shared_ptr<Singleton::Instance> createMyCache();

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
