__attribute__((noinline)) int inline_asm_identity(int value) {
  __asm__ volatile("" : "+r"(value));
  return value;
}

int main(void) {
  return inline_asm_identity(0);
}
