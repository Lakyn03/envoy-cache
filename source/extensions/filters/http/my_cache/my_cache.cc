#include "source/extensions/filters/http/my_cache/my_cache.h"
#include "source/extensions/filters/http/my_cache/my_cache_filter.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

// we want to make a deep copy of the headers
Response::Response(const Http::ResponseHeaderMapPtr headers, std::string body)
    : headers_(Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*headers)),
      body_(std::move(body)) {}

Response& Response::operator=(const Response& other) {
  headers_ = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(*other.headers_);
  body_ = other.body_;
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

void RingBuffer::storeToBuffer(std::string path, Response response) {
  absl::WriterMutexLock lock(&BufMutex_);
  int id;
  // if full, replace start and move it
  if (curSize_ == capacity_) {
    id = startId_;
    startId_ = (startId_ + 1) % capacity_;

    // TODO
    // problem that I need to remove the old mapping from the map, but cant find it in O(1)
    // stupid O(n) solution, try to replace with a map other way around?
    for (auto it = pathToId_.begin(); it != pathToId_.end(); ++it) {
      if (it->second == id) {
        pathToId_.erase(it);
        break;
      }
    }
  } else {
    id = (startId_ + curSize_) % capacity_;
    curSize_++;
  }

  // now replace the one on id with the new response, update the map accordingly
  buffer_[id] = std::move(response);
  pathToId_[path] = id;
}

std::optional<Response> MyCache::getFromCache(std::string host, std::string path) {
  absl::ReaderMutexLock lock(&hostToBufferMutex_);
  auto it = hostToBuffer_.find(host);
  if (it != hostToBuffer_.end()) {
    return it->second->getFromBuffer(path);
  }

  return std::nullopt;
}

void MyCache::storeToCache(std::string host, std::string path, Response response) {
  absl::WriterMutexLock lock(&hostToBufferMutex_);

  auto it = hostToBuffer_.find(host);
  if (it != hostToBuffer_.end()) {
    it->second->storeToBuffer(path, std::move(response));
  } else {
    auto buffer = std::make_unique<RingBuffer>(buffer_size_);
    buffer->storeToBuffer(path, std::move(response));
    hostToBuffer_[host] = std::move(buffer);
  }
}

bool MyCache::checkRequest(std::string key, std::shared_ptr<MyCacheFilter> filter) {
  absl::MutexLock lock(&responsesMutex_);
  auto it = waitingForResponses_.find(key);
  if (it != waitingForResponses_.end()) {
    it->second.push_back(filter);
    return true;
  }

  waitingForResponses_[key] = std::vector<std::shared_ptr<MyCacheFilter>>();
  return false;
}

void MyCache::notifyAllHeaders(std::string key, Http::ResponseHeaderMap& headers, bool end_stream) {
  std::vector<std::shared_ptr<MyCacheFilter>> filters;

  {
    absl::WriterMutexLock lock(&responsesMutex_);
    auto it = waitingForResponses_.find(key);
    if (it == waitingForResponses_.end())
      return;

    filters = it->second;

    if (end_stream) {
      waitingForResponses_.erase(it);
    }
  }

  for (auto& filter : filters) {
    auto headers_copy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);
    filter->getDispatcher().post([filter, end_stream, headers_copy = std::move(headers_copy)]() {
      filter->sendHeaders(*headers_copy, end_stream);
    });
  }
}

void MyCache::notifyAllData(std::string key, Http::ResponseHeaderMap& headers,
                            std::shared_ptr<Buffer::Instance> sharedBuf, Buffer::Instance& chunk,
                            bool end_stream) {
  std::vector<std::shared_ptr<MyCacheFilter>> filters;
  {
    absl::WriterMutexLock lock(&responsesMutex_);
    auto it = waitingForResponses_.find(key);
    if (it == waitingForResponses_.end())
      return;

    filters = it->second;

    if (end_stream) {
      waitingForResponses_.erase(key);
    }
  }

  for (auto& filter : filters) {
    auto chunkCopy = std::make_shared<Buffer::OwnedImpl>(chunk);
    filter->getDispatcher().post([filter, &headers, chunkCopy, end_stream, sharedBuf]() {
      if (!filter->headersSent()) {
        auto bufCopy = std::make_shared<Buffer::OwnedImpl>();
        bufCopy->add(*sharedBuf);
        auto headersCopy = Http::createHeaderMap<Http::ResponseHeaderMapImpl>(headers);
        filter->sendHeaders(*headersCopy, end_stream);

        filter->sendData(*bufCopy, false);
      } else {
        filter->sendData(*chunkCopy, end_stream);
      }
    });
  }
}

std::shared_ptr<Singleton::Instance> createMyCache(int size) {
  return std::make_shared<MyCache>(size);
}

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
