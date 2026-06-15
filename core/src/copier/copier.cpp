#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cleaner.hpp>
#include <climits>
#include <copier.hpp>
#include <cstdint>
#include <iostream>
#include <junk.hpp>
#include <proc.hpp>

#if defined(__APPLE__)
#include <copyfile.h>
#endif

namespace {

// --- POSIX file-copy primitives (the cp hot path) -------------------------
//
// Cp() walks the source tree with readdir + dirent::d_type (no per-entry stat,
// no fs::path allocation) and copies through directory file descriptors using
// the *at() calls, mirroring the cleaner's design. macOS copies file data with
// fcopyfile(3); other platforms use a portable read/write loop. Symlinks are
// recreated as links (never followed), matching `cp -R` and ensuring a copy can
// never escape the source tree.

enum class FileKind : std::uint8_t { kDir, kSymlink, kRegular };

struct Child {
  std::string name;
  FileKind kind;
};

bool IsDotOrDotDot(const char* name) {
  return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

// Classifies an entry without following symlinks, preferring d_type to a stat.
FileKind ClassifyEntry(int dir_fd, const struct dirent* entry) {
#ifdef DT_DIR
  if (entry->d_type == DT_DIR) return FileKind::kDir;
  if (entry->d_type == DT_LNK) return FileKind::kSymlink;
  if (entry->d_type != DT_UNKNOWN) return FileKind::kRegular;
#endif
  struct stat st;
  if (fstatat(dir_fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) return FileKind::kRegular;
  if (S_ISDIR(st.st_mode)) return FileKind::kDir;
  if (S_ISLNK(st.st_mode)) return FileKind::kSymlink;
  return FileKind::kRegular;
}

// Copies the bytes of the open file `in` to the open file `out`.
bool CopyData(int in, int out) {
#if defined(__APPLE__)
  // fcopyfile picks the most efficient mechanism the platform offers.
  return fcopyfile(in, out, nullptr, COPYFILE_DATA) == 0;
#else
  char buffer[1 << 16];
  ssize_t n = 0;
  while ((n = read(in, buffer, sizeof(buffer))) > 0) {
    ssize_t off = 0;
    while (off < n) {
      const ssize_t w = write(out, buffer + off, static_cast<std::size_t>(n - off));
      if (w < 0) return false;
      off += w;
    }
  }
  return n == 0;  // n < 0 indicates a read error
#endif
}

// Copies a single regular file `name` between directory fds, preserving mode.
bool CopyRegularFile(int src_dir_fd, int dst_dir_fd, const char* name) {
  const int in = openat(src_dir_fd, name, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
  if (in < 0) return false;
  struct stat st;
  if (fstat(in, &st) != 0) {
    close(in);
    return false;
  }
  const int out =
      openat(dst_dir_fd, name, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, st.st_mode & 0777);
  if (out < 0) {
    close(in);
    return false;
  }
  const bool ok = CopyData(in, out);
  fchmod(out, st.st_mode & 07777);  // restore bits the umask may have cleared
  close(in);
  close(out);
  return ok;
}

// Recreates the symlink `name` (the link itself, not its target).
bool CopySymlink(int src_dir_fd, int dst_dir_fd, const char* name) {
  char target[PATH_MAX];
  const ssize_t len = readlinkat(src_dir_fd, name, target, sizeof(target) - 1);
  if (len < 0) return false;
  target[len] = '\0';
  unlinkat(dst_dir_fd, name, 0);  // replace any existing entry (best effort)
  return symlinkat(target, dst_dir_fd, name) == 0;
}

struct CopyStats {
  std::uintmax_t copied = 0;
  std::uintmax_t skipped = 0;
};

// Copies the contents of the directory open at `src_fd` into the directory open
// at `dst_fd`, skipping junk. Consumes (closes) both descriptors. `src_path` is
// maintained only for verbose "skipped" messages.
void CopyDirContents(int src_fd, int dst_fd, CopyStats& stats, bool verbose,
                     std::string& src_path) {
  DIR* dirp = fdopendir(src_fd);
  if (dirp == nullptr) {
    close(src_fd);
    close(dst_fd);
    return;
  }

  // Snapshot first (bounded to one directory's width), then act.
  std::vector<Child> children;
  const struct dirent* entry;
  while ((entry = readdir(dirp)) != nullptr) {
    if (IsDotOrDotDot(entry->d_name)) continue;
    children.push_back({entry->d_name, ClassifyEntry(src_fd, entry)});
  }

  for (const auto& child : children) {
    const char* name = child.name.c_str();
    if (junk::IsJunk(name)) {
      stats.skipped++;
      if (verbose) {
        std::cout << "Skipped junk: " << src_path << '/' << child.name << std::endl;
      }
      continue;
    }
    switch (child.kind) {
      case FileKind::kDir: {
        mkdirat(dst_fd, name, 0777);  // ignore EEXIST
        const int cs = openat(src_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        const int cd = openat(dst_fd, name, O_RDONLY | O_DIRECTORY | O_CLOEXEC);
        if (cs >= 0 && cd >= 0) {
          const std::size_t base = src_path.size();
          src_path += '/';
          src_path += child.name;
          CopyDirContents(cs, cd, stats, verbose, src_path);  // consumes cs, cd
          src_path.resize(base);
        } else {
          if (cs >= 0) close(cs);
          if (cd >= 0) close(cd);
        }
        break;
      }
      case FileKind::kSymlink:
        if (CopySymlink(src_fd, dst_fd, name)) stats.copied++;
        break;
      case FileKind::kRegular:
        if (CopyRegularFile(src_fd, dst_fd, name)) {
          stats.copied++;
        } else {
          std::cerr << "Error copying " << src_path << '/' << child.name << std::endl;
        }
        break;
    }
  }

  closedir(dirp);  // also closes src_fd
  close(dst_fd);
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
  struct stat src_st;
  if (stat(src.c_str(), &src_st) != 0) {
    std::cerr << "Error: source does not exist: " << src.string() << std::endl;
    return 1;
  }

  // Mirror `cp` semantics: copying into an existing directory.
  fs::path real_dst = dst;
  struct stat dst_st;
  if (stat(dst.c_str(), &dst_st) == 0 && S_ISDIR(dst_st.st_mode)) {
    real_dst = dst / src.filename();
  }

  CopyStats stats;
  if (S_ISDIR(src_st.st_mode)) {
    mkdir(real_dst.c_str(), 0777);  // ignore EEXIST
    const int src_fd = open(src.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    const int dst_fd = open(real_dst.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (src_fd < 0 || dst_fd < 0) {
      std::cerr << "Error: cannot open " << (src_fd < 0 ? src.string() : real_dst.string())
                << std::endl;
      if (src_fd >= 0) close(src_fd);
      if (dst_fd >= 0) close(dst_fd);
      return 1;
    }
    std::string src_path = src.string();
    while (src_path.size() > 1 && src_path.back() == '/') {
      src_path.pop_back();
    }
    CopyDirContents(src_fd, dst_fd, stats, verbose, src_path);  // consumes both fds
  } else {
    // A single file (top-level): skip it if it is itself junk.
    const std::string name = src.filename().string();
    if (junk::IsJunk(name.c_str())) {
      stats.skipped++;
    } else {
      std::error_code ec;
      fs::create_directories(real_dst.parent_path(), ec);
      const int in = open(src.c_str(), O_RDONLY | O_CLOEXEC);
      const int out = (in < 0) ? -1
                               : open(real_dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC,
                                      src_st.st_mode & 0777);
      if (in < 0 || out < 0) {
        std::cerr << "Error copying " << src.string() << std::endl;
        if (in >= 0) close(in);
        return 1;
      }
      if (CopyData(in, out)) {
        stats.copied++;
      } else {
        std::cerr << "Error copying " << src.string() << std::endl;
      }
      fchmod(out, src_st.st_mode & 07777);
      close(in);
      close(out);
    }
  }

  // Belt-and-braces: sweep the destination for any junk (e.g. pre-existing junk
  // in the target directory). Cheap now that the copy itself skipped junk.
  Cleaner cleaner;
  std::error_code ec;
  const fs::path sweep_dir = fs::is_directory(real_dst, ec) ? real_dst : real_dst.parent_path();
  const int cleaned = cleaner.Clean(sweep_dir, true, false);

  std::cout << "Copy complete: " << stats.copied << " file(s) copied, " << stats.skipped
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
