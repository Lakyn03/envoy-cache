#pragma once

#include <string>
#include "source/extensions/filters/http/my_cache/my_cache.h"
#include "source/common/http/header_map_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

class MyCacheFilter : public Http::PassThroughFilter {
public:
  MyCacheFilter(std::shared_ptr<MyCache> cache);
  ~MyCacheFilter() override = default;

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) override;
  //Http::FilterDataStatus decodeData(Buffer::Instance& headers, bool end_stream) override; // no need as we can only cache GET requests

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& headers, bool end_stream) override;

private:
  std::shared_ptr<MyCache> cache_; // reference to cache
  std::string host_;
  std::string path_;
  bool responseFromCache_ = false; // whether the response is being served from cache or backend
  bool needsToBeCached_ = false;
  Http::ResponseHeaderMapPtr headers_; //headers of the response
  std::string buffer_; // buffer for collecting the body of the reponse in case it is being sent in more chunks
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy