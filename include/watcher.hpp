#pragma once

#include <chrono>
#include <cleaner.hpp>
#include <cstddef>
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

  // Handles one batch of FSEvents paths. The affected directories are
  // de-duplicated so that a burst of file events in the same folder (e.g. one
  // large drag-and-drop) triggers a single shallow sweep per folder rather than
  // one per file. `paths` is the C array FSEvents hands the callback.
  void OnEventBatch(const char* const* paths, std::size_t count);

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
