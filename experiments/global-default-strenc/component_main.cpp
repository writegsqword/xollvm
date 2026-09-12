#include <cstring>

extern "C" const char* component_literal();

namespace {

constexpr char kExpected[] = "xollvm-component-sentinel-9P6M3";

__attribute__((noinline)) void clobber_stack() {
  volatile unsigned char bytes[8192];
  for (unsigned i = 0; i != sizeof(bytes); ++i)
    bytes[i] = static_cast<unsigned char>(i ^ 0xa5);
}

}  // namespace

int main() {
  const char* first = component_literal();
  clobber_stack();
  if (std::strcmp(first, kExpected) != 0)
    return 1;
  return first == component_literal() ? 0 : 2;
}
