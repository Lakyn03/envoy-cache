#pragma once

#include "source/extensions/filters/http/common/pass_through_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {


class MyCacheFilter : public Http::PassThroughFilter {
public:
    MyCacheFilter(const std::string msg);
    ~MyCacheFilter() override = default;

    Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) override;

private:
    std::string msg_;
};



}
}
}
}