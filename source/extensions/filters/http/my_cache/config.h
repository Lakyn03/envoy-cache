#pragma once

#include "envoy/extensions/filters/http/my_cache/v3/my_cache.pb.h"
#include "envoy/extensions/filters/http/my_cache/v3/my_cache.pb.validate.h"
#include "envoy/server/filter_config.h"
#include "envoy/singleton/manager.h"
#include "source/extensions/filters/http/common/factory_base.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// factory class needed to create MyCacheFilter instances by the filter manager
class MyCacheFilterFactory : public Envoy::Extensions::HttpFilters::Common::FactoryBase<
                                 envoy::extensions::filters::http::my_cache::v3::MyCacheConfig> {
public:
  MyCacheFilterFactory() : FactoryBase("envoy.filters.http.my_cache_filter") {}

  Http::FilterFactoryCb createFilterFactoryFromProtoTyped(
      const envoy::extensions::filters::http::my_cache::v3::MyCacheConfig& proto_config,
      const std::string&, Server::Configuration::FactoryContext&) override;
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
