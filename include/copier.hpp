#pragma once

#include <fileManager.hpp>
#include <string>
#include <vector>

// Junk-aware copy / transfer / archive operations.
//
// The common thread: macOS junk is excluded *during* the operation (so it is
// never written to the destination in the first place), and for local copies
// the destination is swept clean afterwards before "complete" is reported.
class Copier {
 public:
  // Local recursive copy of `src` -> `dst`, skipping junk. If `dst` is an
  // existing directory, `src` is copied *into* it. Returns 0 on success.
  int Cp(const fs::path& src, const fs::path& dst, bool verbose);

  // Remote transfer wrapper. Prefers `rsync -e ssh` with --exclude (so junk
  // never crosses the wire); falls back to `scp -r` with a warning if rsync is
  // unavailable. `args` are the user's source(s) + destination, passed straight
  // through. Returns the underlying tool's exit code.
  int Scp(const std::vector<std::string>& args, bool verbose);

  // Creates an archive (tar/tar.gz/tar.bz2/tar.xz/zip, chosen by the output
  // extension) from `sources`, excluding junk. Returns the archiver's exit
  // code.
  int Pack(const fs::path& output, const std::vector<fs::path>& sources, bool verbose);
};
