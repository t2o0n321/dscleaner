#pragma once

#include <fileManager.hpp>

class Cleaner {
public:
    void clean(const fs::path& targetDir, bool recursive);
};