#include <cleaner.hpp>
#include <junk.hpp>
#include <iostream>

int Cleaner::clean(const fs::path& targetDir, bool recursive, bool verbose) {
    if (verbose) {
        if (recursive) {
            std::cout << "Starting cleanup in: " << fs::absolute(targetDir).string()
                      << " and all its subfolders" << std::endl;
        } else {
            std::cout << "Starting cleanup in: " << fs::absolute(targetDir).string() << std::endl;
        }
    }

    auto entries = fileManager::getEntries(targetDir, recursive);
    int count = 0;
    for (const auto& entry : entries) {
        if (!junk::isJunk(entry.filename().string())) {
            continue;
        }
        // A junk directory may have already been removed as part of an earlier
        // junk parent; skip if it is gone.
        std::error_code ec;
        if (!fs::exists(entry, ec)) {
            continue;
        }
        if (fileManager::removePath(entry) > 0) {
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
