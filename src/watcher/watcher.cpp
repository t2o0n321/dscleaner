#include <atomic>
#include <chrono>
#include <iostream>
#include <proc.hpp>
#include <string>
#include <thread>
#include <unordered_set>
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
  // event_paths is a C array of NUL-terminated paths (default, non-CFTypes API).
  self->OnEventBatch(static_cast<const char* const*>(event_paths), num_events);
}

}  // namespace
#endif

Watcher::Watcher(std::vector<fs::path> paths, bool recursive, bool verbose)
    : paths_(std::move(paths)), recursive_(recursive), verbose_(verbose) {}

void Watcher::Notify(int count) {
#ifdef __APPLE__
  pending_notify_ += count;
  const auto now = std::chrono::steady_clock::now();
  // Throttle so a burst of Finder events (e.g. one big drag-and-drop) produces
  // a single notification rather than one per file.
  if (now - last_notify_ < std::chrono::seconds(3)) {
    return;
  }
  last_notify_ = now;
  const std::string message = "Cleaned " + std::to_string(pending_notify_) + " macOS junk item(s)";
  pending_notify_ = 0;
  proc::Run(
      {"osascript", "-e", "display notification \"" + message + "\" with title \"dscleaner\""});
#else
  (void)count;
#endif
}

void Watcher::OnEventBatch(const char* const* paths, std::size_t count) {
  // De-duplicate the directories touched by this batch. FSEvents (with file
  // events) reports one path per changed file, and a single drag-and-drop can
  // change many files in the same folder; cleaning each unique folder once
  // turns potential O(files^2) re-scanning into O(files).
  std::unordered_set<std::string> dirs;
  dirs.reserve(count);
  for (std::size_t i = 0; i < count; i++) {
    const fs::path path(paths[i]);
    std::error_code ec;
    // Normalise to a directory: the event path is usually the changed file, so
    // we sweep its parent; if it is itself a directory, sweep that.
    const fs::path dir = fs::is_directory(path, ec) ? path : path.parent_path();
    dirs.insert(dir.string());
  }

  int total = 0;
  for (const auto& dir : dirs) {
    const int n = cleaner_.Clean(fs::path(dir), false, false);
    if (n > 0) {
      total += n;
      if (verbose_) {
        std::cout << "[watch] cleaned " << n << " junk item(s) in " << dir << std::endl;
      }
    }
  }
  if (total > 0) {
    Notify(total);
  }
}

void Watcher::Run() {
  // Initial sweep so anything already sitting on the volume is dealt with.
  for (const auto& path : paths_) {
    int const n = cleaner_.Clean(path, recursive_, false);
    if (n > 0) {
      if (verbose_) {
        std::cout << "[watch] initial sweep removed " << n << " junk item(s) under "
                  << path.string() << std::endl;
      }
      Notify(n);
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
      if (n > 0) {
        if (verbose_) {
          std::cout << "[watch] cleaned " << n << " junk item(s) under " << path.string()
                    << std::endl;
        }
        Notify(n);
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
