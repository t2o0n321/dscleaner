#include <cleaner.hpp>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char* argv[]) {
    fs::path target_dir = ".";
    bool recursive = true;
    std::vector<std::string> args(argv + 1, argv + argc);

    for (const auto& arg : args) {
        if (arg == "-n" || arg == "--no-recursive") {
            recursive = false;
        } else {
            target_dir = arg;
        }
    }

    if (!fileManager::exists(target_dir)) {
        std::cerr << "Error: Path does not exist: " << target_dir.string() << std::endl;
        return 1;
    }

    if (!fileManager::isDirectory(target_dir)) {
        std::cerr << "Error: Path is not a directory: " << target_dir.string() << std::endl;
        return 1;
    }

    Cleaner cleaner;
    cleaner.clean(target_dir, recursive);

    return 0;
}
