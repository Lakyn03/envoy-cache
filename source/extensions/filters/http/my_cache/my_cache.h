#pragma once

#include <memory>
#include <atomic>
#include <string>
#include "envoy/singleton/instance.h"

namespace Envoy {
namespace Extensions {
namespace HttpFilters {
namespace MyCacheFilter {

class MyCache : public Singleton::Instance {
public:
  MyCache() = default;

  std::string getMsg();

private:
  std::atomic<int> count_{0};
};

// cache singleton class creation
std::shared_ptr<Singleton::Instance> createMyCache();

} // namespace MyCacheFilter
} // namespace HttpFilters
} // namespace Extensions
} // namespace Envoy
