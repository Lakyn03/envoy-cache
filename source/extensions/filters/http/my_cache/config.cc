#include "source/extensions/filters/http/my_cache/config.h"
#include "source/extensions/filters/http/my_cache/my_cache_filter.h"
#include "source/extensions/filters/http/my_cache/my_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// register my_cache singleton
SINGLETON_MANAGER_REGISTRATION(my_cache_singleton);

// filter factory, injects each filter with reference to the global cache
Http::FilterFactoryCb MyCacheFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::my_cache::v3::MyCacheConfig&, const std::string&,
    Server::Configuration::FactoryContext& context) {
    // get instance from the singletonManager, call my creatinon function
  std::shared_ptr<MyCache> cache = context.serverFactoryContext().singletonManager().getTyped<MyCache>(
                                    SINGLETON_MANAGER_REGISTERED_NAME(my_cache_singleton), &createMyCache); 
  // returns a function generating filter instances
  return [cache](Http::FilterChainFactoryCallbacks& callbacks) -> void { 
    callbacks.addStreamFilter(std::make_shared<MyCacheFilter>(cache));
  };
}

REGISTER_FACTORY(MyCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
