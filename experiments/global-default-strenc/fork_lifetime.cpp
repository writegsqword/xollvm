#include <cstring>
#include <sys/wait.h>
#include <unistd.h>

namespace {

constexpr char kExpected[] = "xollvm-fork-lifetime-sentinel-4H8N1";
const char* g_saved = "xollvm-fork-lifetime-sentinel-4H8N1";

}  // namespace

int main() {
  pid_t child = fork();
  if (child < 0)
    return 1;
  if (child == 0)
    _exit(std::strcmp(g_saved, kExpected) == 0 ? 0 : 2);

  int status = 0;
  if (waitpid(child, &status, 0) != child)
    return 3;
  return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 4;
}
