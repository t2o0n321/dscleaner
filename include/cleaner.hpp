#pragma once

#include <fileManager.hpp>

class Cleaner {
public:
    // Remove every macOS junk file/directory under targetDir.
    //   recursive : descend into subdirectories.
    //   verbose   : print per-item / summary output (set false for the watcher
    //               and post-copy sweeps so they stay quiet unless something is
    //               actually removed).
    // Returns the number of junk items removed.
    int clean(const fs::path& targetDir, bool recursive, bool verbose = true);
};
