#ifndef _SDL_REDERER_
#define _SDL_REDERER_

#include <SDL.h>

#include <cstdint>
#include <functional>
#include <string>

#include "Rotation.h"
#include "SoC.h"
#include "device_configuration.h"

class SdlRenderer {
   public:
    SdlRenderer(SDL_Window* window, SoC* soc, int scale, DisplayConfiguration& displayConfiguration,
                Rotation rotation);
    ~SdlRenderer();

    void Draw(bool forceRedraw);

    // Optional: called by Draw() after the screen has been drawn and before it is presented, on
    // the renderer's thread (Android: on-screen buttons). Not set: the output is unchanged.
    void SetBeforePresent(std::function<void(SDL_Renderer*)> callback);

    // Saves the device screen as last drawn (device orientation and size, before the window
    // rotation and scale) to a PNG file. With the LCD off the saved screen is white, as in the
    // window. Changes no emulator state. Returns false and sets error on failure.
    bool SaveScreenshot(const char* path, bool& lcdOn, std::string& error);

   private:
    void DrawSilkscreen();

   private:
    SDL_Window* window{nullptr};
    SDL_Renderer* renderer{nullptr};

    SDL_Texture* frameTexture{nullptr};
    SDL_Texture* intermediateTexture{nullptr};
    SDL_Texture* silkscreenTexture{nullptr};

    bool frameTextureValid{false};
    bool lcdEnabled{true};

    uint32_t flushPipelineCounter{0};

    std::function<void(SDL_Renderer*)> beforePresent;

    SoC* soc{nullptr};

    const int scale;
    Rotation rotation;
    DisplayConfiguration displayConfiguration;

   private:
    SdlRenderer();
    SdlRenderer(const SdlRenderer&);
    SdlRenderer(SdlRenderer&&);
    SdlRenderer& operator=(const SdlRenderer&);
    SdlRenderer& operator=(SdlRenderer&&);
};

#endif  // _SDL_REDERER_
