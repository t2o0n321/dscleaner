#include <cleaner.hpp>
#include <copier.hpp>
#include <service.hpp>
#include <watcher.hpp>

#include <csignal>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {

void onSignal(int) {
    Watcher::requestStop();
}

void printHelp() {
    std::cout <<
        "dscleaner - remove macOS junk (.DS_Store, ._* AppleDouble, Spotlight, ...)\n"
        "\n"
        "USAGE:\n"
        "  dscleaner [path] [-n|--no-recursive]      Clean a directory (default: .)\n"
        "  dscleaner clean [path] [-n]               Same as above, explicit form\n"
        "  dscleaner cp <src> <dst>                  Copy, excluding junk; sweep dst clean\n"
        "  dscleaner scp <src...> <[user@]host:dst>  Transfer (rsync/scp) without junk\n"
        "  dscleaner pack <archive> <src...>         Archive (tar*/zip) excluding junk\n"
        "  dscleaner watch [path...]                 Watch path(s) and auto-clean junk\n"
        "  dscleaner install-service [path...]       Register background service (launchd)\n"
        "  dscleaner uninstall-service               Remove the background service\n"
        "  dscleaner help | version\n"
        "\n"
        "ARCHIVE FORMATS (by extension): .tar .tar.gz/.tgz .tar.bz2 .tar.xz .zip\n"
        "\n"
        "EXAMPLES:\n"
        "  dscleaner /Volumes/USB                    # clean a USB drive\n"
        "  dscleaner cp ~/project /Volumes/USB       # copy to USB, junk-free\n"
        "  dscleaner pack backup.tar.gz ~/project    # tarball without .DS_Store/._*\n"
        "  dscleaner scp ~/project server:/srv/app   # rsync-over-ssh, junk excluded\n"
        "  dscleaner install-service /Volumes        # auto-clean any mounted volume\n";
}

// Parse the classic `[path] [-n|--no-recursive]` argument form.
int doClean(const std::vector<std::string>& args) {
    fs::path target = ".";
    bool recursive = true;
    bool gotPath = false;
    for (const auto& arg : args) {
        if (arg == "-n" || arg == "--no-recursive") {
            recursive = false;
        } else {
            target = arg;
            gotPath = true;
        }
    }
    (void)gotPath;

    if (!fileManager::exists(target)) {
        std::cerr << "Error: Path does not exist: " << target.string() << std::endl;
        return 1;
    }
    if (!fileManager::isDirectory(target)) {
        std::cerr << "Error: Path is not a directory: " << target.string() << std::endl;
        return 1;
    }

    Cleaner cleaner;
    cleaner.clean(target, recursive);
    return 0;
}

std::vector<fs::path> toPaths(const std::vector<std::string>& args) {
    std::vector<fs::path> paths;
    for (const auto& a : args) {
        paths.emplace_back(a);
    }
    return paths;
}

} // namespace

int main(int argc, char* argv[]) {
    std::vector<std::string> args(argv + 1, argv + argc);

    // No args: preserve original behaviour (recursive clean of cwd).
    if (args.empty()) {
        Cleaner cleaner;
        cleaner.clean(".", true);
        return 0;
    }

    const std::string cmd = args[0];
    const std::vector<std::string> rest(args.begin() + 1, args.end());

    if (cmd == "help" || cmd == "-h" || cmd == "--help") {
        printHelp();
        return 0;
    }
    if (cmd == "version" || cmd == "--version" || cmd == "-v") {
        std::cout << "dscleaner 2.0.0" << std::endl;
        return 0;
    }
    if (cmd == "clean") {
        return doClean(rest);
    }
    if (cmd == "cp") {
        if (rest.size() < 2) {
            std::cerr << "Usage: dscleaner cp <src> <dst>" << std::endl;
            return 1;
        }
        Copier copier;
        return copier.cp(rest[0], rest[1], true);
    }
    if (cmd == "scp") {
        Copier copier;
        return copier.scp(rest, true);
    }
    if (cmd == "pack") {
        if (rest.size() < 2) {
            std::cerr << "Usage: dscleaner pack <archive> <src...>" << std::endl;
            return 1;
        }
        Copier copier;
        std::vector<fs::path> sources(rest.begin() + 1, rest.end());
        return copier.pack(rest[0], sources, true);
    }
    if (cmd == "watch") {
        std::signal(SIGINT, onSignal);
        std::signal(SIGTERM, onSignal);
        std::vector<fs::path> paths = rest.empty() ? std::vector<fs::path>{"."} : toPaths(rest);
        Watcher watcher(paths, true, true);
        watcher.run();
        return 0;
    }
    if (cmd == "install-service") {
        return Service::install(rest);
    }
    if (cmd == "uninstall-service") {
        return Service::uninstall();
    }

    // Backwards compatibility: anything else is treated as a clean target/flag,
    // e.g. `dscleaner /path/to/dir -n`.
    return doClean(args);
}
