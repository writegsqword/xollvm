#include <stdint.h>

__attribute__((noinline)) static int leaf(int x) {
  return x * 7 + 19;
}

__attribute__((noinline)) int branch_and_call(int x) {
  if ((x & 3) == 0)
    return leaf(x) + 11;
  if ((x & 3) == 1)
    return leaf(x - 1) - 5;
  return leaf(x + 2) ^ 0x4a;
}

int main(void) {
  uint32_t sum = 0;
  for (int i = 0; i != 32; ++i)
    sum += (uint32_t)branch_and_call(i);
  return sum == 4136 ? 0 : 1;
}
