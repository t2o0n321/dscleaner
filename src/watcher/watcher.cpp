#include <atomic>
#include <chrono>
#include <iostream>
#include <thread>
#include <utility>
#include <watcher.hpp>

namespace {
std::atomic<bool> g_stop{false};
}  // namespace

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>

namespace {

CFRunLoopRef g_run_loop = nullptr;

void FsEventsCallback(ConstFSEventStreamRef /*stream*/, void* client_info, size_t num_events,
                      void* event_paths, const FSEventStreamEventFlags* /*flags*/,
                      const FSEventStreamEventId* /*ids*/) {
  auto* self = static_cast<Watcher*>(client_info);
  auto** paths = static_cast<char**>(event_paths);
  for (size_t i = 0; i < num_events; i++) {
    self->CleanPath(fs::path(paths[i]));
  }
}

}  // namespace
#endif

Watcher::Watcher(std::vector<fs::path> paths, bool recursive, bool verbose)
    : paths_(std::move(paths)), recursive_(recursive), verbose_(verbose) {}

void Watcher::CleanPath(const fs::path& path) {
  std::error_code ec;
  // FSEvents may hand us either the changed file or its containing directory;
  // normalise to a directory and do a shallow sweep of it.
  fs::path const dir = fs::is_directory(path, ec) ? path : path.parent_path();
  int const n = cleaner_.Clean(dir, false, false);
  if (n > 0 && verbose_) {
    std::cout << "[watch] cleaned " << n << " junk item(s) in " << dir.string() << std::endl;
  }
}

void Watcher::Run() {
  // Initial sweep so anything already sitting on the volume is dealt with.
  for (const auto& path : paths_) {
    int const n = cleaner_.Clean(path, recursive_, false);
    if (n > 0 && verbose_) {
      std::cout << "[watch] initial sweep removed " << n << " junk item(s) under " << path.string()
                << std::endl;
    }
  }

#ifdef __APPLE__
  CFMutableArrayRef cf_paths =
      CFArrayCreateMutable(nullptr, static_cast<CFIndex>(paths_.size()), &kCFTypeArrayCallBacks);
  for (const auto& path : paths_) {
    CFStringRef s =
        CFStringCreateWithCString(nullptr, fs::absolute(path).c_str(), kCFStringEncodingUTF8);
    CFArrayAppendValue(cf_paths, s);
    CFRelease(s);
  }

  FSEventStreamContext ctx{0, this, nullptr, nullptr, nullptr};
  FSEventStreamRef stream = FSEventStreamCreate(
      nullptr, &FsEventsCallback, &ctx, cf_paths, kFSEventStreamEventIdSinceNow,
      /*latency=*/0.5, kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
  CFRelease(cf_paths);

  g_run_loop = CFRunLoopGetCurrent();
  FSEventStreamScheduleWithRunLoop(stream, g_run_loop, kCFRunLoopDefaultMode);
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
    for (const auto& path : paths_) {
      int const n = cleaner_.Clean(path, recursive_, false);
      if (n > 0 && verbose_) {
        std::cout << "[watch] cleaned " << n << " junk item(s) under " << path.string()
                  << std::endl;
      }
    }
    // Sleep in small slices so Ctrl-C stays responsive.
    for (int i = 0; i < 20 && !g_stop.load(); i++) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
  }
#endif
  std::cout << "[watch] stopped." << std::endl;
}

void Watcher::RequestStop() {
  g_stop.store(true);
#ifdef __APPLE__
  if (g_run_loop) {
    CFRunLoopStop(g_run_loop);
  }
#endif
}
