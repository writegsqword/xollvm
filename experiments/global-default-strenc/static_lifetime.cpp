#include <cstdlib>
#include <cstring>

namespace {

constexpr char kExpected[] = "xollvm-static-lifetime-sentinel-7Y4Q2";
const char* g_saved = "xollvm-static-lifetime-sentinel-7Y4Q2";

__attribute__((noinline)) const char* return_literal() {
  return "xollvm-static-lifetime-sentinel-7Y4Q2";
}

__attribute__((noinline)) void clobber_stack() {
  volatile unsigned char bytes[8192];
  for (unsigned i = 0; i != sizeof(bytes); ++i)
    bytes[i] = static_cast<unsigned char>(i);
}

struct EarlyReader {
  EarlyReader() {
    if (std::strcmp(g_saved, kExpected) != 0)
      std::abort();
  }
};

EarlyReader g_early_reader;

}  // namespace

int main() {
  const char* first = return_literal();
  clobber_stack();
  if (std::strcmp(first, kExpected) != 0)
    return 1;
  if (first != return_literal())
    return 2;
  if (g_saved != return_literal())
    return 3;
  return 0;
}
