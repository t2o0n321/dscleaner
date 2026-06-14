#include <watcher.hpp>
#include <junk.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>

namespace {
std::atomic<bool> g_stop{false};
}

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>

namespace {
CFRunLoopRef g_runLoop = nullptr;

void fsEventsCallback(ConstFSEventStreamRef /*stream*/, void* clientInfo, size_t numEvents,
                      void* eventPaths, const FSEventStreamEventFlags* /*flags*/,
                      const FSEventStreamEventId* /*ids*/) {
    auto* self = static_cast<Watcher*>(clientInfo);
    auto** paths = static_cast<char**>(eventPaths);
    for (size_t i = 0; i < numEvents; i++) {
        self->cleanPath(fs::path(paths[i]));
    }
}
} // namespace
#endif

Watcher::Watcher(std::vector<fs::path> paths, bool recursive, bool verbose)
    : paths_(std::move(paths)), recursive_(recursive), verbose_(verbose) {}

void Watcher::cleanPath(const fs::path& path) {
    std::error_code ec;
    // FSEvents may hand us either the changed file or its containing directory;
    // normalise to a directory and do a shallow sweep of it.
    fs::path dir = fs::is_directory(path, ec) ? path : path.parent_path();
    int n = cleaner_.clean(dir, false, false);
    if (n > 0 && verbose_) {
        std::cout << "[watch] cleaned " << n << " junk item(s) in " << dir.string() << std::endl;
    }
}

void Watcher::run() {
    // Initial sweep so anything already sitting on the volume is dealt with.
    for (const auto& p : paths_) {
        int n = cleaner_.clean(p, recursive_, false);
        if (n > 0 && verbose_) {
            std::cout << "[watch] initial sweep removed " << n << " junk item(s) under "
                      << p.string() << std::endl;
        }
    }

#ifdef __APPLE__
    CFMutableArrayRef cfPaths =
        CFArrayCreateMutable(nullptr, static_cast<CFIndex>(paths_.size()), &kCFTypeArrayCallBacks);
    for (const auto& p : paths_) {
        CFStringRef s = CFStringCreateWithCString(nullptr, fs::absolute(p).c_str(),
                                                  kCFStringEncodingUTF8);
        CFArrayAppendValue(cfPaths, s);
        CFRelease(s);
    }

    FSEventStreamContext ctx{0, this, nullptr, nullptr, nullptr};
    FSEventStreamRef stream = FSEventStreamCreate(
        nullptr, &fsEventsCallback, &ctx, cfPaths, kFSEventStreamEventIdSinceNow,
        /*latency=*/0.5,
        kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
    CFRelease(cfPaths);

    g_runLoop = CFRunLoopGetCurrent();
    FSEventStreamScheduleWithRunLoop(stream, g_runLoop, kCFRunLoopDefaultMode);
    FSEventStreamStart(stream);

    std::cout << "[watch] monitoring " << paths_.size()
              << " path(s) via FSEvents. Press Ctrl-C to stop." << std::endl;

    CFRunLoopRun();

    FSEventStreamStop(stream);
    FSEventStreamInvalidate(stream);
    FSEventStreamRelease(stream);
#else
    std::cout << "[watch] monitoring " << paths_.size()
              << " path(s) via polling (every 2s). Press Ctrl-C to stop." << std::endl;
    while (!g_stop.load()) {
        for (const auto& p : paths_) {
            int n = cleaner_.clean(p, recursive_, false);
            if (n > 0 && verbose_) {
                std::cout << "[watch] cleaned " << n << " junk item(s) under " << p.string()
                          << std::endl;
            }
        }
        // Sleep in small slices so Ctrl-C is responsive.
        for (int i = 0; i < 20 && !g_stop.load(); i++) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
#endif
    std::cout << "[watch] stopped." << std::endl;
}

void Watcher::requestStop() {
    g_stop.store(true);
#ifdef __APPLE__
    if (g_runLoop) {
        CFRunLoopStop(g_runLoop);
    }
#endif
}
