#include <junk.hpp>

namespace {

// Exact directory / file names that macOS creates as metadata.
const std::vector<std::string> kExactNames = {
    ".DS_Store",
    ".AppleDouble",
    ".AppleDB",
    ".AppleDesktop",
    ".Spotlight-V100",
    ".Trashes",
    ".fseventsd",
    ".TemporaryItems",
    ".DocumentRevisions-V100",
    ".apdisk",
    ".VolumeIcon.icns",
    ".com.apple.timemachine.donotpresent",
};

// Equivalent glob patterns for external tools (tar/rsync/zip).
const std::vector<std::string> kGlobs = {
    ".DS_Store",
    "._*",
    ".AppleDouble",
    ".AppleDB",
    ".AppleDesktop",
    ".Spotlight-V100",
    ".Trashes",
    ".fseventsd",
    ".TemporaryItems",
    ".DocumentRevisions-V100",
    ".apdisk",
    ".VolumeIcon.icns",
    ".com.apple.timemachine.donotpresent",
};

} // namespace

bool junk::isJunk(const std::string& filename) {
    // AppleDouble side-car files: "._<anything>". This is the family the user
    // specifically complained about leaking onto USB drives.
    if (filename.size() >= 2 && filename[0] == '.' && filename[1] == '_') {
        return true;
    }
    for (const auto& name : kExactNames) {
        if (filename == name) {
            return true;
        }
    }
    return false;
}

const std::vector<std::string>& junk::globPatterns() {
    return kGlobs;
}
