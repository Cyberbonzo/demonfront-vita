#pragma once
#include <SDL.h>

namespace hd {
bool init(SDL_Window* window);
void render();
void present(SDL_Window* window);
void close();
}
