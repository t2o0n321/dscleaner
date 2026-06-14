#include <service.hpp>
#include <proc.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr const char* kLabel = "com.t2o0n321.dscleaner";

// Absolute path to the currently running executable.
fs::path executablePath() {
    std::error_code ec;
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string buf(size, '\0');
    if (_NSGetExecutablePath(buf.data(), &size) != 0) {
        return fs::path(buf.c_str());
    }
    fs::path p = fs::canonical(fs::path(buf.c_str()), ec);
    return ec ? fs::path(buf.c_str()) : p;
#else
    fs::path p = fs::canonical("/proc/self/exe", ec);
    return ec ? fs::path("dscleaner") : p;
#endif
}

std::string homeDir() {
    const char* home = std::getenv("HOME");
    return home ? std::string(home) : std::string(".");
}

fs::path plistPath() {
#ifdef __APPLE__
    return fs::path(homeDir()) / "Library" / "LaunchAgents" / (std::string(kLabel) + ".plist");
#else
    return fs::path(homeDir()) / (std::string(kLabel) + ".plist");
#endif
}

// Default watch paths when the user does not specify any.
std::vector<std::string> defaultPaths() {
#ifdef __APPLE__
    return {"/Volumes"};
#else
    return {"."};
#endif
}

std::string xmlEscape(const std::string& in) {
    std::string out;
    for (char c : in) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string buildPlist(const fs::path& exe, const std::vector<std::string>& paths) {
    std::string args = "    <string>" + xmlEscape(exe.string()) + "</string>\n";
    args += "    <string>watch</string>\n";
    for (const auto& p : paths) {
        std::error_code ec;
        fs::path abs = fs::absolute(p, ec);
        args += "    <string>" + xmlEscape(ec ? p : abs.string()) + "</string>\n";
    }

    std::string plist;
    plist += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n";
    plist += "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" "
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

} // namespace

int Service::install(const std::vector<std::string>& paths) {
    std::vector<std::string> watchPaths = paths.empty() ? defaultPaths() : paths;
    fs::path exe = executablePath();
    fs::path plist = plistPath();

    std::error_code ec;
    fs::create_directories(plist.parent_path(), ec);

    std::ofstream out(plist);
    if (!out) {
        std::cerr << "Error: cannot write plist to " << plist.string() << std::endl;
        return 1;
    }
    out << buildPlist(exe, watchPaths);
    out.close();
    std::cout << "Wrote service definition: " << plist.string() << std::endl;

#ifdef __APPLE__
    // Reload cleanly: unload any previous instance, then load.
    proc::run({"launchctl", "unload", plist.string()});
    int rc = proc::run({"launchctl", "load", "-w", plist.string()});
    if (rc == 0) {
        std::cout << "Service '" << kLabel << "' installed and started." << std::endl;
        std::cout << "Watching:";
        for (const auto& p : watchPaths) {
            std::cout << " " << p;
        }
        std::cout << std::endl;
        std::cout << "Logs: /tmp/" << kLabel << ".out.log / .err.log" << std::endl;
    } else {
        std::cerr << "launchctl load failed (exit " << rc << ")." << std::endl;
        return rc;
    }
    return 0;
#else
    std::cout << "Note: launchd is macOS-only. The plist above is for reference; "
                 "on this platform run `dscleaner watch` directly (e.g. under "
                 "systemd/supervisor)." << std::endl;
    return 0;
#endif
}

int Service::uninstall() {
    fs::path plist = plistPath();
    std::error_code ec;

#ifdef __APPLE__
    if (fs::exists(plist, ec)) {
        proc::run({"launchctl", "unload", "-w", plist.string()});
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
