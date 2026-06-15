#include <algorithm>
#include <cctype>
#include <cleaner.hpp>
#include <copier.hpp>
#include <cstdint>
#include <iostream>
#include <junk.hpp>
#include <proc.hpp>

namespace {

// Recursively copies `src` -> `dst`, skipping any junk entries. Counts copied
// files and skipped junk items.
void CopyRecursive(const fs::path& src, const fs::path& dst, std::uintmax_t& copied,
                   std::uintmax_t& skipped, bool verbose) {
  if (junk::IsJunk(src.filename().string())) {
    skipped++;
    if (verbose) {
      std::cout << "Skipped junk: " << src.string() << std::endl;
    }
    return;
  }

  std::error_code ec;
  if (fs::is_directory(src, ec)) {
    fs::create_directories(dst, ec);
    for (const auto& child :
         fs::directory_iterator(src, fs::directory_options::skip_permission_denied, ec)) {
      CopyRecursive(child.path(), dst / child.path().filename(), copied, skipped, verbose);
    }
  } else {
    fs::create_directories(dst.parent_path(), ec);
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
      std::cerr << "Error copying " << src << ": " << ec.message() << std::endl;
    } else {
      copied++;
    }
  }
}

std::string ToLower(std::string s) {
  std::transform(s.begin(), s.end(), s.begin(),
                 [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}

bool EndsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// Supported archive formats, selected by the output file's extension.
enum class Format : std::uint8_t {
  kTar,
  kTarGz,
  kTarBz2,
  kTarXz,
  kTarZst,
  kZip,
  kSevenZip,
  kRar,
  kUnknown
};

Format DetectFormat(const std::string& name) {
  if (EndsWith(name, ".tar.gz") || EndsWith(name, ".tgz")) return Format::kTarGz;
  if (EndsWith(name, ".tar.bz2") || EndsWith(name, ".tbz") || EndsWith(name, ".tbz2")) {
    return Format::kTarBz2;
  }
  if (EndsWith(name, ".tar.xz") || EndsWith(name, ".txz")) return Format::kTarXz;
  if (EndsWith(name, ".tar.zst") || EndsWith(name, ".tzst")) return Format::kTarZst;
  if (EndsWith(name, ".tar")) return Format::kTar;
  if (EndsWith(name, ".zip")) return Format::kZip;
  if (EndsWith(name, ".7z")) return Format::kSevenZip;
  if (EndsWith(name, ".rar")) return Format::kRar;
  return Format::kUnknown;
}

// Returns the first available command among `candidates`, or "" if none exist.
std::string FirstAvailable(const std::vector<std::string>& candidates) {
  for (const auto& candidate : candidates) {
    if (proc::Exists(candidate)) {
      return candidate;
    }
  }
  return "";
}

}  // namespace

int Copier::Cp(const fs::path& src, const fs::path& dst, bool verbose) {
  std::error_code ec;
  if (!fs::exists(src, ec)) {
    std::cerr << "Error: source does not exist: " << src.string() << std::endl;
    return 1;
  }

  // Mirror `cp` semantics: copying into an existing directory.
  fs::path real_dst = dst;
  if (fs::is_directory(dst, ec)) {
    real_dst = dst / src.filename();
  }

  std::uintmax_t copied = 0;
  std::uintmax_t skipped = 0;
  CopyRecursive(src, real_dst, copied, skipped, verbose);

  // Belt-and-braces: sweep the copied destination for any junk that slipped in
  // (e.g. pre-existing junk in the target directory).
  Cleaner cleaner;
  fs::path const sweep_dir = fs::is_directory(real_dst, ec) ? real_dst : real_dst.parent_path();
  int const cleaned = cleaner.Clean(sweep_dir, true, false);

  std::cout << "Copy complete: " << copied << " file(s) copied, " << skipped
            << " junk item(s) skipped";
  if (cleaned > 0) {
    std::cout << ", " << cleaned << " junk item(s) cleaned at destination";
  }
  std::cout << ". Destination is clean." << std::endl;
  return 0;
}

int Copier::Scp(const std::vector<std::string>& args, bool verbose) {
  if (args.size() < 2) {
    std::cerr << "Usage: dscleaner scp <source...> <[user@]host:dest>" << std::endl;
    return 1;
  }

  std::vector<std::string> cmd;
  const bool using_rsync = proc::Exists("rsync");
  if (using_rsync) {
    cmd = {"rsync", "-a", "-e", "ssh"};
    for (const auto& glob : junk::GlobPatterns()) {
      cmd.push_back("--exclude=" + glob);
    }
    if (verbose) {
      cmd.push_back("-v");
    }
  } else {
    std::cerr << "Warning: rsync not found; falling back to scp. "
                 "Junk will NOT be excluded during transfer."
              << std::endl;
    cmd = {"scp", "-r"};
  }
  for (const auto& arg : args) {
    cmd.push_back(arg);
  }

  int const rc = proc::Run(cmd);
  if (rc == 0) {
    if (using_rsync) {
      std::cout << "Transfer complete. macOS junk excluded; destination is clean." << std::endl;
    } else {
      std::cout << "Transfer complete (junk NOT excluded - install rsync for "
                   "junk-free transfers)."
                << std::endl;
    }
  } else {
    std::cerr << "Transfer failed (exit code " << rc << ")." << std::endl;
  }
  return rc;
}

int Copier::Pack(const fs::path& output, const std::vector<fs::path>& sources, bool verbose) {
  if (sources.empty()) {
    std::cerr << "Usage: dscleaner pack <archive> <source...>\n"
                 "Supported: .tar .tar.gz/.tgz .tar.bz2 .tar.xz .tar.zst .zip .7z .rar"
              << std::endl;
    return 1;
  }

  const Format format = DetectFormat(ToLower(output.string()));
  std::vector<std::string> cmd;

  switch (format) {
    case Format::kTar:
    case Format::kTarGz:
    case Format::kTarBz2:
    case Format::kTarXz:
    case Format::kTarZst: {
      if (!proc::Exists("tar")) {
        std::cerr << "Error: `tar` not found." << std::endl;
        return 1;
      }
      cmd = {"tar"};
      for (const auto& glob : junk::GlobPatterns()) {
        cmd.push_back("--exclude=" + glob);
      }
      // Compression option kept separate so .tar.zst (no short flag) shares the
      // same code path as the gzip/bzip2/xz variants.
      if (format == Format::kTarGz) {
        cmd.push_back("-z");
      } else if (format == Format::kTarBz2) {
        cmd.push_back("-j");
      } else if (format == Format::kTarXz) {
        cmd.push_back("-J");
      } else if (format == Format::kTarZst) {
        cmd.push_back("--zstd");
      }
      if (verbose) {
        cmd.push_back("-v");
      }
      cmd.push_back("-cf");
      cmd.push_back(output.string());
      for (const auto& source : sources) {
        cmd.push_back(source.string());
      }
      break;
    }

    case Format::kZip: {
      if (!proc::Exists("zip")) {
        std::cerr << "Error: `zip` not found." << std::endl;
        return 1;
      }
      cmd = {"zip", "-r"};
      if (!verbose) {
        cmd.push_back("-q");
      }
      cmd.push_back(output.string());
      for (const auto& source : sources) {
        cmd.push_back(source.string());
      }
      // zip exclude patterns come after the input list. Cover both top-level and
      // nested matches (e.g. ".DS_Store" and "*/.DS_Store").
      cmd.push_back("-x");
      for (const auto& glob : junk::GlobPatterns()) {
        cmd.push_back(glob);
        cmd.push_back("*/" + glob);
      }
      break;
    }

    case Format::kSevenZip: {
      const std::string tool = FirstAvailable({"7z", "7zz", "7za", "7zr"});
      if (tool.empty()) {
        std::cerr << "Error: no 7-Zip binary found (tried 7z, 7zz, 7za, 7zr)." << std::endl;
        return 1;
      }
      cmd = {tool, "a"};
      if (!verbose) {
        cmd.push_back("-bso0");  // silence the per-file listing
      }
      cmd.push_back(output.string());
      for (const auto& source : sources) {
        cmd.push_back(source.string());
      }
      // `-xr!<glob>` excludes matching names recursively.
      for (const auto& glob : junk::GlobPatterns()) {
        cmd.push_back("-xr!" + glob);
      }
      break;
    }

    case Format::kRar: {
      // Only WinRAR's `rar` can create archives; the free `unrar` is extract-only.
      if (!proc::Exists("rar")) {
        std::cerr << "Error: `rar` not found (the free `unrar` cannot create archives; "
                     "install WinRAR's `rar`)."
                  << std::endl;
        return 1;
      }
      cmd = {"rar", "a", "-r"};
      if (!verbose) {
        cmd.push_back("-idq");  // quiet: suppress copyright/progress output
      }
      cmd.push_back(output.string());
      for (const auto& source : sources) {
        cmd.push_back(source.string());
      }
      // `-x<glob>` exclusion masks apply recursively under `-r`.
      for (const auto& glob : junk::GlobPatterns()) {
        cmd.push_back("-x" + glob);
      }
      break;
    }

    case Format::kUnknown:
      std::cerr << "Error: unsupported archive format for '" << output.string()
                << "'. Supported: .tar .tar.gz/.tgz .tar.bz2 .tar.xz .tar.zst .zip .7z .rar"
                << std::endl;
      return 1;
  }

  const int rc = proc::Run(cmd);
  if (rc == 0) {
    std::cout << "Archive created: " << output.string() << " (macOS junk excluded)." << std::endl;
  } else {
    std::cerr << "Archiving failed (exit code " << rc << ")." << std::endl;
  }
  return rc;
}
