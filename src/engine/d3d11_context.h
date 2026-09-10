#pragma once

// Only ever built on Windows; see the WIN32 guard on this file's source in CMakeLists.txt.
#ifdef _WIN32

#include <SDL2/SDL_video.h>

#include <string>

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

namespace sl::detail
{
    /** Owns the D3D11 device, immediate context, swap chain, and back-buffer view. */
    class D3D11Context
    {
    public:
        D3D11Context() = default;
        ~D3D11Context();

        D3D11Context(const D3D11Context &) = delete;
        D3D11Context &operator=(const D3D11Context &) = delete;

        bool initialise(SDL_Window *window, std::string &error);
        void shutdown();
        bool resize(int width, int height, std::string &error);
        void present(bool vsync);
        void clear(float red, float green, float blue, float alpha);
        void set_render_target(ID3D11RenderTargetView *render_target, int width, int height);
        void bind_backbuffer();
        bool is_valid() const;

        ID3D11Device *device() const { return device_; }
        ID3D11DeviceContext *context() const { return context_; }

    private:
        bool create_backbuffer_view(std::string &error);
        void release_backbuffer_view();

        ID3D11Device *device_ = nullptr;
        ID3D11DeviceContext *context_ = nullptr;
        IDXGISwapChain *swap_chain_ = nullptr;
        ID3D11RenderTargetView *render_target_view_ = nullptr;
        int width_ = 0;
        int height_ = 0;
    };
}

#endif // _WIN32
