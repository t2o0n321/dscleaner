#include <cleaner.hpp>
#include <iostream>
#include <junk.hpp>
#include <vector>

int Cleaner::Clean(const fs::path& target_dir, bool recursive, bool verbose) {
  if (verbose) {
    if (recursive) {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string()
                << " and all its subfolders" << std::endl;
    } else {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string() << std::endl;
    }
  }

  // Stream the tree and keep only the junk paths. Memory stays proportional to
  // the amount of junk found, not to the total number of files scanned, so a
  // sweep of a huge volume does not balloon resident memory.
  std::vector<fs::path> junk_paths;
  FileManager::ForEach(target_dir, recursive, [&](const fs::path& entry) {
    if (junk::IsJunk(FileManager::FileName(entry))) {
      junk_paths.push_back(entry);
    }
  });

  int count = 0;
  for (const auto& entry : junk_paths) {
    // A junk directory may already have been removed as part of an earlier junk
    // parent (entries are discovered parent-first); skip if it is gone.
    if (!FileManager::Exists(entry)) {
      continue;
    }
    if (FileManager::RemovePath(entry) > 0) {
      if (verbose) {
        std::cout << "Removed: " << entry.string() << std::endl;
      }
      count++;
    }
  }

  if (verbose) {
    std::cout << "Removed " << count << " junk item(s)." << std::endl;
  }
  return count;
}
