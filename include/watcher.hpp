#pragma once

#include <chrono>
#include <cleaner.hpp>
#include <vector>

// Background directory watcher. On macOS it uses native FSEvents for real-time
// notification; on other platforms it falls back to periodic polling so the
// code still builds and runs (e.g. in CI). Either way: when junk appears under
// a watched path it is removed automatically.
class Watcher {
 public:
  Watcher(std::vector<fs::path> paths, bool recursive, bool verbose);

  // Blocks, watching until RequestStop() is called (typically from a signal
  // handler).
  void Run();

  // Sweeps a single directory (the one an event fired on) for junk.
  void CleanPath(const fs::path& path);

  // Asks a running watcher to stop. Safe to call from a signal handler.
  static void RequestStop();

 private:
  // Posts a throttled desktop notification (macOS only) reporting that `count`
  // junk items were auto-cleaned. This is the drag-and-drop "completion report":
  // a Finder copy onto a watched volume is cleaned silently otherwise.
  void Notify(int count);

  std::vector<fs::path> paths_;
  bool recursive_;
  bool verbose_;
  Cleaner cleaner_;
  std::chrono::steady_clock::time_point last_notify_{};
  int pending_notify_ = 0;
};
