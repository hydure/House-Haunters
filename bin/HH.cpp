#include <cstdlib>     // _putenv_s / setenv
#include <cstring>
#include <iostream>
#include <string>
#include "HouseHaunters.hpp"
////////////////////////////
// House Haunters entry point. The real setup lives in HouseHauntersGame::init().
//
// Recognized CLI flags:
//   --fullscreen / -f   start in borderless-fullscreen mode (equivalent
//                       to exporting HH_FULLSCREEN=1). F11 or Alt+Enter
//                       toggles at runtime regardless of how it started.
//   --windowed / -w     force windowed mode even if HH_FULLSCREEN is set.
//   --help    / -h      print this list and exit.
//
// Next check out include/HouseHaunters.hpp
///////////////////////////
namespace {
// Tiny portability shim: _putenv_s on Windows, setenv elsewhere. Used
// instead of touching HouseHauntersGame because Config is built inside
// init(); routing through HH_FULLSCREEN keeps the CLI flag and the env
// var on the exact same code path (GameEngine::start() reads it).
void setEnvVar(const char* key, const char* value)
{
#if defined(_WIN32)
    _putenv_s(key, value);
#else
    setenv(key, value, 1);
#endif
}
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--fullscreen" || a == "-f") {
            setEnvVar("HH_FULLSCREEN", "1");
        }
        else if (a == "--windowed" || a == "-w") {
            setEnvVar("HH_FULLSCREEN", "0");
        }
        else if (a == "--help" || a == "-h" || a == "/?") {
            std::cout <<
                "House Haunters\n"
                "  --fullscreen, -f   start in borderless fullscreen\n"
                "  --windowed,   -w   start windowed (overrides HH_FULLSCREEN)\n"
                "  --help,       -h   show this message and exit\n"
                "Runtime toggles: F11 or Alt+Enter switch between\n"
                "fullscreen and windowed at any time.\n";
            return 0;
        }
    }
    HouseHauntersGame game;
    game.start();
    return 0;
}
