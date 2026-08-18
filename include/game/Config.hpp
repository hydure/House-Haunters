#ifndef CONFIGURATIONS_STORE
#define CONFIGURATIONS_STORE

#include <map>
// A global configurations map
class Config
{
public:
    int width = 720;
    int height = 480;

    // Target frame rate. Wired into GameEngine::setTargetFps in HouseHaunters::init.
    int fps = 60;

    // Open the window in borderless fullscreen (true) or as a normal
    // windowed 720x480 (false). The engine renders the scene to a fixed
    // 720x480 RenderTexture and letterbox-blits it into the window, so
    // fullscreen requires no per-screen changes. Toggle at runtime with
    // F11 or Alt+Enter. Default starts windowed; set HH_FULLSCREEN=1 in
    // the environment to start fullscreen.
    bool fullscreen = false;

    enum CHARACTER { BRO = 0, SIS = 1, DAD = 2, MOM = 3};
    // Scales the number of generated rooms; higher = more rooms / harder game.
    enum DIFFICULTY { EASY = 0, NORMAL = 1, HARD = 2 };

    // Maps controller index to player number
    std::map<int, int> player_map;
    // Maps player number to character
    std::map<int, CHARACTER> char_map; // Maps

    int num_players = 1;
    DIFFICULTY difficulty = NORMAL;

    float time_Per_Phase = 90.0;

    static const char* difficultyName(DIFFICULTY d)
    {
        switch (d) {
            case EASY:   return "EASY";
            case NORMAL: return "NORMAL";
            case HARD:   return "HARD";
        }
        return "?";
    }

    static const char* difficultyDescription(DIFFICULTY d)
    {
        switch (d) {
            case EASY:   return "60% house | 120s prep | 8s ghost pings";
            case NORMAL: return "Standard house | 90s prep | 4s ghost pings";
            case HARD:   return "150% house | 60s prep | 1.5s ghost pings";
        }
        return "";
    }
};

#endif
