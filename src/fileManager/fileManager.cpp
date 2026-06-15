#include <fileManager.hpp>
#include <string>

std::string_view FileManager::FileName(const fs::path& path) {
  // native() is the path's own backing string (no copy on POSIX). The filename
  // is the substring after the last '/', or the whole string if there is none.
  // Because it runs to the end of native(), the returned view is NUL-terminated.
  const std::string& native = path.native();
  const std::size_t slash = native.find_last_of('/');
  if (slash == std::string::npos) {
    return native;
  }
  return std::string_view(native).substr(slash + 1);
}

bool FileManager::IsDirectory(const fs::path& path) {
  std::error_code ec;
  return fs::is_directory(path, ec);
}

bool FileManager::Exists(const fs::path& path) {
  std::error_code ec;
  return fs::exists(path, ec);
}
