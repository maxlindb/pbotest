/*
 * pbotest.cpp – SDL2 + OpenGL: 1920×1080 colour‑wave background + bouncing, cycling quad
 *
 * • Starts in **1920 × 1080** fullscreen‑desktop KMS mode.
 * • PNGs tex0.png … tex9.png live in RAM; exactly **one** GL texture object
 *   is reused – overwritten every 10 frames (from frame 100) to show the next image.
 * • Quad moves like a DVD logo, bouncing off edges.
 * • Minimal console output (fatal errors only).
 *
 * Build:
 *   g++ pbotest.cpp -std=c++17 -O2 -Wall $(sdl2-config --cflags --libs) -lGL \
 *       -DSTB_IMAGE_IMPLEMENTATION -o pbotest
 * Run:
 *   SDL_VIDEODRIVER=kmsdrm sudo ./pbotest
 */

#define GL_GLEXT_PROTOTYPES
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <SDL2/SDL_opengl_glext.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <ostream>
#include <iostream>
#include <chrono>

#define STB_IMAGE_STATIC
#include "stb_image.h"






struct ImageRAM { int w, h; std::vector<unsigned char> rgba; };


static SDL_GLContext try_context(SDL_Window* win, int major, int minor, Uint32 profile)
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, profile);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    return SDL_GL_CreateContext(win);
}

// ------------------------------------------------------ SDL helpers
static SDL_GLContext create_context(SDL_Window* win)
{
    /*SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    return SDL_GL_CreateContext(win);*/
    
    SDL_GLContext ctx = nullptr;
    ctx = try_context(win, 3, 1, SDL_GL_CONTEXT_PROFILE_CORE);
    if (!ctx) {
        std::cerr << "Core 3.x context failed (" << SDL_GetError() << "), retrying 2.1 compat…\n";
        SDL_GL_ResetAttributes();
        ctx = try_context(win, 2, 1, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
        if (!ctx) {
            std::cerr << "Compat 2.1 context failed (" << SDL_GetError() << "), retrying GLES 2…\n";
            SDL_GL_ResetAttributes();
            ctx = try_context(win, 2, 0, SDL_GL_CONTEXT_PROFILE_ES);
            if (!ctx) {
                std::fprintf(stderr, "All GL context attempts failed: %s\n", SDL_GetError());
                return 0;
            }
        }
    }
    return ctx;
}


GLint implFmt, implType;

static bool init_sdl(int w, int h, SDL_Window** outWin, SDL_GLContext* outCtx)
{
    if (SDL_Init(SDL_INIT_VIDEO) < 0) { std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return false; }
    SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

    SDL_Window* win = SDL_CreateWindow("Bouncing quad – single VRAM texture", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                       w, h, SDL_WINDOW_OPENGL | SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) { std::fprintf(stderr, "SDL_CreateWindow: %s\n", SDL_GetError()); return false; }

    SDL_GLContext ctx = create_context(win);
    if (!ctx)  { std::fprintf(stderr, "SDL_GL_CreateContext: %s\n", SDL_GetError()); return false; }
    
    bool havePBO = (glGetString(GL_VERSION) &&      // 2.1 == core PBO
                strncmp((const char*)glGetString(GL_VERSION), "2.1", 3) == 0) ||
               (strstr((const char*)glGetString(GL_EXTENSIONS),
                       "GL_ARB_pixel_buffer_object") != nullptr);
    
                       
    std::cout << "SDL video driverr: "     << SDL_GetCurrentVideoDriver() << "\n";
    std::cout << "GL_VENDOR    : "        << glGetString(GL_VENDOR)   << "\n";
    std::cout << "GL_RENDERER  : "        << glGetString(GL_RENDERER) << "\n";
    std::cout << "GL_VERSION   : "        << glGetString(GL_VERSION)  << "\n";
    
    
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_FORMAT, &implFmt);
    glGetIntegerv(GL_IMPLEMENTATION_COLOR_READ_TYPE,   &implType);
    printf("native upload format = 0x%04X, type = 0x%04X\n", implFmt, implType);
    
    const GLubyte* ext = glGetString(GL_EXTENSIONS);
    std::cout << "GL_EXTENSIONS: " << ext << "\n";


    
    std::cout << "havePBO:" << havePBO << std::endl;


    SDL_GL_SetSwapInterval(1);
    *outWin = win; *outCtx = ctx; return true;
}

// ------------------------------------------------------ PNG loading
static std::vector<ImageRAM> load_images_to_ram()
{
    std::vector<ImageRAM> imgs;
    for (int i = 0; i < 10; ++i) {
        char path[32]; std::snprintf(path, sizeof(path), "tex%d.png", i);
        int w, h, ch; unsigned char* data = stbi_load(path, &w, &h, &ch, 4);
        if (!data) continue;
        imgs.push_back({w, h, std::vector<unsigned char>(data, data + w*h*4)});
        stbi_image_free(data);
        std::cout << "Loaded img " << path << std::endl;
    }
    if (imgs.empty()) std::fprintf(stderr, "Warning: no texN.png images found.\n");
    return imgs;
}


// ------------------------------------------------------ main
int main()
{
    constexpr int START_W = 1920;
    constexpr int START_H = 1080;

    SDL_Window* win = nullptr; SDL_GLContext ctx = nullptr;
    if (!init_sdl(START_W, START_H, &win, &ctx)) return EXIT_FAILURE;
    
    auto TexStorage2D = (PFNGLTEXSTORAGE2DPROC)SDL_GL_GetProcAddress("glTexStorage2D");
    auto TexStorage2DEXT = (PFNGLTEXSTORAGE2DEXTPROC)SDL_GL_GetProcAddress("glTexStorage2DEXT");

    std::vector<ImageRAM> images = load_images_to_ram();

    
    int texW = 2048;
    int texH = 2048;
    int texChannels = 4;
    
    int texDataSize = texW * texH * texChannels; 
    
    const int numPBOS = 2;
    GLuint pbos[numPBOS];    
    glGenBuffers(numPBOS,pbos);
    for(int i = 0;i < numPBOS; i++) {
      glBindBuffer(GL_PIXEL_UNPACK_BUFFER, pbos[i]);
      glBufferData(GL_PIXEL_UNPACK_BUFFER, texDataSize, nullptr, GL_STREAM_DRAW);
    }
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    int pboIndex = 0;
    

    GLuint texIDs[2];
    glGenTextures(2, texIDs);
    for(int i = 0;i < 2;i++){
        glBindTexture(GL_TEXTURE_2D, texIDs[i]);
        //glTexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2048, 2048);
        //TexStorage2D(GL_TEXTURE_2D, 1, GL_RGBA8, 2048, 2048);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 2048, 2048, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);
    }


    size_t currentIdx = SIZE_MAX; // force first upload

    // DVD‑style bouncing physics
    float quadW = START_W * 0.25f, quadH = START_H * 0.25f;
    float posX  = (START_W - quadW) * 0.5f;
    float posY  = (START_H - quadH) * 0.5f;
    float velX  = 250.0f;   // px/s – tuned for 1080p
    float velY  = 190.0f;
    
    
    unsigned char* testMemoryCopyPtr = (unsigned char*)malloc(images[0].rgba.size());
    for(size_t offset = 0; offset < images[0].rgba.size(); offset += 4096){
      testMemoryCopyPtr[offset] = 0;
    }
    
    
    
    GLuint drawingTexture = texIDs[0];
    GLuint uploadingTexture = texIDs[1];

    unsigned long frame = 0; Uint32 lastTicks = SDL_GetTicks();
    bool running = true;
    float time = 0.0f;
    float alwaysDT = 0.01666f;
    while (running) {
        SDL_Event ev; while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) running = false;
            if (ev.type == SDL_KEYDOWN && ev.key.keysym.sym == SDLK_ESCAPE) running = false;
        }

        
        time += 0.016667f;
        Uint32 now = SDL_GetTicks();
        //float dt = (now - lastTicks) * 0.001f; lastTicks = now;
        float dt = alwaysDT;

        // Background colour wave
        //float t = now * 0.001f;
        float t = time;
        float rc = 0.5f + 0.5f * std::sin(t);
        float gc = 0.5f + 0.5f * std::sin(t + 2.094395f);
        float bc = 0.5f + 0.5f * std::sin(t + 4.188790f);

        int dw, dh; SDL_GL_GetDrawableSize(win, &dw, &dh);
        glViewport(0, 0, dw, dh);
        glClearColor(rc, gc, bc, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (frame >= 100 && !images.empty()) {
            size_t newIdx = ((frame - 100) / 200) % images.size();
            if (newIdx != currentIdx) {
                const ImageRAM& img = images[newIdx];
                
                
                /*
                auto startt = std::chrono::steady_clock::now();
                
                glBindTexture(GL_TEXTURE_2D, texID);
                memcpy(testMemoryCopyPtr, img.rgba.data(), img.rgba.size());
                
       	        auto elapsedd = (std::chrono::steady_clock::now() - startt).count();
                std::cout << "Test memcpy:" << std::to_string(elapsedd/1000000) << " ms" << std::endl;
                */
                
                auto start = std::chrono::steady_clock::now();
                
                
                /*glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                //glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img.w, img.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img.rgba.data());
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.w, img.h,GL_RGBA, GL_UNSIGNED_BYTE,img.rgba.data());*/                
               	
                
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER,pbos[pboIndex]);
                //glBufferData(GL_PIXEL_UNPACK_BUFFER,texDataSize,nullptr, GL_STREAM_DRAW);
                
                void* ptr = glMapBufferRange(GL_PIXEL_UNPACK_BUFFER,0,texDataSize, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT | GL_MAP_UNSYNCHRONIZED_BIT);
                memcpy(ptr, img.rgba.data(), texDataSize);
                glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
                
                glBindTexture(GL_TEXTURE_2D, uploadingTexture);
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.w, img.h,GL_RGBA, GL_UNSIGNED_BYTE,nullptr); //null means "read from bound pbo"
                glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
                
                pboIndex++;
                if(pboIndex == numPBOS)pboIndex = 0;
                
                auto elapsed = (std::chrono::steady_clock::now() - start).count();                
                std::cout << "GPU upload:" << std::to_string(elapsed/1000000) << " ms" << std::endl;
                currentIdx = newIdx;
                
            }
            
            

            // update position
            posX += velX * dt; posY += velY * dt;
            if (posX <= 0.0f)              { posX = 0.0f;        velX =  fabsf(velX); }
            if (posX + quadW >= dw)        { posX = dw - quadW;  velX = -fabsf(velX); }
            if (posY <= 0.0f)              { posY = 0.0f;        velY =  fabsf(velY); }
            if (posY + quadH >= dh)        { posY = dh - quadH;  velY = -fabsf(velY); }

            // draw quad
            glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity(); glOrtho(0, dw, dh, 0, -1, 1);
            glMatrixMode(GL_MODELVIEW);  glPushMatrix(); glLoadIdentity();

            glEnable(GL_TEXTURE_2D); glBindTexture(GL_TEXTURE_2D, drawingTexture);
            glBegin(GL_QUADS);
                glTexCoord2f(0,0); glVertex2f(posX,         posY);
                glTexCoord2f(1,0); glVertex2f(posX+quadW,  posY);
                glTexCoord2f(1,1); glVertex2f(posX+quadW,  posY+quadH);
                glTexCoord2f(0,1); glVertex2f(posX,         posY+quadH);
            glEnd();
            glBindTexture(GL_TEXTURE_2D, 0); glDisable(GL_TEXTURE_2D);

            glMatrixMode(GL_MODELVIEW);  glPopMatrix();
            glMatrixMode(GL_PROJECTION); glPopMatrix();
        }

        SDL_GL_SwapWindow(win);
        ++frame;
    }

    //glDeleteTextures(1, texIDs);
    SDL_GL_DeleteContext(ctx); SDL_DestroyWindow(win); SDL_Quit();
    return 0;
}
