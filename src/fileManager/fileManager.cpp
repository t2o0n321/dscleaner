#include <fileManager.hpp>
#include <iostream>

std::vector<fs::path> fileManager::getFiles(const fs::path& path, bool recursive) {
    std::vector<fs::path> files;
    for (const auto& entry : getEntries(path, recursive)) {
        std::error_code ec;
        if (fs::is_regular_file(entry, ec)) {
            files.push_back(entry);
        }
    }
    return files;
}

std::vector<fs::path> fileManager::getEntries(const fs::path& path, bool recursive) {
    std::vector<fs::path> entries;
    std::error_code ec;

    if (recursive) {
        fs::recursive_directory_iterator it(
            path, fs::directory_options::skip_permission_denied, ec);
        const fs::recursive_directory_iterator end;
        if (ec) {
            std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
            return entries;
        }
        while (it != end) {
            entries.push_back(it->path());
            it.increment(ec);
            if (ec) {
                // Skip the offending entry and keep going.
                std::cerr << "Skipping inaccessible entry: " << ec.message() << std::endl;
                ec.clear();
            }
        }
    } else {
        fs::directory_iterator it(path, fs::directory_options::skip_permission_denied, ec);
        const fs::directory_iterator end;
        if (ec) {
            std::cerr << "Error accessing path " << path << ": " << ec.message() << std::endl;
            return entries;
        }
        while (it != end) {
            entries.push_back(it->path());
            it.increment(ec);
            if (ec) {
                std::cerr << "Skipping inaccessible entry: " << ec.message() << std::endl;
                ec.clear();
            }
        }
    }
    return entries;
}

bool fileManager::removeFile(const fs::path& path) {
    try {
        return fs::remove(path);
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Error removing file " << path << ": " << e.what() << std::endl;
        return false;
    }
}

std::uintmax_t fileManager::removePath(const fs::path& path) {
    std::error_code ec;
    std::uintmax_t removed = fs::remove_all(path, ec);
    if (ec) {
        std::cerr << "Error removing " << path << ": " << ec.message() << std::endl;
        return 0;
    }
    return removed;
}

bool fileManager::isDirectory(const fs::path& path) {
    return fs::is_directory(path);
}

bool fileManager::exists(const fs::path& path) {
    return fs::exists(path);
}
