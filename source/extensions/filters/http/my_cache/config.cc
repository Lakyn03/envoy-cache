#include "source/extensions/filters/http/my_cache/config.h"
#include "source/extensions/filters/http/my_cache/my_cache_filter.h"
#include "source/extensions/filters/http/my_cache/my_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

#define DEFAULT_BUFFER_SIZE 100

// register my_cache singleton
SINGLETON_MANAGER_REGISTRATION(my_cache_singleton);

// filter factory, injects each filter with a reference to the global cache
Http::FilterFactoryCb MyCacheFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::my_cache::v3::MyCacheConfig& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  
  // load from config
  int size = proto_config.buffer_size();
  if(size == 0) {
    size = DEFAULT_BUFFER_SIZE;
  }
  // safety bounds for each buffer size
  if(size < 1 || size > 1000000) {
    throw Envoy::ProtoValidationException(fmt::format("MyCacheConfig: buffer_size must be between 1 and 1000000, got {}", size));
  }
 
  // get cache instance from the singletonManager, need to make a closure for the size parameter as getTyped needs a func with no parameters
  std::shared_ptr<MyCache> cache = context.serverFactoryContext().singletonManager().getTyped<MyCache>(
    SINGLETON_MANAGER_REGISTERED_NAME(my_cache_singleton),
    static_cast<Singleton::SingletonFactoryCb>(
      [size]() -> std::shared_ptr<Singleton::Instance> {
        return std::make_shared<MyCache>(size);
      }
    )
  );
  
  // returns a function generating filter instances and adding them to the filter chain
  return [cache](Http::FilterChainFactoryCallbacks& callbacks) -> void { 
    callbacks.addStreamFilter(std::make_shared<MyCacheFilter>(cache));
  };
}

REGISTER_FACTORY(MyCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
