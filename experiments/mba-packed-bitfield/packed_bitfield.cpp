#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <new>

#if defined(XOLLVM_TEST_SOURCE_ANNOTATION)
#ifndef XOLLVM_TEST_MBA_CONFIG
#define XOLLVM_TEST_MBA_CONFIG \
  "mba(preset=light,prob=20,maxSites=24,maxDepth=1)"
#endif
#define XOLLVM_TEST_MBA                                                    \
  __attribute__((annotate("obf: " XOLLVM_TEST_MBA_CONFIG)))
#else
#define XOLLVM_TEST_MBA
#endif

namespace v8::internal::ro {

struct EncodedExternalReference {
  static constexpr int kTagBits = 8;
  static constexpr int kIsApiReferenceBits = 1;
  static constexpr int kIndexBits = 23;

  __attribute__((noinline)) XOLLVM_TEST_MBA
  EncodedExternalReference(uint16_t tag, bool is_api_reference, uint32_t index)
      : tag(tag), is_api_reference(is_api_reference), index(index) {}

  uint32_t ToUint32() const {
    uint32_t value;
    static_assert(sizeof(value) == sizeof(*this));
    std::memcpy(&value, this, sizeof(value));
    return value;
  }

  unsigned tag : kTagBits;
  unsigned is_api_reference : kIsApiReferenceBits;
  unsigned index : kIndexBits;
};

static_assert(sizeof(EncodedExternalReference) == sizeof(uint32_t));

}  // namespace v8::internal::ro

int main() {
  struct TestCase {
    uint16_t tag;
    uint32_t index;
  };

  constexpr TestCase cases[] = {
      {0x3d, 1591}, {0, 0}, {0xff, 0x7fffff}, {0xa5, 0x456789},
  };

  constexpr uint32_t initial_patterns[] = {
      0x00000000u, 0xffffffffu, 0x00000100u, 0xfffffeffu, 0xa5a5a5a5u,
  };

  for (const uint32_t initial_pattern : initial_patterns) {
    for (const TestCase test : cases) {
      for (const bool is_api_reference : {false, true}) {
        alignas(v8::internal::ro::EncodedExternalReference)
            unsigned char storage[sizeof(uint32_t)];
        std::memcpy(storage, &initial_pattern, sizeof(initial_pattern));
        const auto* encoded = new (storage)
            v8::internal::ro::EncodedExternalReference(
                test.tag, is_api_reference, test.index);
        const uint32_t actual = encoded->ToUint32();
        const uint32_t expected = static_cast<uint32_t>(test.tag & 0xffu) |
                                  (static_cast<uint32_t>(is_api_reference) << 8) |
                                  ((test.index & 0x7fffffu) << 9);
        if (actual != expected) {
          std::fprintf(stderr,
                       "packed bitfield mismatch: initial=%08x tag=%04x api=%u "
                       "index=%08x expected=%08x actual=%08x xor=%08x\n",
                       initial_pattern, test.tag, is_api_reference, test.index,
                       expected, actual, expected ^ actual);
          return 1;
        }
      }
    }
  }

  std::puts("packed bitfield MBA regression passed");
  return 0;
}
