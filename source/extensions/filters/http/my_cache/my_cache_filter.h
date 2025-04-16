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

  // needed in MyChache to post to thread managing this filter
  Envoy::Event::Dispatcher& getDispatcher() const;
  
  // sends cached response
  void sendResponse(std::shared_ptr<Response> response);
  void sendHeaders(Http::ResponseHeaderMap& headers, bool end_stream);
  void sendData(const Buffer::Instance& data, bool end_stream);

  Http::FilterHeadersStatus decodeHeaders(Http::RequestHeaderMap& headers, bool end_stream) override;
  Http::FilterHeadersStatus encodeHeaders(Http::ResponseHeaderMap& headers, bool end_stream) override;
  Http::FilterDataStatus encodeData(Buffer::Instance& headers, bool end_stream) override;

private:
  std::shared_ptr<MyCache> cache_;
  std::shared_ptr<Coalescing> coal_;
  std::string host_;
  std::string path_;
  // key_ + host_ used for coalescing
  std::string key_;
  // whether the response is being served from cache or backend
  bool responseFromCache_ = false; 
  // whether the response has been cached in the meantime or still needs to be stored
  bool needsToBeCached_ = false;
  // true if this filter is the one sending a coalesced request upstream and should notify others upon arrival
  bool coalLeader_ = false; 
  Http::ResponseHeaderMapPtr headers_;
  std::shared_ptr<Buffer::Instance> buffer_;
};

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy