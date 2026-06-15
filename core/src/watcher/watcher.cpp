#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <proc.hpp>
#include <set>
#include <string>
#include <thread>
#include <unordered_set>
#include <utility>
#include <watcher.hpp>

namespace {

std::atomic<bool> g_stop{false};

// Notifications are on unless DSCLEANER_NOTIFICATIONS is set to a falsey value.
// The service plist sets this variable, so the GUI's toggle controls it.
bool NotificationsEnabledFromEnv() {
  const char* env = std::getenv("DSCLEANER_NOTIFICATIONS");
  if (env == nullptr) {
    return true;
  }
  const std::string value(env);
  return !(value == "0" || value == "false" || value == "no" || value == "off");
}

// Strips trailing slashes so "/Volumes/" and "/Volumes" compare equal.
std::string NormalizeKey(const fs::path& path) {
  std::string key = path.string();
  while (key.size() > 1 && key.back() == '/') {
    key.pop_back();
  }
  return key;
}

// The set of paths whose child directories are treated as mounted volumes:
// /Volumes on macOS, plus anything in DSCLEANER_MOUNT_ROOTS (colon-separated).
std::set<std::string> MountRootSet() {
  std::set<std::string> roots;
#ifdef __APPLE__
  roots.insert("/Volumes");
#endif
  if (const char* env = std::getenv("DSCLEANER_MOUNT_ROOTS")) {
    const std::string value(env);
    std::size_t start = 0;
    while (start <= value.size()) {
      const std::size_t colon = value.find(':', start);
      const std::size_t end = (colon == std::string::npos) ? value.size() : colon;
      if (end > start) {
        roots.insert(NormalizeKey(value.substr(start, end - start)));
      }
      if (colon == std::string::npos) {
        break;
      }
      start = colon + 1;
    }
  }
  return roots;
}

}  // namespace

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <DiskArbitration/DiskArbitration.h>

namespace {

CFRunLoopRef g_run_loop = nullptr;
FSEventStreamRef g_stream = nullptr;
DASessionRef g_da_session = nullptr;

void FsEventsCallback(ConstFSEventStreamRef /*stream*/, void* client_info, size_t num_events,
                      void* event_paths, const FSEventStreamEventFlags* /*flags*/,
                      const FSEventStreamEventId* /*ids*/) {
  auto* self = static_cast<Watcher*>(client_info);
  // event_paths is a C array of NUL-terminated paths (default, non-CFTypes API).
  self->OnEventBatch(static_cast<const char* const*>(event_paths), num_events);
}

// Creates and starts an FSEvents stream over `paths`, owned by g_stream.
void StartStreamFor(Watcher* self, const std::vector<fs::path>& paths) {
  if (paths.empty()) {
    return;
  }
  CFMutableArrayRef cf_paths =
      CFArrayCreateMutable(nullptr, static_cast<CFIndex>(paths.size()), &kCFTypeArrayCallBacks);
  for (const auto& path : paths) {
    CFStringRef s =
        CFStringCreateWithCString(nullptr, fs::absolute(path).c_str(), kCFStringEncodingUTF8);
    CFArrayAppendValue(cf_paths, s);
    CFRelease(s);
  }
  FSEventStreamContext ctx{0, self, nullptr, nullptr, nullptr};
  g_stream = FSEventStreamCreate(
      nullptr, &FsEventsCallback, &ctx, cf_paths, kFSEventStreamEventIdSinceNow,
      /*latency=*/0.5, kFSEventStreamCreateFlagFileEvents | kFSEventStreamCreateFlagNoDefer);
  CFRelease(cf_paths);
  FSEventStreamScheduleWithRunLoop(g_stream, g_run_loop, kCFRunLoopDefaultMode);
  FSEventStreamStart(g_stream);
}

void StopStreamNow() {
  if (g_stream != nullptr) {
    FSEventStreamStop(g_stream);
    FSEventStreamInvalidate(g_stream);
    FSEventStreamRelease(g_stream);
    g_stream = nullptr;
  }
}

// DiskArbitration delivers hardware mount/unmount events with no polling. All
// three callbacks funnel into Reconcile(), which re-lists the mount roots,
// sweeps any newly mounted volume and rebuilds the FSEvents stream. Reconcile()
// is idempotent and cheap, so reacting to every event is fine.
void DiskAppearedCallback(DADiskRef /*disk*/, void* info) {
  static_cast<Watcher*>(info)->Reconcile();
}

void DiskDisappearedCallback(DADiskRef /*disk*/, void* info) {
  static_cast<Watcher*>(info)->Reconcile();
}

// Fires when a disk's mount path is set or cleared - the precise "mounted at
// /Volumes/X" / "unmounted" signal.
void DiskDescriptionChangedCallback(DADiskRef /*disk*/, CFArrayRef /*keys*/, void* info) {
  static_cast<Watcher*>(info)->Reconcile();
}

// Registers a DiskArbitration session on the run loop so mount/unmount events
// trigger Reconcile() instantly (no timer / polling).
void StartDiskArbitration(Watcher* self) {
  g_da_session = DASessionCreate(kCFAllocatorDefault);
  if (g_da_session == nullptr) {
    return;
  }
  DARegisterDiskAppearedCallback(g_da_session, nullptr, &DiskAppearedCallback, self);
  DARegisterDiskDisappearedCallback(g_da_session, nullptr, &DiskDisappearedCallback, self);

  // Watch the volume-path key so we hear about mounts and unmounts specifically.
  const void* keys[] = {kDADiskDescriptionVolumePathKey};
  CFArrayRef watch = CFArrayCreate(nullptr, keys, 1, &kCFTypeArrayCallBacks);
  DARegisterDiskDescriptionChangedCallback(g_da_session, nullptr, watch,
                                           &DiskDescriptionChangedCallback, self);
  CFRelease(watch);

  DASessionScheduleWithRunLoop(g_da_session, g_run_loop, kCFRunLoopDefaultMode);
}

void StopDiskArbitration() {
  if (g_da_session != nullptr) {
    DASessionUnscheduleFromRunLoop(g_da_session, g_run_loop, kCFRunLoopDefaultMode);
    CFRelease(g_da_session);
    g_da_session = nullptr;
  }
}

}  // namespace
#endif  // __APPLE__

Watcher::Watcher(std::vector<fs::path> paths, bool recursive, bool verbose)
    : recursive_(recursive), verbose_(verbose), notifications_(NotificationsEnabledFromEnv()) {
  // Split the requested paths into plain directories and mount roots (whose
  // child volumes are auto-watched).
  const std::set<std::string> mount_root_set = MountRootSet();
  for (auto& path : paths) {
    if (mount_root_set.count(NormalizeKey(path)) != 0) {
      mount_roots_.push_back(std::move(path));
    } else {
      direct_paths_.push_back(std::move(path));
    }
  }
}

void Watcher::Notify(int count) {
  if (!notifications_) {
    return;
  }
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

void Watcher::SweepOnce(const fs::path& path) {
  const int n = cleaner_.Clean(path, recursive_, false);
  if (n > 0) {
    if (verbose_) {
      std::cout << "[watch] cleaned " << n << " junk item(s) under " << path.string() << std::endl;
    }
    Notify(n);
  }
}

std::vector<fs::path> Watcher::EnumerateVolumes() const {
  std::vector<fs::path> volumes;
  for (const auto& root : mount_roots_) {
    std::error_code ec;
    fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
      continue;
    }
    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
      if (ec) {
        break;
      }
      // Skip symlinks (the boot volume appears under /Volumes as a symlink to
      // "/"); only real mounted volumes are directories.
      if (it->is_symlink(ec) || !it->is_directory(ec)) {
        continue;
      }
      // Belt-and-braces: never treat the root/boot volume as a removable volume,
      // even if it is exposed under a mount root as a real directory.
      std::error_code eq_ec;
      if (fs::equivalent(it->path(), "/", eq_ec) && !eq_ec) {
        continue;
      }
      volumes.push_back(it->path());
    }
  }
  std::sort(volumes.begin(), volumes.end());
  return volumes;
}

std::vector<fs::path> Watcher::ActivePaths() const {
  std::vector<fs::path> active = direct_paths_;
  active.insert(active.end(), mount_roots_.begin(), mount_roots_.end());
  active.insert(active.end(), volumes_.begin(), volumes_.end());
  return active;
}

bool Watcher::Reconcile() {
  std::vector<fs::path> desired = EnumerateVolumes();
  const bool changed = (desired != volumes_);

  // Immediately sweep volumes that just appeared (a USB stick was inserted).
  for (const auto& volume : desired) {
    if (!std::binary_search(volumes_.begin(), volumes_.end(), volume)) {
      if (verbose_) {
        std::cout << "[watch] volume mounted: " << volume.string() << std::endl;
      }
      SweepOnce(volume);
    }
  }

  if (changed) {
    volumes_ = std::move(desired);
    RestartStream();  // no-op on non-macOS
  }
  return changed;
}

void Watcher::RestartStream() {
#ifdef __APPLE__
  StopStreamNow();
  StartStreamFor(this, ActivePaths());
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
  // Seed the volume set and sweep everything once up front.
  volumes_ = EnumerateVolumes();
  for (const auto& path : ActivePaths()) {
    SweepOnce(path);
  }

#ifdef __APPLE__
  g_run_loop = CFRunLoopGetCurrent();
  StartStreamFor(this, ActivePaths());

  // Event-driven mount detection: DiskArbitration notifies us the instant a
  // drive is inserted or removed (no polling). Each event triggers Reconcile().
  StartDiskArbitration(this);

  std::cout << "[watch] monitoring " << ActivePaths().size()
            << " path(s) via FSEvents + DiskArbitration (drives auto-watched on insert). "
               "Press Ctrl-C to stop."
            << std::endl;

  CFRunLoopRun();

  StopDiskArbitration();
  StopStreamNow();
#else
  std::cout << "[watch] monitoring via polling (every 2s, auto-watching mounted volumes). "
               "Press Ctrl-C to stop."
            << std::endl;
  while (!g_stop.load()) {
    Reconcile();  // detect & sweep newly mounted volumes
    for (const auto& path : ActivePaths()) {
      SweepOnce(path);
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
  if (g_run_loop != nullptr) {
    CFRunLoopStop(g_run_loop);
  }
#endif
}
