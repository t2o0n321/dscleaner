#include <fileManager.hpp>

bool FileManager::IsDirectory(const fs::path& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

bool FileManager::Exists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}
