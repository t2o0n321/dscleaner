#pragma once

#include <string>
#include <vector>

// Central definition of what counts as macOS-generated "junk".
//
// macOS sprinkles a number of metadata files/directories onto any volume it
// touches. The two that bite the most when copying to a USB stick / non-Apple
// filesystem are:
//   - .DS_Store : per-directory Finder view settings
//   - ._<name>  : AppleDouble side-car files holding resource forks / xattrs
// but there are several more (Spotlight, Trash, fsevents, ...). Keeping the
// list in one place lets the cleaner, the copier and the watcher all agree on
// what to strip.
namespace junk {

// Returns true if a single path component (a file or directory *name*, not a
// full path) is macOS junk that should be removed / excluded.
bool isJunk(const std::string& filename);

// Glob patterns suitable for passing to external tools such as `tar
// --exclude=` and `rsync --exclude=`. These mirror isJunk() but are expressed
// as shell globs.
const std::vector<std::string>& globPatterns();

} // namespace junk
