#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

// Filesystem helpers shared across dscleaner. All methods are static; the class
// is a grouping namespace rather than an instantiable type.
class FileManager {
 public:
  // Returns every entry (files *and* directories) under `path`. Directories
  // that cannot be read are skipped instead of aborting the whole scan.
  static std::vector<fs::path> GetEntries(const fs::path& path, bool recursive);

  // Removes a file or an entire directory tree. Returns the number of items
  // removed (0 on error or if nothing was removed).
  static std::uintmax_t RemovePath(const fs::path& path);

  static bool IsDirectory(const fs::path& path);
  static bool Exists(const fs::path& path);
};
