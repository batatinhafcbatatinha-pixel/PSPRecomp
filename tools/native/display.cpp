#include "display_window.hpp"
#include <SDL.h>
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <vector>

namespace {
SDL_Window *window{};
SDL_Renderer *renderer{};
SDL_Texture *texture{};
SDL_GameController *controller{};
std::mutex frame_mutex, input_mutex;
std::vector<std::byte> pixels;
std::uint32_t frame_width{}, frame_height{}, texture_width{}, texture_height{};
bool dirty{}, title_dirty{};
std::string title{"PSPRecomp"};
std::atomic<bool> closed{false}, aspect_locked{true};
vcs::HostInputState input;

void open_controller() {
    if (controller && !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller);
        controller = nullptr;
    }
    for (int i = 0; !controller && i < SDL_NumJoysticks(); ++i)
        if (SDL_IsGameController(i)) controller = SDL_GameControllerOpen(i);
}
}

bool native_ui_start() {
    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_AUDIO) != 0) {
        std::fprintf(stderr, "SDL initialization failed: %s\n", SDL_GetError());
        return false;
    }
    if (!vcs::display_window_enabled()) return true;
    window = SDL_CreateWindow("PSPRecomp", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                              960, 544, SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (window) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (window && !renderer) renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::fprintf(stderr, "Cannot create native window: %s\n", SDL_GetError());
        if (window) SDL_DestroyWindow(window);
        window = nullptr;
        SDL_Quit();
        return false;
    }
    open_controller();
    return true;
}

void native_ui_tick() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) closed = true;
        if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) closed = true;
        if (event.type == SDL_CONTROLLERDEVICEADDED || event.type == SDL_CONTROLLERDEVICEREMOVED)
            open_controller();
    }
    vcs::HostInputState next;
    if (window && (SDL_GetWindowFlags(window) & SDL_WINDOW_INPUT_FOCUS)) {
        const auto *keys = SDL_GetKeyboardState(nullptr);
        const SDL_Scancode codes[] = {SDL_SCANCODE_BACKSPACE, SDL_SCANCODE_RETURN,
            SDL_SCANCODE_UP, SDL_SCANCODE_RIGHT, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT,
            SDL_SCANCODE_Q, SDL_SCANCODE_E, SDL_SCANCODE_I, SDL_SCANCODE_L,
            SDL_SCANCODE_K, SDL_SCANCODE_J};
        const std::uint32_t masks[] = {1, 8, 16, 32, 64, 128, 256, 512, 4096, 8192, 16384, 32768};
        for (unsigned i = 0; i < 12; ++i) if (keys[codes[i]]) next.buttons |= masks[i];
        next.analog_x = static_cast<std::uint8_t>(128 + 127 * (keys[SDL_SCANCODE_D] - keys[SDL_SCANCODE_A]));
        next.analog_y = static_cast<std::uint8_t>(128 + 127 * (keys[SDL_SCANCODE_S] - keys[SDL_SCANCODE_W]));
        if (controller) {
            const SDL_GameControllerButton buttons[] = {SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
                SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_RIGHT,
                SDL_CONTROLLER_BUTTON_DPAD_DOWN, SDL_CONTROLLER_BUTTON_DPAD_LEFT,
                SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER,
                SDL_CONTROLLER_BUTTON_Y, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_X};
            for (unsigned i = 0; i < 12; ++i)
                if (SDL_GameControllerGetButton(controller, buttons[i])) next.buttons |= masks[i];
            auto axis = [](SDL_GameControllerAxis which) {
                const int value = SDL_GameControllerGetAxis(controller, which);
                return std::abs(value) < 6000 ? 128 : std::clamp((value + 32768) / 256, 0, 255);
            };
            const int x = axis(SDL_CONTROLLER_AXIS_LEFTX), y = axis(SDL_CONTROLLER_AXIS_LEFTY);
            if (x != 128) next.analog_x = static_cast<std::uint8_t>(x);
            if (y != 128) next.analog_y = static_cast<std::uint8_t>(y);
            next.camera_x = axis(SDL_CONTROLLER_AXIS_RIGHTX) - 128;
            next.camera_y = axis(SDL_CONTROLLER_AXIS_RIGHTY) - 128;
        }
    }
    { std::lock_guard lock(input_mutex); input = next; }
    if (renderer) {
        std::lock_guard lock(frame_mutex);
        if (title_dirty) { SDL_SetWindowTitle(window, title.c_str()); title_dirty = false; }
        if (dirty) {
            if (!texture || texture_width != frame_width || texture_height != frame_height) {
                SDL_DestroyTexture(texture);
                texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32,
                    SDL_TEXTUREACCESS_STREAMING, static_cast<int>(frame_width), static_cast<int>(frame_height));
                texture_width = frame_width; texture_height = frame_height;
            }
            if (texture) SDL_UpdateTexture(texture, nullptr, pixels.data(), static_cast<int>(frame_width * 4));
            dirty = false;
        }
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        if (texture) {
            int w{}, h{};
            SDL_GetRendererOutputSize(renderer, &w, &h);
            SDL_Rect dst{0, 0, w, h};
            if (aspect_locked && texture_width && texture_height) {
                const double scale = std::min(double(w) / texture_width, double(h) / texture_height);
                dst.w = static_cast<int>(texture_width * scale); dst.h = static_cast<int>(texture_height * scale);
                dst.x = (w - dst.w) / 2; dst.y = (h - dst.h) / 2;
            }
            SDL_RenderCopy(renderer, texture, nullptr, &dst);
        }
        SDL_RenderPresent(renderer);
    }
    SDL_Delay(8);
}

void native_ui_stop() {
    if (controller) SDL_GameControllerClose(controller);
    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

namespace vcs {
bool display_window_enabled() {
    const char *value = std::getenv("PSPRECOMP_WINDOW");
    return !value || *value != '0';
}
void display_window_start() {}
void display_window_set_status(const char *value) {
    std::lock_guard lock(frame_mutex); title = value ? value : "PSPRecomp"; title_dirty = true;
}
void display_window_set_aspect_lock(bool value) noexcept { aspect_locked = value; }
void display_window_present(const psprecomp::GuestMemory &memory, const FramebufferDescription &description) {
    const auto rgba = decode_framebuffer_rgba(memory, description);
    display_window_present_rgba(rgba, description.width, description.height);
}
void display_window_present_rgba(std::span<const std::byte> rgba, std::uint32_t width, std::uint32_t height) {
    if (!width || !height || width > 8192 || height > 8192 || rgba.size() < std::size_t(width) * height * 4) return;
    std::lock_guard lock(frame_mutex);
    pixels.assign(rgba.begin(), rgba.begin() + std::size_t(width) * height * 4);
    frame_width = width; frame_height = height; dirty = true;
}
DisplayWindowSurface display_window_surface() { return {}; }
HostInputState display_window_input() { std::lock_guard lock(input_mutex); return input; }
std::uint32_t display_window_buttons() { return display_window_input().buttons; }
void display_window_analog(std::uint8_t &x, std::uint8_t &y) {
    const auto state = display_window_input(); x = state.analog_x; y = state.analog_y;
}
bool display_window_close_requested() { return closed.load(); }
void display_window_shutdown() {}
}
