#include <junk.hpp>

namespace junk {
namespace {

// Exact directory / file names that macOS creates as metadata. Ordered roughly
// by how often they occur, so the linear scan below hits common cases first.
const std::vector<std::string> kExactNames = {
    ".DS_Store",     ".AppleDouble",     ".AppleDB",
    ".AppleDesktop", ".Spotlight-V100",  ".Trashes",
    ".fseventsd",    ".TemporaryItems",  ".DocumentRevisions-V100",
    ".apdisk",       ".VolumeIcon.icns", ".com.apple.timemachine.donotpresent",
};

// Equivalent glob patterns for external tools (tar/rsync/zip).
const std::vector<std::string> kGlobs = {
    ".DS_Store",
    "._*",
    ".AppleDouble",
    ".AppleDB",
    ".AppleDesktop",
    ".Spotlight-V100",
    ".Trashes",
    ".fseventsd",
    ".TemporaryItems",
    ".DocumentRevisions-V100",
    ".apdisk",
    ".VolumeIcon.icns",
    ".com.apple.timemachine.donotpresent",
};

// Fast test for the "._" AppleDouble prefix - by far the most common macOS junk
// and the hottest positive case in the cleaner's inner loop. It loads the first
// two bytes as a single 16-bit word and compares them against the little-endian
// pattern for "._" (byte0 = '.' = 0x2E, byte1 = '_' = 0x5F  =>  0x5F2E).
//
// Hand-written assembly is provided for the two targets dscleaner ships on -
// Apple Silicon (AArch64) and older Intel Macs (x86-64) - with a portable C++
// fallback for every other platform (and for static analysis).
//
// Precondition: `data` points to at least two readable bytes. Callers guarantee
// this by checking the length first; the std::string backing a path name is
// always NUL-terminated, so the second byte is safe to read even for a 1-char
// name, but we never rely on that here.
inline bool HasDotUnderscorePrefix(const char* data) {
#if defined(__aarch64__)
  // --- Apple Silicon / ARM64 (primary target) ---------------------------
  unsigned int word = 0;  // holds the two loaded bytes
  unsigned int want = 0;  // holds the "._" constant (needs its own register:
                          //   0x5F2E is too wide for a cmp immediate on ARM64)
  unsigned int result = 0;
  __asm__(
      "ldrh  %w[word], [%[p]]\n\t"    // word = *(uint16_t*)data (zero-extended)
      "movz  %w[want], #0x5F2E\n\t"   // want = 0x5F2E  ("._" little-endian)
      "cmp   %w[word], %w[want]\n\t"  // compare the two halfwords
      "cset  %w[result], eq\n\t"      // result = (word == want) ? 1 : 0
      : [word] "=&r"(word), [want] "=&r"(want), [result] "=r"(result)
      : [p] "r"(data)
      : "cc", "memory");  // "memory": the asm reads *data through a register
  return result != 0;
#elif defined(__x86_64__)
  // --- Older Intel Macs / x86-64 ----------------------------------------
  // NOLINTNEXTLINE(misc-const-correctness): written by the asm as an output.
  unsigned char result = 0;
  __asm__(
      "movzwl (%[p]), %%eax\n\t"  // eax = *(uint16_t*)data (zero-extended)
      "cmpw   $0x5F2E, %%ax\n\t"  // compare AX against "._" little-endian
      "sete   %[result]\n\t"      // result = (equal) ? 1 : 0
      : [result] "=q"(result)
      : [p] "r"(data)
      : "eax", "cc", "memory");  // "memory": the asm reads *data through a register
  return result != 0;
#else
  // --- Portable fallback ------------------------------------------------
  return data[0] == '.' && data[1] == '_';
#endif
}

}  // namespace

bool IsJunk(std::string_view filename) {
  // Fast reject: every macOS junk name begins with '.'. Ordinary files almost
  // never do, so this single byte test short-circuits the overwhelming majority
  // of entries before any further work.
  if (filename.empty() || filename.front() != '.') {
    return false;
  }
  // Hottest positive case: AppleDouble "._*" side-cars (the files that leak onto
  // USB drives). Handled by the architecture-specific primitive above.
  if (filename.size() >= 2 && HasDotUnderscorePrefix(filename.data())) {
    return true;
  }
  // Otherwise fall back to the (short) exact-name set. This branch is reached
  // only for the relatively rare dot-prefixed names.
  for (const auto& name : kExactNames) {
    if (filename == name) {
      return true;
    }
  }
  return false;
}

const std::vector<std::string>& GlobPatterns() { return kGlobs; }

bool IsJunk(const char* filename) {
  // Same logic as the string_view overload, but a NUL-terminated C string lets
  // us short-circuit without computing the length first.
  //   - filename[0] != '.'  -> not junk (also handles the empty-string case,
  //     where filename[0] is '\0').
  //   - the asm primitive may safely read two bytes: a C string always has at
  //     least its NUL terminator as the second byte.
  if (filename[0] != '.') {
    return false;
  }
  if (HasDotUnderscorePrefix(filename)) {
    return true;
  }
  for (const auto& name : kExactNames) {
    if (name == filename) {
      return true;
    }
  }
  return false;
}

}  // namespace junk
