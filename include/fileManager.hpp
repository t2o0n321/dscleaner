#pragma once

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

// Filesystem helpers shared across dscleaner. All methods are static; the class
// is a grouping namespace rather than an instantiable type.
class FileManager {
 public:
  // Streams every entry (files *and* directories) under `path` to `visit`,
  // skipping directories that cannot be read instead of aborting the scan.
  //
  // Streaming (rather than returning a vector of the whole tree) keeps memory
  // usage proportional to the directory depth, not to the total number of
  // files - important when sweeping a large volume. `visit` is a callable
  // taking `const fs::path&`; it is invoked inline, so the hot per-entry test
  // is not hidden behind a std::function indirection.
  template <typename Visit>
  static void ForEach(const fs::path& path, bool recursive, Visit&& visit) {
    std::error_code ec;
    if (recursive) {
      fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
      if (ec) {
        std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
        return;
      }
      const fs::recursive_directory_iterator end;
      while (it != end) {
        visit(it->path());
        it.increment(ec);
        if (ec) {
          std::cerr << "Skipping inaccessible entry: " << ec.message() << std::endl;
          ec.clear();
        }
      }
    } else {
      fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
      if (ec) {
        std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
        return;
      }
      const fs::directory_iterator end;
      while (it != end) {
        visit(it->path());
        it.increment(ec);
        if (ec) {
          std::cerr << "Skipping inaccessible entry: " << ec.message() << std::endl;
          ec.clear();
        }
      }
    }
  }

  // Returns the final path component as a view into `path` (no allocation). The
  // view is valid only while `path` is alive. POSIX paths only ('/' separator),
  // which is what dscleaner targets (macOS / Linux).
  static std::string_view FileName(const fs::path& path);

  // Removes a file or an entire directory tree. Returns the number of items
  // removed (0 on error or if nothing was removed).
  static std::uintmax_t RemovePath(const fs::path& path);

  static bool IsDirectory(const fs::path& path);
  static bool Exists(const fs::path& path);
};
