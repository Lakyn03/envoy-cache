#include "source/extensions/filters/http/my_cache/my_cache.h"
#include "source/extensions/filters/http/my_cache/my_cache_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// we want to make a deep copy of the headers
Response::Response(const Http::ResponseHeaderMapPtr headers, const Buffer::Instance& body)
    : headers_(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*headers)) {
        body_.add(body);
    }

Response& Response::operator=(const Response& other) {
  headers_ = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*other.headers_);
  body_ = Buffer::OwnedImpl();
  body_.add(other.body_);
  return *this;
}

RingBuffer::RingBuffer(int capacity) {
  capacity_ = capacity;
  buffer_.resize(capacity);
}

std::optional<Response> RingBuffer::getFromBuffer(const std::string& path) {
  absl::ReaderMutexLock lock(&BufMutex_);
  auto it = pathToId_.find(path);
  if (it != pathToId_.end()) {
    // makes a copy
    return buffer_[it->second];
  }
  return std::nullopt;
}

void RingBuffer::storeToBuffer(const std::string& path, Response response) {
  absl::WriterMutexLock lock(&BufMutex_);
  int id;
  // if full, replace start and move it
  if (curSize_ == capacity_) {
    id = startId_;
    startId_ = (startId_ + 1) % capacity_;

    // delete the old mappings
    auto oldPath = idToPath_.find(id);
    if(oldPath != idToPath_.end()) {
        pathToId_.erase(oldPath->second);
        idToPath_.erase(id);
    }
  } else {
    id = (startId_ + curSize_) % capacity_;
    curSize_++;
  }

  // now replace the one on id with the new response, update the map accordingly
  buffer_[id] = std::move(response);
  pathToId_[path] = id;
  idToPath_[id] = path;
}

void Coalescing::setHeaders(Http::ResponseHeaderMapPtr headers) {
  absl::MutexLock lock(&mutex_);
  if (!headers_) {
    headers_ = std::move(headers);
  }
}

void Coalescing::addData(const Buffer::Instance& data) {
  absl::MutexLock lock(&mutex_);
  buffer_.add(data);
}

void Coalescing::addFilter(std::shared_ptr<MyCacheFilter> filter) {
  absl::MutexLock lock(&mutex_);
  auto filterInfo = std::make_shared<FilterInfo>();
  filterInfo->filter = filter;
  filterInfo->offset = 0;
  filterInfo->headersSent = false;
  filters_.push_back(filterInfo);
}


void Coalescing::send(bool end_stream) {
  absl::MutexLock lock(&mutex_);
  // copy buffer
  auto bufferCopy = std::make_shared<Buffer::OwnedImpl>();
  bufferCopy->add(buffer_);

  for (auto& filterInfo : filters_) {
    auto filter = filterInfo->filter;

    // check if headers already sent
    bool need_send_headers = false;
    if (!filterInfo->headersSent) {
      filterInfo->headersSent = true;
      need_send_headers = true;
    }

    // check how much data needs to be sent
    uint64_t available = bufferCopy->length();
    uint64_t offset = filterInfo->offset;
    uint64_t sendSize = 0;
    if (available > offset) {
      sendSize = available - offset;
      filterInfo->offset += sendSize;
    }

    // copy the chunk that needs to be sent
    Buffer::OwnedImpl chunk;
    if (sendSize > 0) {
      //raw memory space to store the copied data
      std::vector<char> temp(sendSize);
      bufferCopy->copyOut(offset, sendSize, temp.data());
      // add to the chunk
      chunk.add(temp.data(), sendSize);
    }

    auto headersCopy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*headers_);

    // post to the thread event pool, all values that are given to the lambda need to be copied, because it might be executed later and objects could be already deleted
    filter->getDispatcher().post([filter, need_send_headers, headersCopy = std::move(headersCopy),
                                  haveDataToSend = (sendSize > 0), chunk = std::move(chunk), end_stream]() {
        // security check to not get segfault
        if (!filter) {
          return;
        }
        if (need_send_headers) {
          filter->sendHeaders(*headersCopy, false);
        }
        if (haveDataToSend) {
          filter->sendData(chunk, end_stream);
        } else if (end_stream) {
          Buffer::OwnedImpl empty;
          filter->sendData(empty, true);
        }
      }
    );
  }
}


std::optional<Response> MyCache::getFromCache(const std::string& host, const std::string& path) {
  absl::ReaderMutexLock lock(&hostToBufferMutex_);
  auto it = hostToBuffer_.find(host);
  if (it != hostToBuffer_.end()) {
    return it->second->getFromBuffer(path);
  }
  return std::nullopt;
}

void MyCache::storeToCache(const std::string& host, const std::string& path, Response response) {
  absl::WriterMutexLock lock(&hostToBufferMutex_);

  auto it = hostToBuffer_.find(host);
  if (it != hostToBuffer_.end()) {
    it->second->storeToBuffer(path, std::move(response));
  } else {
    // create new buffer for this host
    auto buffer = std::make_unique<RingBuffer>(buffer_size_);
    buffer->storeToBuffer(path, std::move(response));
    hostToBuffer_[host] = std::move(buffer);
  }
}


std::shared_ptr<Coalescing> MyCache::checkRequest(const std::string& key, std::shared_ptr<MyCacheFilter> filter) {
  absl::MutexLock lock(&coalescingMutex_);
  auto it = coalescing_map_.find(key);

  // add new filter to waiting
  if (it != coalescing_map_.end()) {
    it->second->addFilter(filter);
    return nullptr;
  }

  //create new coalescing object
  std::shared_ptr<Coalescing> coal = std::make_shared<Coalescing>();
  coalescing_map_[key] = coal;
  return coal;
}


void MyCache::deleteCoal(const std::string& key) {
    absl::MutexLock lock(&coalescingMutex_);
    coalescing_map_.erase(key);
}

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
