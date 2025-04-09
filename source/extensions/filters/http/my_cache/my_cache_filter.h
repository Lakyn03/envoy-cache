#pragma once

#include <string>
#include "source/extensions/filters/http/my_cache/my_cache.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

class MyCacheFilter : public Http::PassThroughFilter {
public:
  MyCacheFilter(std::shared_ptr<MyCache> cache) : cache_(cache) {}
  ~MyCacheFilter() override = default;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers,
                                          bool end_stream) override;

private:
  std::shared_ptr<MyCache> cache_;
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy