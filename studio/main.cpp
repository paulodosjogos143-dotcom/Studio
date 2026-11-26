// ROXBOX Studio - editor entry point
#include "studio.h"
#include "../engine/core/log.h"
#include <cstdio>
#include <cstdlib>
#include <string>

using namespace rb;

int main(int argc, char** argv) {
    rb::logSetLevel(rb::LogLevel::Info);
    rb::logSetFile("roxbox_studio.log");

    StudioApp app;
    EngineConfig cfg;
    cfg.mode = AppMode::Studio;
    cfg.window.title = "ROXBOX Studio";
    cfg.window.width = 1600;
    cfg.window.height = 900;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--tier" && i + 1 < argc) {
            cfg.startTier = std::atoi(argv[++i]);
            cfg.autoQuality = false;
        } else if (a == "--auto") {
            cfg.autoQuality = true;
        } else if (a == "--width" && i + 1 < argc) {
            cfg.window.width = std::atoi(argv[++i]);
        } else if (a == "--height" && i + 1 < argc) {
            cfg.window.height = std::atoi(argv[++i]);
        } else if (!a.empty() && a.find("--") != 0) {
            cfg.mapPath = a; // open a .roxmap on startup
        }
    }

    RB_LOG_I("ROXBOX Studio starting...");
    if (!app.init(cfg)) {
        RB_LOG_E("init failed");
        return 1;
    }
    app.run();
    RB_LOG_I("ROXBOX Studio exited");
    return 0;
}
