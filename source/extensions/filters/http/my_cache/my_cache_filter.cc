#include "source/extensions/filters/http/my_cache/my_cache_filter.h"
#include "source/common/common/logger.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

MyCacheFilter::MyCacheFilter(std::shared_ptr<MyCache> cache) : cache_(cache) {}

Http::FilterHeadersStatus MyCacheFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
    std::string host = std::string(headers.getHostValue());
    std::string path = std::string(headers.getPathValue());
    key_ = host + path;

    Response* response = cache_->getCached(key_);
    // if cached, send response
    if(response) {
        ENVOY_LOG_MISC(info, "Found in cache for key '{}'", key_);
        responseFromCache_ = true;

        Http::ResponseHeaderMapPtr headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(response->getHeaders());
        Buffer::OwnedImpl body(response->getBody()); 

        //send headers
        decoder_callbacks_->encodeHeaders(std::move(headers), body.length() == 0, "cache_hit");
        
        //send body 
        if(body.length() != 0) {
            decoder_callbacks_->encodeData(body, true);
        }

        //stop iteration
        return Http::FilterHeadersStatus::StopIteration;
    }

    return Http::FilterHeadersStatus::Continue;
}


Http::FilterHeadersStatus MyCacheFilter::encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
    if(responseFromCache_) {
        return Http::FilterHeadersStatus::Continue;
    }


    // if still not cached, start storing the response
    if(!cache_->getCached(key_)) {
        needsToBeCached_ = true;
        if(end_stream) {
            // store response with no body
            cache_->storeResponse(key_, Response(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers), std::string()));
        } else {
            // save just the headers for later
            headers_ = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);
        }
    }
    
    return Http::FilterHeadersStatus::Continue;
}


Http::FilterDataStatus MyCacheFilter::encodeData(Buffer::Instance& data, bool end_stream) {
    if(responseFromCache_) {
        return Http::FilterDataStatus::Continue;
    }

    // store the body
    if(needsToBeCached_) {
        buffer_ += data.toString();

        if(end_stream) {
            cache_->storeResponse(key_, Response(std::move(headers_), std::move(buffer_)));
        }
    }
    

    return Http::FilterDataStatus::Continue;
}

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy