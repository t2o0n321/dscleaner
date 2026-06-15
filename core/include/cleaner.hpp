#pragma once

#include <fileManager.hpp>

class Cleaner {
 public:
  // Removes every macOS junk file/directory under `target_dir`.
  //   recursive : descend into subdirectories.
  //   verbose   : print per-item / summary output. Set false for the watcher
  //               and post-copy sweeps so they stay quiet unless something is
  //               actually removed.
  // Returns the number of junk items removed.
  int Clean(const fs::path& target_dir, bool recursive, bool verbose = true);
};
