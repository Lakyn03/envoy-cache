#include "source/extensions/filters/http/my_cache/my_cache.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {



Response::Response(Http::ResponseHeaderMapPtr headers, std::string body) : headers_(std::move(headers)), body_(body) {}

RingBuffer::RingBuffer(int capacity) {
    capacity_ = capacity;
    buffer_.resize(capacity);
}


Response* RingBuffer::getFromBuffer(std::string path) {
    absl::ReaderMutexLock lock(&BufMutex_);
    auto it = pathToId_.find(path);
    if (it != pathToId_.end()) {
        return &buffer_[it->second];
    }

    return nullptr;
}

void RingBuffer::storeToBuffer(std::string path, Response response) {
    absl::WriterMutexLock lock(&BufMutex_);
    int id;
    // if full, replace start and move it
    if(curSize_ == capacity_) {
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



Response* MyCache::getFromCache(std::string host, std::string path) {
    absl::ReaderMutexLock lock(&mutex_);
    auto it = hostToBuffer_.find(host);
    if (it != hostToBuffer_.end()) {
        return it->second->getFromBuffer(path);
    }

    return nullptr;
}

void MyCache::storeToCache(std::string host, std::string path, Response response) {
    absl::WriterMutexLock lock(&mutex_);

    auto it = hostToBuffer_.find(host);
    if (it != hostToBuffer_.end()) {
        it->second->storeToBuffer(path, std::move(response));
    } else {
        auto buffer = std::make_unique<RingBuffer>(buffer_size_);
        buffer->storeToBuffer(path, std::move(response));
        hostToBuffer_[host] = std::move(buffer);
    }
}

std::shared_ptr<Singleton::Instance> createMyCache(int size) { return std::make_shared<MyCache>(size); }

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
