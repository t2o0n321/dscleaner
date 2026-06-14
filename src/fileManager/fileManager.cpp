#include <fileManager.hpp>
#include <iostream>

namespace {

// Collects entries from a directory iterator, skipping (and reporting) any
// entry that cannot be accessed. Works for both directory_iterator and
// recursive_directory_iterator.
template <typename Iterator>
void Collect(Iterator it, std::vector<fs::path>& entries) {
  std::error_code ec;
  const Iterator end;
  while (it != end) {
    entries.push_back(it->path());
    it.increment(ec);
    if (ec) {
      std::cerr << "Skipping inaccessible entry: " << ec.message() << std::endl;
      ec.clear();
    }
  }
}

}  // namespace

std::vector<fs::path> FileManager::GetEntries(const fs::path& path, bool recursive) {
  std::vector<fs::path> entries;
  std::error_code ec;

  if (recursive) {
    fs::recursive_directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
      return entries;
    }
    Collect(std::move(it), entries);
  } else {
    fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
      return entries;
    }
    Collect(std::move(it), entries);
  }
  return entries;
}

std::uintmax_t FileManager::RemovePath(const fs::path& path) {
  std::error_code ec;
  std::uintmax_t const removed = fs::remove_all(path, ec);
  if (ec) {
    std::cerr << "Error removing " << path << ": " << ec.message() << std::endl;
    return 0;
  }
  return removed;
}

bool FileManager::IsDirectory(const fs::path& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

bool FileManager::Exists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}
