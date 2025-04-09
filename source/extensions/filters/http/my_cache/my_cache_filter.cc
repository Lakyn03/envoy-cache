#include "source/extensions/filters/http/my_cache/my_cache_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

MyCacheFilter::MyCacheFilter(const std::string msg) : msg_(msg) {}

Http::FilterHeadersStatus MyCacheFilter::encodeHeaders(Http::ResponseHeaderMap& headers, bool) {
    headers.addCopy(Http::LowerCaseString("my-cache-filter"), msg_);
    return Http::FilterHeadersStatus::Continue;
}


}
}
}
}