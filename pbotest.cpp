/*
 * pbotest.cpp – SDL2 + OpenGL: colour‑wave background + **cycling** textured quad
 *
 * NEW BEHAVIOUR  ▸ All PNGs (tex0‑9.png) are decoded at startup and kept in
 *                  *system RAM only*.
 *                ▸ GPU holds **exactly one** GL texture object.  Every 10
 *                  frames (after frame 100) we re‑upload the next image into
 *                  that same object, so VRAM never contains more than one
 *                  texture at a time.
 *
 *                ▸ Still a 2.1 compatibility context → immediate‑mode works on
 *                  Raspberry Pi.
 *
 * BUILD
 *   sudo apt update && sudo apt install build-essential libsdl2-dev libgl1-mesa-dev
 *   g++ pbotest.cpp -std=c++17 -O2 -Wall $(sdl2-config --cflags --libs) -lGL \
 *       -DSTB_IMAGE_IMPLEMENTATION -o pbotest
 *
 * RUN (console)
 *   SDL_VIDEODRIVER=kmsdrm sudo ./pbotest
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <vector>
#include <string>

#define STB_IMAGE_STATIC
#include "stb_image.h"

struct ImageRAM {
    int w {0}, h {0};
    std::vector<unsigned char> rgba; // 4 bytes per pixel
};

// -------------------------------------------------- helpers
static SDL_GLContext create_context(SDL_Window* win)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    return SDL_GL_CreateContext(win);
}

static bool init_sdl(int w, int h, SDL_Window** outWin, SDL_GLContext* outCtx)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return false;
    }
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    SDL_Window* win = SDL_CreateWindow("Cycling textures – single VRAM slot", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }

    SDL_GLContext ctx = create_context(win);
    if (!ctx) { std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError()); return false; }

    SDL_GL_SetSwapInterval(1);
    *outWin = win; *outCtx = ctx;
    return true;
}

static std::vector<ImageRAM> load_images_to_ram()
{
    std::vector<ImageRAM> imgs;
    for (int i = 0; i < 10; ++i) {
        char path[32]; std::snprintf(path, sizeof(path), "tex%d.png", i);
        int w, h, ch;
        unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
        if (!data) continue; // skip missing
        ImageRAM img; img.w = w; img.h = h; img.rgba.assign(data, data + w*h*4);
        imgs.push_back(std::move(img));
        stbi_image_free(data);
        std::cout << "Loaded " << path << " (" << w << "×" << h << ") into RAM\n";
    }
    if (imgs.empty())
        std::cerr << "Warning: no texN.png images found – quad will be invisible.\n";
    else
        std::cout << "Total images in RAM: " << imgs.size() << "\n";
    return imgs;
}

// -------------------------------------------------- main
int main()
{
    SDL_Window* win = nullptr; SDL_GLContext ctx = nullptr;
    if (!init_sdl(640, 480, &win, &ctx)) return EXIT_FAILURE;

    // Load all PNGs into RAM buffers
    std::vector<ImageRAM> images = load_images_to_ram();

    // Create a single GL texture object (empty for now)
    GLuint texID = 0; glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);

    size_t currentIdx = SIZE_MAX; // invalid so we force first upload

    bool running = true; unsigned long frame = 0;
    while (running) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        // background colour wave
        float t = SDL_GetTicks() * 0.001f;
        float r = 0.5f + 0.5f * std::sin(t);
        float g = 0.5f + 0.5f * std::sin(t + 2.094395f);
        float b = 0.5f + 0.5f * std::sin(t + 4.188790f);

        int dw, dh; SDL_GL_GetDrawableSize(win, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // quad display logic
        if (frame >= 100 && !images.empty()) {
            size_t newIdx = ((frame - 100) / 10) % images.size();
            if (newIdx != currentIdx) {
                // upload new image into THE ONE texture object
                const ImageRAM& img = images[newIdx];
                glBindTexture(GL_TEXTURE_2D, texID);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
                currentIdx = newIdx;
            }

            // draw
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, dw, dh, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();

            float qw = dw * 0.5f, qh = dh * 0.5f;
            float x0 = (dw - qw) * 0.5f, y0 = (dh - qh) * 0.5f;
            float x1 = x0 + qw,        y1 = y0 + qh;

            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, texID);
            glBegin(GL_QUADS);
                glTexCoord2f(0, 0); glVertex2f(x0, y0);
                glTexCoord2f(1, 0); glVertex2f(x1, y0);
                glTexCoord2f(1, 1); glVertex2f(x1, y1);
                glTexCoord2f(0, 1); glVertex2f(x0, y1);
            glEnd();
            glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_MODELVIEW);  glPopMatrix();
            glMatrixMode(GL_PROJECTION); glPopMatrix();
        }

        SDL_GL_SwapWindow(win);
        ++frame;
    }

    glDeleteTextures(1, &texID);
    SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
