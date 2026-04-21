// Punto de entrada de MoodEditor.
//
// Nota: SDL_MAIN_HANDLED evita que SDL reemplace main() con su propia macro;
// queremos firma estandar int main(int, char**) y controlar nosotros el flujo.

#define SDL_MAIN_HANDLED
#include <SDL.h>

#include "core/Log.h"
#include "editor/EditorApplication.h"

#include <cstdio>
#include <exception>

int main(int /*argc*/, char** /*argv*/) {
    try {
        Mood::EditorApplication app;
        return app.run();
    } catch (const std::exception& e) {
        // Si el logger no pudo inicializarse, caemos a stderr.
        std::fprintf(stderr, "Fatal: %s\n", e.what());
        return 1;
    }
}
