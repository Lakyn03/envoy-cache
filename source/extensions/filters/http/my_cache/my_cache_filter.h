#pragma once

#include <string>
#include "source/extensions/filters/http/my_cache/my_cache.h"
#include "source/common/http/header_map_impl.h"
#include "source/extensions/filters/http/common/pass_through_filter.h"
#include "envoy/buffer/buffer.h"


namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

class MyCacheFilter : public Http::PassThroughFilter, public std::enable_shared_from_this<MyCacheFilter>{
public:
  MyCacheFilter(std::shared_ptr<MyCache> cache);
  ~MyCacheFilter() override = default;

  Envoy::Event::Dispatcher& getDispatcher();
  bool headersSent();
  
  // sends cached (or coalesced) response
  void sendResponse(std::shared_ptr<Response> response);

  void sendHeaders(Http::ResponseHeaderMap& headers, bool end_stream);
  void sendData(Buffer::Instance& data, bool end_stream);

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) override;

  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& headers, bool end_stream) override;

private:
  std::shared_ptr<MyCache> cache_; // reference to cache
  std::string host_;
  std::string path_;
  bool responseFromCache_ = false; // whether the response is being served from cache or backend
  bool needsToBeCached_ = false;
  bool coalLeader_; // true if this filter is the one sending a coalesced request upstream and should notify others upon arrival
  bool headersSent_ = false;
  Http::ResponseHeaderMapPtr headers_; //headers of the response
  std::string buffer_; // buffer for collecting the body of the reponse in case it is being sent in more chunks
  std::shared_ptr<Buffer::Instance> sharedBuf_;
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy