#include <stdexcept>
#include <cstring>

__attribute__((noinline)) static int exception_path(int value) {
  try {
    if (value == 17)
      throw std::runtime_error("xollvm-exception-lifetime-sentinel");
    return value + 3;
  } catch (const std::runtime_error &error) {
    return std::strlen(error.what()) == 34 ? 71 : -1;
  }
}

int main(int argc, char **) {
  return exception_path(argc + 16) == 71 ? 0 : 1;
}
