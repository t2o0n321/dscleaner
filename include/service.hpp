#pragma once

#include <string>
#include <vector>

// Register / unregister dscleaner as a background system service that keeps
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
    static int install(const std::vector<std::string>& paths);
    static int uninstall();
};
