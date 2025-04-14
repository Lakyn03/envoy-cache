#include "source/extensions/filters/http/my_cache/my_cache_filter.h"
#include "source/common/common/logger.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

MyCacheFilter::MyCacheFilter(std::shared_ptr<MyCache> cache) : cache_(cache) {
    sharedBuf_ = std::make_shared<Buffer::OwnedImpl>();
}


Envoy::Event::Dispatcher& MyCacheFilter::getDispatcher() {
    return decoder_callbacks_->dispatcher();
}

bool MyCacheFilter::headersSent() {
    return headersSent_;
}


void MyCacheFilter::sendResponse(std::shared_ptr<Response> response) {
    Http::ResponseHeaderMapPtr headers = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(response->getHeaders());
    Buffer::OwnedImpl body(response->getBody()); 

    //send headers
    decoder_callbacks_->encodeHeaders(std::move(headers), body.length() == 0, "");
        
    //send body 
    if(body.length() != 0) {
        decoder_callbacks_->encodeData(body, true);
    }
}


void MyCacheFilter::sendHeaders(Http::ResponseHeaderMap& headers, bool end_stream) {
    if(headersSent_) return;

    headersSent_ = true;

    Http::ResponseHeaderMapPtr headersCopy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);

    decoder_callbacks_->encodeHeaders(std::move(headersCopy), end_stream, "");
}


void MyCacheFilter::sendData(Buffer::Instance& data, bool end_stream) {
    Buffer::OwnedImpl body(data); 

    decoder_callbacks_->encodeData(body, end_stream);
}


Http::FilterHeadersStatus MyCacheFilter::decodeHeaders(Http::RequestHeaderMap& headers, bool) {
    host_ = std::string(headers.getHostValue());
    path_ = std::string(headers.getPathValue());

    std::optional<Response> response = cache_->getFromCache(host_, path_);
    // if cached, send response
    if(response.has_value()) {
        //ENVOY_LOG_MISC(info, "Found in cache for key '{}'", host_ + path_);
        responseFromCache_ = true;
        sendResponse(std::make_shared<Response>(response.value()));
    
        //stop iteration
        return Http::FilterHeadersStatus::StopIteration;
    }

    if(cache_->checkRequest(host_ + path_, shared_from_this())) {
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


    // if still not cached, start storing the response
    if(!cache_->getFromCache(host_, path_)) {
        needsToBeCached_ = true;
        if(end_stream) {
            // store response with no body
            Response response = Response(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers), std::string());
            cache_->storeToCache(host_, path_, response);
        } else {
            // save just the headers for later
            headers_ = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);
        }

        if(coalLeader_) {
            cache_->notifyAllHeaders(host_ + path_, headers, end_stream);
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
        sharedBuf_->add(data);

        if(end_stream) {
            Response response = Response(std::move(headers_), std::move(buffer_));
            cache_->storeToCache(host_, path_, response);
        }
    }

    if(coalLeader_) {
        cache_->notifyAllData(host_ + path_, *headers_, sharedBuf_, data, end_stream);
    }
    
    return Http::FilterDataStatus::Continue;
}

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy