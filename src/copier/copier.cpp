#include <algorithm>
#include <cctype>
#include <cleaner.hpp>
#include <copier.hpp>
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
    std::cerr << "Usage: dscleaner pack <output.{tar,tar.gz,tgz,tar.bz2,tar.xz,zip}> "
                 "<source...>"
              << std::endl;
    return 1;
  }

  const std::string name = ToLower(output.string());
  std::vector<std::string> cmd;

  if (EndsWith(name, ".zip")) {
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
  } else {
    // Everything else is handled by tar; pick the compression flag.
    std::string flag = "-cf";
    if (EndsWith(name, ".tar.gz") || EndsWith(name, ".tgz")) {
      flag = "-czf";
    } else if (EndsWith(name, ".tar.bz2") || EndsWith(name, ".tbz") || EndsWith(name, ".tbz2")) {
      flag = "-cjf";
    } else if (EndsWith(name, ".tar.xz") || EndsWith(name, ".txz")) {
      flag = "-cJf";
    }

    cmd = {"tar"};
    for (const auto& glob : junk::GlobPatterns()) {
      cmd.push_back("--exclude=" + glob);
    }
    if (verbose) {
      cmd.push_back("-v");
    }
    cmd.push_back(flag);
    cmd.push_back(output.string());
    for (const auto& source : sources) {
      cmd.push_back(source.string());
    }
  }

  int const rc = proc::Run(cmd);
  if (rc == 0) {
    std::cout << "Archive created: " << output.string() << " (macOS junk excluded)." << std::endl;
  } else {
    std::cerr << "Archiving failed (exit code " << rc << ")." << std::endl;
  }
  return rc;
}
