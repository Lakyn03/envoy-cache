#include "source/extensions/filters/http/my_cache/my_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {



Response::Response(Http::ResponseHeaderMapPtr headers, std::string body) : headers_(std::move(headers)), body_(body) {}



Response* MyCache::getCached(std::string id) {
    auto it = storage_.find(id);
    if (it != storage_.end()) {
        return &it->second;
    }

    return nullptr;
}

void MyCache::storeResponse(std::string key, Response response) {
    storage_.emplace(std::move(key), std::move(response));
}

std::shared_ptr<Singleton::Instance> createMyCache() { return std::make_shared<MyCache>(); }

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
