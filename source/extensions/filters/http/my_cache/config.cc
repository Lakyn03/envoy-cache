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
    const envoy::extensions::filters::http::my_cache::v3::MyCacheConfig& proto_config, const std::string&,
    Server::Configuration::FactoryContext& context) {
  
  // load from config
  int size = proto_config.buffer_size();
  
  // change default value if not specified
  if(size == 0) {
    size = 100;
  }

  // safety bounds for each buffer size
  if(size < 1 || size > 1000000) {
    throw Envoy::ProtoValidationException(fmt::format("MyCacheConfig: message_number must be between 1 and 1000000, got {}", size));
  }


  // get instance from the singletonManager, call my creatinon function, need to make a closure for the size parameter as getTyped needs a func with no parameters
  std::shared_ptr<MyCache> cache = context.serverFactoryContext().singletonManager().getTyped<MyCache>(
    SINGLETON_MANAGER_REGISTERED_NAME(my_cache_singleton),
    static_cast<Singleton::SingletonFactoryCb>(
      [size]() -> std::shared_ptr<Singleton::Instance> {
        return std::make_shared<MyCache>(size);
      }
    )
  );
  
  
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
