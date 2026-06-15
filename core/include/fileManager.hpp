#pragma once

#include <filesystem>

namespace fs = std::filesystem;

// Small std::filesystem helpers used for CLI argument validation. The
// throughput-critical cleaner and copier talk to POSIX directly (see
// cleaner.cpp / copier.cpp). All methods are static; the class is a grouping
// namespace rather than an instantiable type.
class FileManager {
 public:
  // Exception-free wrappers (errors are reported as a false result rather than
  // thrown).
  static bool IsDirectory(const fs::path& path);
  static bool Exists(const fs::path& path);
};
