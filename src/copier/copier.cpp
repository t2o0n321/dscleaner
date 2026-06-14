#include <copier.hpp>
#include <cleaner.hpp>
#include <junk.hpp>
#include <proc.hpp>

#include <algorithm>
#include <iostream>

namespace {

// Recursively copy s -> d, skipping any junk entries. Counts copied files and
// skipped junk items.
void copyRecursive(const fs::path& s, const fs::path& d, std::uintmax_t& copied,
                   std::uintmax_t& skipped, bool verbose) {
    if (junk::isJunk(s.filename().string())) {
        skipped++;
        if (verbose) {
            std::cout << "Skipped junk: " << s.string() << std::endl;
        }
        return;
    }

    std::error_code ec;
    if (fs::is_directory(s, ec)) {
        fs::create_directories(d, ec);
        for (const auto& child :
             fs::directory_iterator(s, fs::directory_options::skip_permission_denied, ec)) {
            copyRecursive(child.path(), d / child.path().filename(), copied, skipped, verbose);
        }
    } else {
        fs::create_directories(d.parent_path(), ec);
        fs::copy_file(s, d, fs::copy_options::overwrite_existing, ec);
        if (ec) {
            std::cerr << "Error copying " << s << ": " << ec.message() << std::endl;
        } else {
            copied++;
        }
    }
}

std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return std::tolower(c); });
    return s;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

} // namespace

int Copier::cp(const fs::path& src, const fs::path& dst, bool verbose) {
    std::error_code ec;
    if (!fs::exists(src, ec)) {
        std::cerr << "Error: source does not exist: " << src.string() << std::endl;
        return 1;
    }

    // Mirror `cp` semantics: copying into an existing directory.
    fs::path realDst = dst;
    if (fs::is_directory(dst, ec)) {
        realDst = dst / src.filename();
    }

    std::uintmax_t copied = 0;
    std::uintmax_t skipped = 0;
    copyRecursive(src, realDst, copied, skipped, verbose);

    // Belt-and-braces: sweep the destination for any junk that slipped in
    // (e.g. pre-existing junk in the target directory).
    Cleaner cleaner;
    fs::path sweepDir = fs::is_directory(realDst, ec) ? realDst : realDst.parent_path();
    int cleaned = cleaner.clean(sweepDir, true, false);

    std::cout << "Copy complete: " << copied << " file(s) copied, " << skipped
              << " junk item(s) skipped";
    if (cleaned > 0) {
        std::cout << ", " << cleaned << " junk item(s) cleaned at destination";
    }
    std::cout << ". Destination is clean." << std::endl;
    return 0;
}

int Copier::scp(const std::vector<std::string>& args, bool verbose) {
    if (args.size() < 2) {
        std::cerr << "Usage: dscleaner scp <source...> <[user@]host:dest>" << std::endl;
        return 1;
    }

    std::vector<std::string> cmd;
    bool usingRsync = proc::exists("rsync");
    if (usingRsync) {
        cmd = {"rsync", "-a", "-e", "ssh"};
        for (const auto& g : junk::globPatterns()) {
            cmd.push_back("--exclude=" + g);
        }
        if (verbose) {
            cmd.push_back("-v");
        }
    } else {
        std::cerr << "Warning: rsync not found; falling back to scp. "
                     "Junk will NOT be excluded during transfer." << std::endl;
        cmd = {"scp", "-r"};
    }
    for (const auto& a : args) {
        cmd.push_back(a);
    }

    int rc = proc::run(cmd);
    if (rc == 0) {
        if (usingRsync) {
            std::cout << "Transfer complete. macOS junk excluded; destination is clean."
                      << std::endl;
        } else {
            std::cout << "Transfer complete (junk NOT excluded - install rsync for "
                         "junk-free transfers)." << std::endl;
        }
    } else {
        std::cerr << "Transfer failed (exit code " << rc << ")." << std::endl;
    }
    return rc;
}

int Copier::pack(const fs::path& output, const std::vector<fs::path>& sources, bool verbose) {
    if (sources.empty()) {
        std::cerr << "Usage: dscleaner pack <output.{tar,tar.gz,tgz,tar.bz2,tar.xz,zip}> "
                     "<source...>" << std::endl;
        return 1;
    }

    const std::string name = toLower(output.string());
    std::vector<std::string> cmd;

    if (endsWith(name, ".zip")) {
        if (!proc::exists("zip")) {
            std::cerr << "Error: `zip` not found." << std::endl;
            return 1;
        }
        cmd = {"zip", "-r"};
        if (!verbose) {
            cmd.push_back("-q");
        }
        cmd.push_back(output.string());
        for (const auto& s : sources) {
            cmd.push_back(s.string());
        }
        // zip excludes come after the input list.
        cmd.push_back("-x");
        for (const auto& g : junk::globPatterns()) {
            cmd.push_back(g);
            cmd.push_back("*/" + g);
        }
    } else {
        // Everything else is handled by tar; pick the compression flag.
        std::string flag = "-cf";
        if (endsWith(name, ".tar.gz") || endsWith(name, ".tgz")) {
            flag = "-czf";
        } else if (endsWith(name, ".tar.bz2") || endsWith(name, ".tbz") ||
                   endsWith(name, ".tbz2")) {
            flag = "-cjf";
        } else if (endsWith(name, ".tar.xz") || endsWith(name, ".txz")) {
            flag = "-cJf";
        }

        cmd = {"tar"};
        for (const auto& g : junk::globPatterns()) {
            cmd.push_back("--exclude=" + g);
        }
        if (verbose) {
            cmd.push_back("-v");
        }
        cmd.push_back(flag);
        cmd.push_back(output.string());
        for (const auto& s : sources) {
            cmd.push_back(s.string());
        }
    }

    int rc = proc::run(cmd);
    if (rc == 0) {
        std::cout << "Archive created: " << output.string()
                  << " (macOS junk excluded)." << std::endl;
    } else {
        std::cerr << "Archiving failed (exit code " << rc << ")." << std::endl;
    }
    return rc;
}
