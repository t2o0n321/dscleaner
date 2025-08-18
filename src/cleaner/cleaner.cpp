#include <cleaner.hpp>
#include <iostream>

void Cleaner::clean(const fs::path& targetDir, bool recursive) {
    if(recursive) {
        std::cout << "Starting cleanup in: " << fs::absolute(targetDir).string() << " and all its subfolders" << std::endl;
    } else {
        std::cout << "Starting cleanup in: " << fs::absolute(targetDir).string() << std::endl;
    }
    auto files = fileManager::getFiles(targetDir, recursive);
    int count = 0;
    for (const auto& file : files) {
        if (file.filename() == ".DS_Store") {
            if (fileManager::removeFile(file)) {
                std::cout << "Removed: " << file.string() << std::endl;
                count++;
            }
        }
    }
    std::cout << "Removed " << count << " .DS_Store file(s)." << std::endl;
}
