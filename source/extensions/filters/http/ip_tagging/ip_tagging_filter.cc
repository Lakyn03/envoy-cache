#include "source/extensions/filters/http/ip_tagging/ip_tagging_filter.h"

#include "envoy/config/core/v3/address.pb.h"
#include "envoy/extensions/filters/http/ip_tagging/v3/ip_tagging.pb.h"

#include "source/common/http/header_map_impl.h"
#include "source/common/http/headers.h"

#include "absl/strings/str_join.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace IpTagging {

absl::StatusOr<IpTaggingFilterConfigSharedPtr> IpTaggingFilterConfig::create(
    const envoy::extensions::filters::http::ip_tagging::v3::IPTagging& config,
    const std::string& stat_prefix, Stats::Scope& scope, Runtime::Loader& runtime) {
  absl::Status creation_status = absl::OkStatus();
  auto config_ptr = std::shared_ptr<IpTaggingFilterConfig>(
      new IpTaggingFilterConfig(config, stat_prefix, scope, runtime, creation_status));
  RETURN_IF_NOT_OK(creation_status);
  return config_ptr;
}

IpTaggingFilterConfig::IpTaggingFilterConfig(
    const envoy::extensions::filters::http::ip_tagging::v3::IPTagging& config,
    const std::string& stat_prefix, Stats::Scope& scope, Runtime::Loader& runtime,
    absl::Status& creation_status)
    : request_type_(requestTypeEnum(config.request_type())), scope_(scope), runtime_(runtime),
      stat_name_set_(scope.symbolTable().makeSet("IpTagging")),
      stats_prefix_(stat_name_set_->add(stat_prefix + "ip_tagging")),
      no_hit_(stat_name_set_->add("no_hit")), total_(stat_name_set_->add("total")),
      unknown_tag_(stat_name_set_->add("unknown_tag.hit")),
      ip_tag_header_(config.has_ip_tag_header() ? config.ip_tag_header().header() : ""),
      ip_tag_header_action_(config.has_ip_tag_header()
                                ? config.ip_tag_header().action()
                                : HeaderAction::IPTagging_IpTagHeader_HeaderAction_SANITIZE) {

  // Once loading IP tags from a file system is supported, the restriction on the size
  // of the set should be removed and observability into what tags are loaded needs
  // to be implemented.
  // TODO(ccaraman): Remove size check once file system support is implemented.
  // Work is tracked by issue https://github.com/envoyproxy/envoy/issues/2695.
  if (config.ip_tags().empty()) {
    creation_status =
        absl::InvalidArgumentError("HTTP IP Tagging Filter requires ip_tags to be specified.");
    return;
  }

  std::vector<std::pair<std::string, std::vector<Network::Address::CidrRange>>> tag_data;
  tag_data.reserve(config.ip_tags().size());
  for (const auto& ip_tag : config.ip_tags()) {
    std::vector<Network::Address::CidrRange> cidr_set;
    cidr_set.reserve(ip_tag.ip_list().size());
    for (const envoy::config::core::v3::CidrRange& entry : ip_tag.ip_list()) {

      absl::StatusOr<Network::Address::CidrRange> cidr_or_error =
          Network::Address::CidrRange::create(entry);
      if (cidr_or_error.status().ok()) {
        cidr_set.emplace_back(std::move(cidr_or_error.value()));
      } else {
        creation_status = absl::InvalidArgumentError(
            fmt::format("invalid ip/mask combo '{}/{}' (format is <ip>/<# mask bits>)",
                        entry.address_prefix(), entry.prefix_len().value()));
        return;
      }
    }

    tag_data.emplace_back(ip_tag.ip_tag_name(), cidr_set);
    stat_name_set_->rememberBuiltin(absl::StrCat(ip_tag.ip_tag_name(), ".hit"));
  }
  trie_ = std::make_unique<Network::LcTrie::LcTrie<std::string>>(tag_data);
}

void IpTaggingFilterConfig::incCounter(Stats::StatName name) {
  Stats::SymbolTable::StoragePtr storage = scope_.symbolTable().join({stats_prefix_, name});
  scope_.counterFromStatName(Stats::StatName(storage.get())).inc();
}

IpTaggingFilter::IpTaggingFilter(IpTaggingFilterConfigSharedPtr config) : config_(config) {}

IpTaggingFilter::~IpTaggingFilter() = default;

void IpTaggingFilter::onDestroy() {}

Http::FilterHeadersStatus IpTaggingFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
  const bool is_internal_request = headers.EnvoyInternalRequest() &&
                                   (headers.EnvoyInternalRequest()->value() ==
                                    Http::Headers::get().EnvoyInternalRequestValues.True.c_str());

  if ((is_internal_request && config_->requestType() == FilterRequestType::EXTERNAL) ||
      (!is_internal_request && config_->requestType() == FilterRequestType::INTERNAL) ||
      !config_->runtime().snapshot().featureEnabled("ip_tagging.http_filter_enabled", 100)) {
    return Http::FilterHeadersStatus::Continue;
  }

  std::vector<std::string> tags =
      config_->trie().getData(callbacks_->streamInfo().downstreamAddressProvider().remoteAddress());

  applyTags(headers, tags);
  if (!tags.empty()) {
    // For a large number(ex > 1000) of tags, stats cardinality will be an issue.
    // If there are use cases with a large set of tags, a way to opt into these stats
    // should be exposed and other observability options like logging tags need to be implemented.
    for (const std::string& tag : tags) {
      config_->incHit(tag);
    }
  } else {
    config_->incNoHit();
  }
  config_->incTotal();
  return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus IpTaggingFilter::decodeData(Buffer::Instance&, bool) {
  return Http::FilterDataStatus::Continue;
}

Http::FilterTrailersStatus IpTaggingFilter::decodeTrailers(Http::RequestTrailerMap&) {
  return Http::FilterTrailersStatus::Continue;
}

void IpTaggingFilter::setDecoderFilterCallbacks(Http::StreamDecoderFilterCallbacks& callbacks) {
  callbacks_ = &callbacks;
}

void IpTaggingFilter::applyTags(Http::RequestHeaderMap& headers,
                                const std::vector<std::string>& tags) {
  using HeaderAction = IpTaggingFilterConfig::HeaderAction;

  OptRef<const Http::LowerCaseString> header_name = config_->ipTagHeader();

  if (tags.empty()) {
    bool maybe_sanitize =
        config_->ipTagHeaderAction() == HeaderAction::IPTagging_IpTagHeader_HeaderAction_SANITIZE;
    if (header_name.has_value() && maybe_sanitize) {
      if (headers.remove(header_name.value()) != 0) {
        // We must clear the route cache in case it held a decision based on the now-removed header.
        callbacks_->downstreamCallbacks()->clearRouteCache();
      }
    }
    return;
  }

  const std::string tags_join = absl::StrJoin(tags, ",");
  if (!header_name.has_value()) {
    // The x-envoy-ip-tags header was cleared at the start of the filter chain.
    // We only do append here, so that if multiple ip-tagging filters are run sequentially,
    // the behaviour will be backwards compatible.
    headers.appendEnvoyIpTags(tags_join, ",");
  } else {
    switch (config_->ipTagHeaderAction()) {
      PANIC_ON_PROTO_ENUM_SENTINEL_VALUES;
    case HeaderAction::IPTagging_IpTagHeader_HeaderAction_SANITIZE:
      headers.setCopy(header_name.value(), tags_join);
      break;
    case HeaderAction::IPTagging_IpTagHeader_HeaderAction_APPEND_IF_EXISTS_OR_ADD:
      headers.appendCopy(header_name.value(), tags_join);
      break;
    }
  }

  // We must clear the route cache so it can match on the updated value of the header.
  callbacks_->downstreamCallbacks()->clearRouteCache();
}

} // namespace IpTagging
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
