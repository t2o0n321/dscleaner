#pragma once

#include <cleaner.hpp>
#include <vector>

// Background directory watcher. On macOS it uses native FSEvents for real-time
// notification; on other platforms it falls back to periodic polling so the
// code still builds and runs (e.g. in CI). Either way: when junk appears under
// a watched path it is removed automatically.
class Watcher {
public:
    Watcher(std::vector<fs::path> paths, bool recursive, bool verbose);

    // Blocks, watching until requestStop() is called (typically from a signal
    // handler).
    void run();

    // Sweep a single directory (the one an event fired on) for junk.
    void cleanPath(const fs::path& path);

    // Ask a running watcher to stop. Safe to call from a signal handler.
    static void requestStop();

private:
    std::vector<fs::path> paths_;
    bool recursive_;
    bool verbose_;
    Cleaner cleaner_;
};
