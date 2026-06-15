#pragma once

#include <string>
#include <vector>

// A snapshot of the service's current state, consumed by the `status` command
// (and therefore by the menu bar GUI).
struct ServiceStatus {
  bool installed = false;                // the launchd plist exists
  bool running = false;                  // launchctl reports the agent loaded (macOS)
  std::string label;                     // launchd label
  std::string plist;                     // plist path
  std::string out_log;                   // stdout log path
  std::string err_log;                   // stderr log path
  std::vector<std::string> watch_paths;  // paths the agent watches
};

// Registers / unregisters dscleaner as a background system service that keeps
// watched paths free of macOS junk.
//
// On macOS this manages a per-user launchd LaunchAgent
// (~/Library/LaunchAgents/com.t2o0n321.dscleaner.plist) running
// `dscleaner watch <paths...>`. On other platforms it generates the plist for
// inspection and explains that launchd is macOS-only.
class Service {
 public:
  // `paths` are the directories to watch. If empty, a sensible default is
  // chosen (/Volumes on macOS so freshly-mounted USB drives are covered).
  static int Install(const std::vector<std::string>& paths);
  static int Uninstall();

  // Returns the current service state for the `status` command / GUI.
  static ServiceStatus Query();
};
