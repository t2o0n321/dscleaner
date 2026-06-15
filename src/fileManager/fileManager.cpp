#include <fileManager.hpp>
#include <iostream>

std::string_view FileManager::FileName(const fs::path& path) {
  // Reference into the path's own storage - no copy (POSIX std::string native).
  const std::string& native = path.native();
  const std::size_t slash = native.find_last_of('/');
  if (slash == std::string::npos) {
    return native;
  }
  return std::string_view(native).substr(slash + 1);
}

std::uintmax_t FileManager::RemovePath(const fs::path& path) {
  std::error_code ec;
  const std::uintmax_t removed = fs::remove_all(path, ec);
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
