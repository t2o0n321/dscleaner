#pragma once

#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

class fileManager {
public:
    static std::vector<fs::path> getFiles(const fs::path& path, bool recursive);
    static bool removeFile(const fs::path& path);
    static bool isDirectory(const fs::path& path);
    static bool exists(const fs::path& path);
};