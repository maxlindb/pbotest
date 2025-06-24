/*
 * pbotest.cpp – SDL2 + OpenGL, colour‑wave background + textured quad after 100 frames
 *
 * This is a companion to minimal_sdl_kms_opengl.cpp.  The start‑up and colour
 * wave are identical, but beginning at frame 100 the program uploads a PNG
 * (tex0.png, tex1.png …) into a GL texture and renders it on a centred quad.
 *
 * We stay in a **compatibility (2.1) context** so immediate‑mode and fixed‑
 * function state are available, keeping the code short and Pi‑friendly.
 *
 * ─────────────────────────────── BUILD ───────────────────────────────
 * sudo apt update
 * sudo apt install build-essential libsdl2-dev libgl1-mesa-dev
 *
 * # We use stb_image for PNG decoding – header‑only, bundled in the source tree.
 * g++ sdl_kms_texture.cpp -std=c++17 -O2 -Wall \
 *     $(sdl2-config --cflags --libs) -lGL -DSTB_IMAGE_IMPLEMENTATION -o sdl_kms_texture
 *
 * Make sure tex0.png (or tex1.png…) is in the same directory when you run:
 * SDL_VIDEODRIVER=kmsdrm sudo ./sdl_kms_texture
 */

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cmath>
#include <cstring>

// stb_image for PNG decoding
#define STB_IMAGE_STATIC
#include "stb_image.h"

static SDL_GLContext try_context(SDL_Window* win)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    return SDL_GL_CreateContext(win);
}

static bool init_sdl_opengl(int width, int height, SDL_Window** outWindow, SDL_GLContext* outCtx)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    SDL_Window* win = SDL_CreateWindow("SDL KMSDRM OpenGL + Texture", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       width, height,
                                       SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) {
        std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_GLContext ctx = try_context(win);
    if (!ctx) {
        std::fprintf(stderr, "Could not create a compat 2.1 context: %s\n", SDL_GetError());
        return false;
    }

    SDL_GL_SetSwapInterval(1);

    std::cout << "GL_VERSION   : "        << glGetString(GL_VERSION)  << "\n";
    std::cout << "GL_RENDERER  : "        << glGetString(GL_RENDERER) << "\n";

    *outWindow = win;
    *outCtx    = ctx;
    return true;
}

static GLuint load_texture_from_png(const char* path)
{
    int w, h, chans;
    stbi_uc* pixels = stbi_load(path, &w, &h, &chans, 4); // force RGBA
    if (!pixels) {
        std::fprintf(stderr, "Failed to load %s: %s\n", path, stbi_failure_reason());
        return 0;
    }

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glBindTexture(GL_TEXTURE_2D, 0);

    stbi_image_free(pixels);
    return tex;
}

int main()
{
    SDL_Window*  window  = nullptr;
    SDL_GLContext context = nullptr;

    if (!init_sdl_opengl(640, 480, &window, &context))
        return EXIT_FAILURE;

    // Look for tex0.png, tex1.png … up to tex9.png
    char chosen[32] = "";
    for (int i = 0; i < 10; ++i) {
        std::snprintf(chosen, sizeof(chosen), "tex%d.png", i);
        FILE* fp = std::fopen(chosen, "rb");
        if (fp) { std::fclose(fp); break; }
        chosen[0] = '\0';
    }
    if (chosen[0] == '\0') {
        std::fprintf(stderr, "No texN.png found – place tex0.png next to the executable\n");
    }

    GLuint texture = chosen[0] ? load_texture_from_png(chosen) : 0;

    bool running = true;
    unsigned long frame = 0;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT)
                running = false;
            else if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE)
                running = false;
        }

        // background colour wave
        float t = SDL_GetTicks() * 0.001f;
        float r = 0.5f + 0.5f * std::sin(t);
        float g = 0.5f + 0.5f * std::sin(t + 2.094395f);
        float b = 0.5f + 0.5f * std::sin(t + 4.188790f);

        int w, h; SDL_GL_GetDrawableSize(window, &w, &h);
        glViewport(0, 0, w, h);
        glClearColor(r, g, b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (frame >= 100 && texture) {
            // Ortho so 0,0 is top‑left in window space
            glMatrixMode(GL_PROJECTION);
            glPushMatrix();
            glLoadIdentity();
            glOrtho(0, w, h, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);
            glPushMatrix();
            glLoadIdentity();

            float quadW = w * 0.5f;
            float quadH = h * 0.5f;
            float x0 = (w - quadW) * 0.5f;
            float y0 = (h - quadH) * 0.5f;
            float x1 = x0 + quadW;
            float y1 = y0 + quadH;

            glEnable(GL_TEXTURE_2D);
            glBindTexture(GL_TEXTURE_2D, texture);

            glBegin(GL_QUADS);
            glTexCoord2f(0.0f, 0.0f); glVertex2f(x0, y0);
            glTexCoord2f(1.0f, 0.0f); glVertex2f(x1, y0);
            glTexCoord2f(1.0f, 1.0f); glVertex2f(x1, y1);
            glTexCoord2f(0.0f, 1.0f); glVertex2f(x0, y1);
            glEnd();

            glBindTexture(GL_TEXTURE_2D, 0);
            glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_MODELVIEW);
            glPopMatrix();
            glMatrixMode(GL_PROJECTION);
            glPopMatrix();
        }

        SDL_GL_SwapWindow(window);
        ++frame;
    }

    if (texture) glDeleteTextures(1, &texture);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
