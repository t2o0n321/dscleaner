#pragma once

#include <sys/wait.h>
#include <unistd.h>

#include <string>
#include <vector>

// Tiny POSIX process helper shared by the copier and the service installer.
// We deliberately avoid std::system() so that user-supplied paths are passed as
// argv elements rather than being re-parsed by a shell (no quoting / injection
// surprises).
namespace proc {

// Runs a program (searched on PATH) with the given argv. Returns the child's
// exit code, or -1 on failure to spawn / abnormal termination.
inline int Run(const std::vector<std::string>& args) {
  if (args.empty()) {
    return -1;
  }

  std::vector<char*> argv;
  argv.reserve(args.size() + 1);
  for (const auto& arg : args) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  const pid_t pid = fork();
  if (pid < 0) {
    return -1;
  }
  if (pid == 0) {
    execvp(argv[0], argv.data());
    _exit(127);  // exec failed
  }

  int status = 0;
  while (waitpid(pid, &status, 0) < 0) {
    // Retry on EINTR.
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return -1;
}

// Returns true if a command is available on PATH. `name` is always a fixed
// literal in our usage, so the shell interpolation here is safe.
inline bool Exists(const std::string& name) {
  return Run({"sh", "-c", "command -v " + name + " >/dev/null 2>&1"}) == 0;
}

}  // namespace proc
