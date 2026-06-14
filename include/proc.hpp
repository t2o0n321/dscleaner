#pragma once

#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

// Tiny POSIX process helper shared by the copier and the service installer.
// We deliberately avoid std::system() so that user-supplied paths are passed as
// argv elements rather than being re-parsed by a shell (no quoting / injection
// surprises).
namespace proc {

// Run a program (searched on PATH) with the given argv. Returns the child's
// exit code, or -1 on failure to spawn.
inline int run(const std::vector<std::string>& args) {
    if (args.empty()) {
        return -1;
    }

    std::vector<char*> argv;
    argv.reserve(args.size() + 1);
    for (const auto& a : args) {
        argv.push_back(const_cast<char*>(a.c_str()));
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid < 0) {
        return -1;
    }
    if (pid == 0) {
        execvp(argv[0], argv.data());
        _exit(127); // exec failed
    }

    int status = 0;
    while (waitpid(pid, &status, 0) < 0) {
        // retry on EINTR
    }
    if (WIFEXITED(status)) {
        return WEXITSTATUS(status);
    }
    return -1;
}

// Returns true if a command is available on PATH. `name` is always a fixed
// literal in our usage, so the shell interpolation here is safe.
inline bool exists(const std::string& name) {
    return run({"sh", "-c", "command -v " + name + " >/dev/null 2>&1"}) == 0;
}

} // namespace proc
