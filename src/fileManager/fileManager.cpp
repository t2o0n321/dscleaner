#include <fileManager.hpp>
#include <iostream>

std::vector<fs::path> fileManager::getFiles(const fs::path& path, bool recursive) {
    std::vector<fs::path> files;
    try {
        if (recursive) {
            for (const auto& entry : fs::recursive_directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }
        } else {
            for (const auto& entry : fs::directory_iterator(path)) {
                if (entry.is_regular_file()) {
                    files.push_back(entry.path());
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error accessing path " << path << ": " << e.what() << std::endl;
    }
    return files;
}

bool fileManager::removeFile(const fs::path& path) {
    try {
        return fs::remove(path);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error removing file " << path << ": " << e.what() << std::endl;
        return false;
    }
}

bool fileManager::isDirectory(const fs::path& path) {
    return fs::is_directory(path);
}

bool fileManager::exists(const fs::path& path) {
    return fs::exists(path);
}
