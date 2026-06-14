#include <cleaner.hpp>
#include <iostream>
#include <junk.hpp>

int Cleaner::Clean(const fs::path& target_dir, bool recursive, bool verbose) {
  if (verbose) {
    if (recursive) {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string()
                << " and all its subfolders" << std::endl;
    } else {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string() << std::endl;
    }
  }

  std::vector<fs::path> const entries = FileManager::GetEntries(target_dir, recursive);
  int count = 0;
  for (const auto& entry : entries) {
    if (!junk::IsJunk(entry.filename().string())) {
      continue;
    }
    // A junk directory may already have been removed as part of an earlier
    // junk parent (entries are listed parent-first); skip if it is gone.
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
