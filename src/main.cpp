#include <cleaner.hpp>
#include <copier.hpp>
#include <csignal>
#include <iostream>
#include <service.hpp>
#include <string>
#include <vector>
#include <watcher.hpp>

namespace {

void OnSignal(int /*signum*/) { Watcher::RequestStop(); }

void PrintHelp() {
  std::cout << "dscleaner - remove macOS junk (.DS_Store, ._* AppleDouble, Spotlight, ...)\n"
               "            (also available as the short alias `dc`)\n"
               "\n"
               "USAGE:\n"
               "  dscleaner [path] [-n|--no-recursive]      Clean a directory (default: .)\n"
               "  dscleaner clean [path] [-n]               Same as above, explicit form\n"
               "  dscleaner cp <src> <dst>                  Copy, excluding junk; sweep dst clean\n"
               "  dscleaner scp <src...> <[user@]host:dst>  Transfer (rsync/scp) without junk\n"
               "  dscleaner pack <archive> <src...>         Archive (tar*/zip/7z/rar) excl. junk\n"
               "  dscleaner watch [path...]                 Watch path(s); auto-watch mounts\n"
               "  dscleaner install-service [path...]       Register background service (launchd)\n"
               "  dscleaner uninstall-service               Remove the background service\n"
               "  dscleaner help | version\n"
               "\n"
               "ARCHIVE FORMATS (by extension):\n"
               "  .tar .tar.gz/.tgz .tar.bz2 .tar.xz .tar.zst .zip .7z .rar\n"
               "\n"
               "EXAMPLES:\n"
               "  dscleaner /Volumes/USB                    # clean a USB drive\n"
               "  dscleaner cp ~/project /Volumes/USB       # copy to USB, junk-free\n"
               "  dscleaner pack backup.tar.gz ~/project    # tarball without .DS_Store/._*\n"
               "  dscleaner scp ~/project server:/srv/app   # rsync-over-ssh, junk excluded\n"
               "  dscleaner install-service /Volumes        # auto-clean any mounted volume\n";
}

// Parses the classic `[path] [-n|--no-recursive]` argument form.
int DoClean(const std::vector<std::string>& args) {
  fs::path target = ".";
  bool recursive = true;
  for (const auto& arg : args) {
    if (arg == "-n" || arg == "--no-recursive") {
      recursive = false;
    } else {
      target = arg;
    }
  }

  if (!FileManager::Exists(target)) {
    std::cerr << "Error: Path does not exist: " << target.string() << std::endl;
    return 1;
  }
  if (!FileManager::IsDirectory(target)) {
    std::cerr << "Error: Path is not a directory: " << target.string() << std::endl;
    return 1;
  }

  Cleaner cleaner;
  cleaner.Clean(target, recursive);
  return 0;
}

std::vector<fs::path> ToPaths(const std::vector<std::string>& args) {
  std::vector<fs::path> paths;
  paths.reserve(args.size());
  for (const auto& arg : args) {
    paths.emplace_back(arg);
  }
  return paths;
}

}  // namespace

int main(int argc, char* argv[]) {
  const std::vector<std::string> args(argv + 1, argv + argc);

  // No args: preserve the original behaviour (recursive clean of cwd).
  if (args.empty()) {
    Cleaner cleaner;
    cleaner.Clean(".", true);
    return 0;
  }

  const std::string& cmd = args[0];
  const std::vector<std::string> rest(args.begin() + 1, args.end());

  if (cmd == "help" || cmd == "-h" || cmd == "--help") {
    PrintHelp();
    return 0;
  }
  if (cmd == "version" || cmd == "--version" || cmd == "-v") {
    std::cout << "dscleaner 2.0.0" << std::endl;
    return 0;
  }
  if (cmd == "clean") {
    return DoClean(rest);
  }
  if (cmd == "cp") {
    if (rest.size() < 2) {
      std::cerr << "Usage: dscleaner cp <src> <dst>" << std::endl;
      return 1;
    }
    Copier copier;
    return copier.Cp(rest[0], rest[1], true);
  }
  if (cmd == "scp") {
    Copier copier;
    return copier.Scp(rest, true);
  }
  if (cmd == "pack") {
    if (rest.size() < 2) {
      std::cerr << "Usage: dscleaner pack <archive> <src...>" << std::endl;
      return 1;
    }
    Copier copier;
    const std::vector<fs::path> sources = ToPaths({rest.begin() + 1, rest.end()});
    return copier.Pack(rest[0], sources, true);
  }
  if (cmd == "watch") {
    std::signal(SIGINT, OnSignal);
    std::signal(SIGTERM, OnSignal);
    std::vector<fs::path> paths = rest.empty() ? std::vector<fs::path>{"."} : ToPaths(rest);
    Watcher watcher(std::move(paths), true, true);
    watcher.Run();
    return 0;
  }
  if (cmd == "install-service") {
    return Service::Install(rest);
  }
  if (cmd == "uninstall-service") {
    return Service::Uninstall();
  }

  // Backwards compatibility: anything else is treated as a clean target/flag,
  // e.g. `dscleaner /path/to/dir -n`.
  return DoClean(args);
}
