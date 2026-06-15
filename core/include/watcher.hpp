#pragma once

#include <chrono>
#include <cleaner.hpp>
#include <cstddef>
#include <vector>

// Background directory watcher. On macOS it uses native FSEvents for real-time
// notification; on other platforms it falls back to periodic polling so the
// code still builds and runs (e.g. in CI). Either way: when junk appears under
// a watched path it is removed automatically.
//
// Removable-drive support: a watched path may be a "mount root" (default
// /Volumes on macOS). FSEvents is per-volume, so watching /Volumes alone does
// not see *inside* a freshly mounted USB stick. The watcher therefore detects
// volumes mounting/unmounting under its mount roots and dynamically watches
// each mounted volume - so inserting a USB drive automatically starts cleaning
// it. Extra mount roots can be supplied via the DSCLEANER_MOUNT_ROOTS
// environment variable (colon-separated; useful for Linux's /media).
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

  // Detects volumes mounting/unmounting under the mount roots. Newly mounted
  // volumes are swept immediately; on a change the active set is updated and the
  // FSEvents stream is rebuilt (no-op on non-macOS). Returns true if it changed.
  // Public because the macOS DiskArbitration callbacks invoke it directly.
  bool Reconcile();

  // Asks a running watcher to stop. Safe to call from a signal handler.
  static void RequestStop();

 private:
  // Posts a throttled desktop notification (macOS only) reporting that `count`
  // junk items were auto-cleaned. This is the drag-and-drop / USB-insert
  // "completion report": an auto-clean is silent otherwise.
  void Notify(int count);

  // Recursively cleans `path` once, logging / notifying if anything was removed.
  void SweepOnce(const fs::path& path);

  // Immediate child directories (mounted volumes) of every mount root, skipping
  // symlinks so the boot volume's /Volumes entry is ignored. Sorted.
  std::vector<fs::path> EnumerateVolumes() const;

  // direct paths + mount roots + currently-mounted volumes: everything to watch
  // right now.
  std::vector<fs::path> ActivePaths() const;

  // Rebuilds the FSEvents stream for the current ActivePaths() (macOS only).
  void RestartStream();

  std::vector<fs::path> direct_paths_;  // watched as-is
  std::vector<fs::path> mount_roots_;   // their child volumes are auto-watched
  std::vector<fs::path> volumes_;       // currently-mounted volumes (dynamic, sorted)
  bool recursive_;
  bool verbose_;
  bool notifications_ = true;  // post macOS notifications (DSCLEANER_NOTIFICATIONS)
  Cleaner cleaner_;
  std::chrono::steady_clock::time_point last_notify_{};
  int pending_notify_ = 0;
};
