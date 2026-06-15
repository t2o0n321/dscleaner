#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <proc.hpp>
#include <service.hpp>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kLabel = "com.t2o0n321.dscleaner";

// Absolute path to the currently running executable.
fs::path ExecutablePath() {
  std::error_code ec;
#ifdef __APPLE__
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size);
  std::string buf(size, '\0');
  if (_NSGetExecutablePath(buf.data(), &size) != 0) {
    return fs::path(buf.c_str());
  }
  fs::path canonical = fs::canonical(fs::path(buf.c_str()), ec);
  return ec ? fs::path(buf.c_str()) : canonical;
#else
  fs::path const canonical = fs::canonical("/proc/self/exe", ec);
  return ec ? fs::path("dscleaner") : canonical;
#endif
}

std::string HomeDir() {
  const char* home = std::getenv("HOME");
  return home ? std::string(home) : std::string(".");
}

fs::path PlistPath() {
#ifdef __APPLE__
  return fs::path(HomeDir()) / "Library" / "LaunchAgents" / (std::string(kLabel) + ".plist");
#else
  return fs::path(HomeDir()) / (std::string(kLabel) + ".plist");
#endif
}

// Default watch paths when the user does not specify any.
std::vector<std::string> DefaultPaths() {
#ifdef __APPLE__
  return {"/Volumes"};
#else
  return {"."};
#endif
}

std::string XmlEscape(const std::string& in) {
  std::string out;
  for (char const c : in) {
    switch (c) {
      case '&':
        out += "&amp;";
        break;
      case '<':
        out += "&lt;";
        break;
      case '>':
        out += "&gt;";
        break;
      default:
        out += c;
        break;
    }
  }
  return out;
}

std::string OutLogPath() { return std::string("/tmp/") + kLabel + ".out.log"; }
std::string ErrLogPath() { return std::string("/tmp/") + kLabel + ".err.log"; }

// Extracts the watch paths from a plist we previously wrote. ProgramArguments is
// [exe, "watch", path...], so we collect the <string> values inside that array
// and drop everything up to and including the "watch" marker. This keeps the
// launchd plist as the single source of truth (no separate config file).
std::vector<std::string> ParseWatchPaths(const fs::path& plist) {
  std::vector<std::string> result;
  std::ifstream in(plist);
  if (!in) {
    return result;
  }
  const std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());

  const std::size_t pos = content.find("<key>ProgramArguments</key>");
  if (pos == std::string::npos) {
    return result;
  }
  const std::size_t array_end = content.find("</array>", pos);
  const std::string open = "<string>";
  const std::string close = "</string>";
  bool seen_watch = false;
  for (std::size_t s = content.find(open, pos);
       s != std::string::npos && (array_end == std::string::npos || s < array_end);
       s = content.find(open, s + 1)) {
    const std::size_t value_start = s + open.size();
    const std::size_t value_end = content.find(close, value_start);
    if (value_end == std::string::npos) {
      break;
    }
    const std::string value = content.substr(value_start, value_end - value_start);
    if (seen_watch) {
      result.push_back(value);
    } else if (value == "watch") {
      seen_watch = true;
    }
    s = value_end;
  }
  return result;
}

std::string BuildPlist(const fs::path& exe, const std::vector<std::string>& paths) {
  std::string args = "    <string>" + XmlEscape(exe.string()) + "</string>\n";
  args += "    <string>watch</string>\n";
  for (const auto& path : paths) {
    std::error_code ec;
    fs::path const abs = fs::absolute(path, ec);
    args += "    <string>" + XmlEscape(ec ? path : abs.string()) + "</string>\n";
  }

  std::string plist;
  plist += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
  plist +=
      "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
      "\"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n";
  plist += "<plist version=\"1.0\">\n";
  plist += "<dict>\n";
  plist += "  <key>Label</key>\n";
  plist += "  <string>" + std::string(kLabel) + "</string>\n";
  plist += "  <key>ProgramArguments</key>\n";
  plist += "  <array>\n";
  plist += args;
  plist += "  </array>\n";
  plist += "  <key>RunAtLoad</key>\n  <true/>\n";
  plist += "  <key>KeepAlive</key>\n  <true/>\n";
  plist += "  <key>StandardOutPath</key>\n";
  plist += "  <string>/tmp/" + std::string(kLabel) + ".out.log</string>\n";
  plist += "  <key>StandardErrorPath</key>\n";
  plist += "  <string>/tmp/" + std::string(kLabel) + ".err.log</string>\n";
  plist += "</dict>\n";
  plist += "</plist>\n";
  return plist;
}

}  // namespace

int Service::Install(const std::vector<std::string>& paths) {
  std::vector<std::string> const watch_paths = paths.empty() ? DefaultPaths() : paths;
  fs::path const exe = ExecutablePath();
  fs::path const plist = PlistPath();

  std::error_code ec;
  fs::create_directories(plist.parent_path(), ec);

  std::ofstream out(plist);
  if (!out) {
    std::cerr << "Error: cannot write plist to " << plist.string() << std::endl;
    return 1;
  }
  out << BuildPlist(exe, watch_paths);
  out.close();
  std::cout << "Wrote service definition: " << plist.string() << std::endl;

#ifdef __APPLE__
  // Reload cleanly: unload any previous instance, then load.
  proc::Run({"launchctl", "unload", plist.string()});
  int rc = proc::Run({"launchctl", "load", "-w", plist.string()});
  if (rc != 0) {
    std::cerr << "launchctl load failed (exit " << rc << ")." << std::endl;
    return rc;
  }
  std::cout << "Service '" << kLabel << "' installed and started." << std::endl;
  std::cout << "Watching:";
  for (const auto& path : watch_paths) {
    std::cout << " " << path;
  }
  std::cout << std::endl;
  std::cout << "Logs: /tmp/" << kLabel << ".out.log / .err.log" << std::endl;
  return 0;
#else
  std::cout << "Note: launchd is macOS-only. The plist above is for reference; "
               "on this platform run `dscleaner watch` directly (e.g. under "
               "systemd/supervisor)."
            << std::endl;
  return 0;
#endif
}

int Service::Uninstall() {
  fs::path const plist = PlistPath();
  std::error_code ec;

#ifdef __APPLE__
  if (fs::exists(plist, ec)) {
    proc::Run({"launchctl", "unload", "-w", plist.string()});
  }
#endif

  if (fs::exists(plist, ec)) {
    fs::remove(plist, ec);
    if (ec) {
      std::cerr << "Error removing plist: " << ec.message() << std::endl;
      return 1;
    }
    std::cout << "Service '" << kLabel << "' uninstalled (" << plist.string() << " removed)."
              << std::endl;
  } else {
    std::cout << "Service '" << kLabel << "' is not installed." << std::endl;
  }
  return 0;
}

ServiceStatus Service::Query() {
  ServiceStatus status;
  status.label = kLabel;
  const fs::path plist = PlistPath();
  status.plist = plist.string();
  status.out_log = OutLogPath();
  status.err_log = ErrLogPath();

  std::error_code ec;
  status.installed = fs::exists(plist, ec);
  if (status.installed) {
    status.watch_paths = ParseWatchPaths(plist);
  }

#ifdef __APPLE__
  // `launchctl list <label>` exits 0 when the agent is loaded. Run it quietly.
  status.running =
      proc::Run({"sh", "-c", "launchctl list " + std::string(kLabel) + " >/dev/null 2>&1"}) == 0;
#else
  // No launchd elsewhere: treat "installed" as the best available signal.
  status.running = status.installed;
#endif
  return status;
}
