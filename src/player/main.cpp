// Punto de entrada de MoodPlayer — runtime standalone del juego.
//
// SDL_MAIN_HANDLED igual que MoodEditor: queremos firma estandar
// int main(int, char**) sin que SDL la reemplace con su macro.

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "player/PlayerApplication.h"

#include <cstdio>
#include <exception>

int main(int /*argc*/, char** /*argv*/) {
    try {
        Mood::PlayerApplication app;
        return app.run();
    } catch (const std::exception& e) {
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
}
