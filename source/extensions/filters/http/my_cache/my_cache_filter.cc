#include "source/extensions/filters/http/my_cache/my_cache_filter.h"
#include "source/common/common/logger.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

MyCacheFilter::MyCacheFilter(std::shared_ptr<MyCache> cache) : cache_(cache) {
    buffer_ = std::make_shared<Buffer::OwnedImpl>();
}

Envoy::Event::Dispatcher& MyCacheFilter::getDispatcher() const {
    return decoder_callbacks_->dispatcher();
}

void MyCacheFilter::sendResponse(std::shared_ptr<Response> response) {
    Http::ResponseHeaderMapPtr headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(response->getHeaders());
    Buffer::OwnedImpl body(response->getBody()); 

    decoder_callbacks_->encodeHeaders(std::move(headers), body.length() == 0, "");
    if(body.length() != 0) {
        decoder_callbacks_->encodeData(body, true);
    }
}

void MyCacheFilter::sendHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
    Http::ResponseHeaderMapPtr headersCopy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);
    decoder_callbacks_->encodeHeaders(std::move(headersCopy), end_stream, "");
}

void MyCacheFilter::sendData(const Buffer::Instance& data, bool end_stream) {
    Buffer::OwnedImpl body(data); 
    decoder_callbacks_->encodeData(body, end_stream);
}

Http::FilterHeadersStatus MyCacheFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
    host_ = std::string(headers.getHostValue());
    path_ = std::string(headers.getPathValue());
    key_ = host_ + path_;

    // check no-cache header
    auto cacheControl = headers.get(Http::LowerCaseString("cache-control"));
    if(!cacheControl.empty() && absl::StrContains(cacheControl[0]->value().getStringView(), "no-cache")) {
        return Http::FilterHeadersStatus::Continue;
    }

    std::optional<Response> response = cache_->getFromCache(host_, path_);
    // if cached, send response
    if(response.has_value()) {
        responseFromCache_ = true;
        sendResponse(std::make_shared<Response>(response.value()));
        //stop the filter chain
        return Http::FilterHeadersStatus::StopIteration;
    }

    coal_ = cache_->checkRequest(key_, shared_from_this());
    if(coal_ == nullptr) {
        // request found, filter is in line, so sleep
        return Http::FilterHeadersStatus::StopIteration;
    }

    // not cached and not in waiting requests -> lead coalescing filter, notify others upon arrival
    coalLeader_ = true;

    return Http::FilterHeadersStatus::Continue;
}

Http::FilterHeadersStatus MyCacheFilter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
    if(responseFromCache_) {
        return Http::FilterHeadersStatus::Continue;
    }

    headers_ = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);

    // if coal leader, send to others
    if(coalLeader_) {
        coal_->setHeaders(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers));
        coal_->send(end_stream);

        if(end_stream) {
            cache_->deleteCoal(key_);
        }
    }

    // if still not cached, start storing the response
    if(!cache_->getFromCache(host_, path_)) {
        needsToBeCached_ = true;
        if(end_stream) {
            // store response with no body
            Response response = Response(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers), Buffer::OwnedImpl());
            cache_->storeToCache(host_, path_, response);
        }
    }
    
    return Http::FilterHeadersStatus::Continue;
}

Http::FilterDataStatus MyCacheFilter::encodeData(Buffer::Instance& data, bool end_stream) {
    if(responseFromCache_) {
        return Http::FilterDataStatus::Continue;
    }

    buffer_->add(data);

    // if coal leader, send to others
    if(coalLeader_) {
        coal_->addData(data);
        coal_->send(end_stream);

        if(end_stream) {
            cache_->deleteCoal(key_);
        }
    }

    // store the body
    if(needsToBeCached_) {
        if(end_stream) {
            Response response = Response(std::move(headers_), *buffer_);
            cache_->storeToCache(host_, path_, response);
        }
    }
    return Http::FilterDataStatus::Continue;
}

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy