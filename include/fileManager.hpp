#pragma once

#include <filesystem>
#include <string_view>

namespace fs = std::filesystem;

// Small std::filesystem helpers used outside the throughput-critical cleaner
// (which talks to POSIX directly - see cleaner.cpp). All methods are static; the
// class is a grouping namespace rather than an instantiable type.
class FileManager {
 public:
  // Returns the final path component as a view into `path` - no allocation. The
  // view is valid only while `path` is alive, and only for POSIX paths ('/'
  // separator), which is what dscleaner targets (macOS / Linux). Used on the
  // copier's hot path to feed junk::IsJunk without building a temporary string.
  static std::string_view FileName(const fs::path& path);

  // Thin, exception-free wrappers (errors are reported as a false result rather
  // than thrown) used for CLI argument validation.
  static bool IsDirectory(const fs::path& path);
  static bool Exists(const fs::path& path);
};
