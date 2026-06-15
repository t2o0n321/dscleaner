#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cleaner.hpp>
#include <iostream>
#include <junk.hpp>
#include <string>
#include <vector>

// The cleaner is the throughput-critical path: it may sweep an entire volume.
// It is implemented directly on POSIX (the project targets macOS) rather than
// std::filesystem to minimise overhead and system load:
//   - readdir() + dirent::d_type classifies entries without a stat(2) in the
//     common case (only DT_UNKNOWN / symlinks fall back to fstatat);
//   - openat()/unlinkat() operate relative to a directory fd, so no full path
//     string is rebuilt per entry and there is no PATH_MAX limit;
//   - each directory's entries are snapshotted before any deletion, so the
//     directory is never mutated while being iterated (safe and portable),
//     while memory stays bounded by a single directory's width - not the tree.
namespace {

struct Entry {
  std::string name;
  bool is_dir;
};

bool IsDotOrDotDot(const char* name) {
  return name[0] == '.' && (name[1] == '\0' || (name[1] == '.' && name[2] == '\0'));
}

// Classifies a directory entry, preferring readdir's d_type to avoid a stat.
bool EntryIsDir(int dir_fd, const struct dirent* entry) {
#ifdef DT_DIR
  if (entry->d_type == DT_DIR) {
    return true;
  }
  if (entry->d_type != DT_UNKNOWN && entry->d_type != DT_LNK) {
    return false;
  }
#endif
  // DT_UNKNOWN (some filesystems) or a symlink: stat without following, so a
  // symlink is never treated as a directory (we must not descend it).
  struct stat st;
  if (fstatat(dir_fd, entry->d_name, &st, AT_SYMLINK_NOFOLLOW) != 0) {
    return false;
  }
  return S_ISDIR(st.st_mode);
}

// Reads every child of an open directory into `out` (excluding "." / "..").
void ReadChildren(int dir_fd, DIR* dirp, std::vector<Entry>& out) {
  const struct dirent* entry;
  while ((entry = readdir(dirp)) != nullptr) {
    if (IsDotOrDotDot(entry->d_name)) {
      continue;
    }
    out.push_back({entry->d_name, EntryIsDir(dir_fd, entry)});
  }
}

// Recursively deletes the directory `name` under `parent_fd`, then the directory
// itself. Returns true on success.
bool RemoveDirTreeAt(int parent_fd, const char* name) {
  const int fd = openat(parent_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
  if (fd < 0) {
    // Not a directory after all (e.g. a symlink): remove the entry itself.
    return unlinkat(parent_fd, name, 0) == 0;
  }
  DIR* dirp = fdopendir(fd);
  if (dirp == nullptr) {
    close(fd);
    return unlinkat(parent_fd, name, AT_REMOVEDIR) == 0;
  }
  std::vector<Entry> children;
  ReadChildren(fd, dirp, children);
  for (const auto& child : children) {
    if (child.is_dir) {
      RemoveDirTreeAt(fd, child.name.c_str());
    } else {
      unlinkat(fd, child.name.c_str(), 0);
    }
  }
  closedir(dirp);  // also closes fd
  return unlinkat(parent_fd, name, AT_REMOVEDIR) == 0;
}

// Cleans junk under the open directory `dir_fd` (which this call consumes).
// `path` is maintained purely for verbose logging. Adds removed-item count.
void CleanFd(int dir_fd, bool recursive, bool verbose, std::string& path, int& count) {
  DIR* dirp = fdopendir(dir_fd);
  if (dirp == nullptr) {
    close(dir_fd);
    return;
  }
  std::vector<Entry> children;
  ReadChildren(dir_fd, dirp, children);

  // Single pass over the snapshot: delete junk, descend into non-junk dirs. The
  // directory is no longer being iterated, so mutating it here is safe.
  for (const auto& child : children) {
    const char* name = child.name.c_str();
    if (junk::IsJunk(name)) {
      const bool removed =
          child.is_dir ? RemoveDirTreeAt(dir_fd, name) : (unlinkat(dir_fd, name, 0) == 0);
      if (removed) {
        count++;
        if (verbose) {
          std::cout << "Removed: " << path << '/' << child.name << std::endl;
        }
      }
    } else if (recursive && child.is_dir) {
      const int sub = openat(dir_fd, name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      if (sub >= 0) {
        const std::size_t base = path.size();
        path += '/';
        path += child.name;
        CleanFd(sub, recursive, verbose, path, count);  // consumes sub
        path.resize(base);
      }
    }
  }
  closedir(dirp);  // also closes dir_fd
}

}  // namespace

int Cleaner::Clean(const fs::path& target_dir, bool recursive, bool verbose) {
  if (verbose) {
    if (recursive) {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string()
                << " and all its subfolders" << std::endl;
    } else {
      std::cout << "Starting cleanup in: " << fs::absolute(target_dir).string() << std::endl;
    }
  }

  // The user-supplied target may itself be a symlink, so it is allowed to be
  // followed here; symlinks *within* the tree are never followed (O_NOFOLLOW).
  const int fd = open(target_dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
  if (fd < 0) {
    if (verbose) {
      std::cerr << "Error: cannot open directory: " << target_dir.string() << std::endl;
    }
    return 0;
  }

  std::string path = target_dir.string();
  while (path.size() > 1 && path.back() == '/') {
    path.pop_back();  // avoid doubled separators in logged paths
  }

  int count = 0;
  CleanFd(fd, recursive, verbose, path, count);  // consumes fd

  if (verbose) {
    std::cout << "Removed " << count << " junk item(s)." << std::endl;
  }
  return count;
}
