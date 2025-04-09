#include "source/extensions/filters/http/my_cache/config.h"
#include "source/extensions/filters/http/my_cache/my_cache_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {
    

Http::FilterFactoryCb MyCacheFilterFactory::createFilterFactoryFromProtoTyped(
    const envoy::extensions::filters::http::my_cache::v3::MyCacheConfig& proto_config,
    const std::string&,
    Server::Configuration::FactoryContext&) {
  const std::string& msg = proto_config.message();
  return [msg](Http::FilterChainFactoryCallbacks& callbacks) -> void {
    callbacks.addStreamFilter(std::make_shared<MyCacheFilter>(msg));
  };
}

REGISTER_FACTORY(MyCacheFilterFactory, Server::Configuration::NamedHttpFilterConfigFactory);

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
