#include "source/extensions/filters/http/my_cache/my_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

std::string MyCache::getMsg() {
  int current = count_.fetch_add(1);
  return std::to_string(current);
}

std::shared_ptr<Singleton::Instance> createMyCache() { return std::make_shared<MyCache>(); }

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
