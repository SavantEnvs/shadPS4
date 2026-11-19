// Shared harness setup. shadPS4's path_util builds its user directory in a static
// initializer ($XDG_DATA_HOME/shadPS4, else ~/.local/share/shadPS4) and throws if the
// parent does not exist. This prioritised constructor runs before the default-priority
// static initializers and points it at the writable scratch dir ($TMPDIR, else /tmp), so
// the harness never depends on $HOME. Only directories are created there; the parsers
// under test do not write to it.
#pragma once
#include <cstdlib>

__attribute__((constructor(101))) static void ShadFuzzSetUserDir() {
    const char* tmp = std::getenv("TMPDIR");
    setenv("XDG_DATA_HOME", (tmp && *tmp) ? tmp : "/tmp", 1);
}
