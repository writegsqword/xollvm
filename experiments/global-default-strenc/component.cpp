#include <cstdlib>
#include <cstring>

namespace {

constexpr char kExpected[] = "xollvm-component-sentinel-9P6M3";
const char* g_component_saved = "xollvm-component-sentinel-9P6M3";

struct ComponentEarlyReader {
  ComponentEarlyReader() {
    if (std::strcmp(g_component_saved, kExpected) != 0)
      std::abort();
  }
};

ComponentEarlyReader g_component_early_reader;

}  // namespace

extern "C" __attribute__((noinline)) const char* component_literal() {
  return "xollvm-component-sentinel-9P6M3";
}
